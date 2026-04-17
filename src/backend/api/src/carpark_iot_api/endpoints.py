import io
from datetime import datetime
from typing import Annotated

import asyncio

from dependency_injector.wiring import inject, Provide
from fastapi import APIRouter, Request
from fastapi.params import Depends
from fastapi.sse import EventSourceResponse
from fastapi.templating import Jinja2Templates
from sqlalchemy import event, Connection, select
from sqlalchemy.orm import Mapper
from starlette.responses import HTMLResponse, StreamingResponse
from starlette.staticfiles import StaticFiles

from carpark_iot_api.containers import ApplicationContainer
from carpark_iot_core.db.database import AsyncDatabase
from carpark_iot_core.db.models import Entry as DBEntry

router = APIRouter()
router.mount("/static", StaticFiles(directory="static"), name="static")

templates = Jinja2Templates(directory="templates")

MESSAGE_STREAM_DELAY_MS = 1000
MESSAGE_STREAM_RETRY_TIMEOUT_MS = 15000


class State:
    def __init__(self):
        self.entry: DBEntry | None = None
        self.condition = asyncio.Condition()

    async def notify(self, entry: DBEntry) -> None:
        async with self.condition:
            self.entry = entry
            self.condition.notify_all()

    @event.listens_for(DBEntry, "after_insert", named=True)
    def on_new_entry(self, mapper: Mapper[DBEntry], connection: Connection, target: DBEntry) -> None:
        asyncio.run(self.notify(target))


class StreamingOutput(io.BufferedIOBase):
    def __init__(self):
        self.frame: bytes | None = None
        self.condition = asyncio.Condition()

    async def notify(self, buf: bytes) -> None:
        async with self.condition:
            self.frame = buf
            self.condition.notify_all()
            await self.condition.wait()

    def write(self, buf: bytes):
        asyncio.run(self.notify(buf))


class MJpegStreamingResponse(StreamingResponse):
    media_type = "multipart/x-mixed-replace; boundary=frame"


state = State()
output = StreamingOutput()


@router.get("/", response_class=HTMLResponse)
@inject
async def index(request: Request):
    return templates.TemplateResponse(request=request, name="index.html")


@router.get("/hx-entry", response_class=HTMLResponse)
async def get_entries(
        request: Request,
        db: Annotated[AsyncDatabase, Depends(Provide[ApplicationContainer.db])],
        cursor: datetime,
        count: int = 20
):
    async with db.session as session:
        stmt = select(DBEntry) \
            .where(DBEntry.timestamp < cursor) \
            .order_by(DBEntry.timestamp.desc()) \
            .limit(count)
        results = (await session.scalars(stmt)).all()

        return templates.TemplateResponse(request, name="_entries.html", context={
            "entries": results
        })


@router.get("/hx-entry/stream", response_class=EventSourceResponse)
async def get_entries_stream(request: Request):
    while True:
        async with state.condition:
            await state.condition.wait()

            if state.entry is not None:
                yield templates.TemplateResponse(request, name="_entry.html", context={
                    "entry": state.entry
                })


@router.get("/camera/live.mjpg", response_class=MJpegStreamingResponse)
async def get_camera_stream():
    while True:
        async with output.condition:
            await output.condition.wait()

            assert output.frame is not None
            yield b"--frame\r\n" \
                  b"Content-Type: image/jpeg\r\n" \
                  b"Content-Length: " + str(len(output.frame)).encode() + b"\r\n" \
                + output.frame + b"\r\n"


__all__ = (
    "router",
    "output"
)
