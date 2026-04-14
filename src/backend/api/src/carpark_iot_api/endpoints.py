import asyncio
from datetime import datetime
from typing import Annotated, AsyncIterable

from dependency_injector.wiring import inject, Provide
from fastapi import APIRouter, Request
from fastapi.params import Depends
from fastapi.sse import EventSourceResponse
from fastapi.templating import Jinja2Templates
from sqlalchemy import event, Connection, select
from sqlalchemy.orm import Mapper
from starlette.responses import HTMLResponse
from starlette.staticfiles import StaticFiles

from carpark_iot_api.containers import ApplicationContainer
from carpark_iot_api.schemas import Entry, EntryCursor
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

    @event.listens_for(DBEntry, "after_insert", named=True)
    def on_new_entry(self, mapper: Mapper[DBEntry], connection: Connection, target: DBEntry) -> None:
        self.entry = target
        self.condition.notify_all()


state = State()


@router.get("/", response_class=HTMLResponse)
@inject
async def index(request: Request):
    return templates.TemplateResponse(request=request, name="index.html.j2")


@router.get("/entry")
@inject
async def get_entries(
        db: Annotated[AsyncDatabase,
        Depends(Provide[ApplicationContainer.db])],
        cursor: datetime,
        count: int
) -> EntryCursor:
    async with db.session as session:
        stmt = select(DBEntry) \
            .where(DBEntry.timestamp < cursor) \
            .order_by(DBEntry.timestamp.desc()) \
            .limit(count)
        result = tuple(map(lambda r: Entry.model_validate(r), (await session.execute(stmt)).all()))

        return EntryCursor(data=result, count=len(result), cursor=result[-1].timestamp)


@router.get("/entry/stream", response_class=EventSourceResponse)
async def get_entries_stream(request: Request) -> AsyncIterable[Entry]:
    while not await request.is_disconnected():
        async with state.condition:
            await state.condition.wait()

        if state.entry is not None:
            yield state.entry


__all__ = (
    "router",
)
