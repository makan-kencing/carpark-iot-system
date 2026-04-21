import io
import threading
from datetime import datetime
from decimal import Decimal
from typing import Annotated, Iterable

from dependency_injector.wiring import inject, Provide
from fastapi import APIRouter, Request, Depends, Form, HTTPException
from fastapi.sse import EventSourceResponse, ServerSentEvent
from fastapi.templating import Jinja2Templates
from sqlalchemy import event, Connection, select, update
from sqlalchemy.orm import Mapper
from starlette.responses import HTMLResponse, StreamingResponse
from starlette.staticfiles import StaticFiles
from starlette.status import HTTP_200_OK, HTTP_404_NOT_FOUND, HTTP_418_IM_A_TEAPOT

from carpark_iot_api.containers import ApplicationContainer
from carpark_iot_core.core import Carpark
from carpark_iot_core.components.models import SmartGate, SmartParkingSpace
from carpark_iot_core.db.database import Database
from carpark_iot_core.db.models import Entry as DBEntry, Wallet as DBWallet

router = APIRouter()
router.mount("/static", StaticFiles(directory="static"), name="static")

templates = Jinja2Templates(directory="templates")

MESSAGE_STREAM_DELAY_MS = 1000
MESSAGE_STREAM_RETRY_TIMEOUT_MS = 15000


class State:
    def __init__(self):
        self.entry: DBEntry | None = None
        self.condition = threading.Condition()

    def notify(self, entry: DBEntry) -> None:
        with self.condition:
            self.entry = entry
            self.condition.notify_all()


class StreamingOutput(io.BufferedIOBase):
    def __init__(self):
        self.frame: bytes | None = None
        self.condition = threading.Condition()

    def notify(self, buf: bytes) -> None:
        with self.condition:
            self.frame = buf
            self.condition.notify_all()

    def write(self, buf: bytes):
        self.notify(buf)


class MJpegStreamingResponse(StreamingResponse):
    media_type = "multipart/x-mixed-replace; boundary=frame"


state = State()
output = StreamingOutput()


@event.listens_for(DBEntry, "after_insert", named=True)
def on_new_entry(mapper: Mapper[DBEntry], connection: Connection, target: DBEntry) -> None:
    state.notify(target)


def get_latest_entry(request: Request) -> Iterable[DBEntry]:
    while True:
        with state.condition:
            state.condition.wait()

        if state.entry is not None:
            yield state.entry


def get_camera_frame() -> Iterable[bytes]:
    while True:
        with output.condition:
            output.condition.wait()

        assert output.frame is not None
        yield b"--frame\r\n" \
              b"Content-Type: image/jpeg\r\n" \
              b"Content-Length: " + str(len(output.frame)).encode() + b"\r\n" \
            + b"\r\n" \
            + output.frame + b"\r\n"


@router.get("/", response_class=HTMLResponse)
async def index(request: Request):
    return templates.TemplateResponse(request=request, name="index.html")


@router.get("/hx-entry", response_class=HTMLResponse)
@inject
def get_entries(
        request: Request,
        db: Annotated[Database, Depends(Provide[ApplicationContainer.db])],
        cursor: datetime | None = None,
        count: int = 20
):
    with db.session as session:
        stmt = select(DBEntry) \
            .order_by(DBEntry.timestamp.desc()) \
            .limit(count)
        if cursor is not None:
            stmt = stmt.where(DBEntry.timestamp < cursor)
        results = session.scalars(stmt).all()

        return templates.TemplateResponse(request, name="_entries.html", context={
            "entries": results
        })


@router.get("/hx-component", response_class=HTMLResponse)
@inject
def get_components(
        request: Request,
        carpark: Annotated[Carpark, Depends(Provide[ApplicationContainer.carpark])]
):
    contents = []
    for component_id, component in carpark.mqtt_components.items():
        if isinstance(component, SmartGate):
            contents.append(templates.TemplateResponse(request, name="_smart_gate_component.html", context={
                "id": component_id,
                "component": component,
                "type": "EXIT" if component_id == carpark._exit_gate_id else "ENTRY"
            }))
        elif isinstance(component, SmartParkingSpace):
            contents.append(templates.TemplateResponse(request, name="_smart_parking_space_component.html", context={
                "id": component_id,
                "component": component
            }))
    return HTMLResponse(content="".join(content.body.decode() for content in contents), status_code=HTTP_200_OK)

@router.post("/hx-component/gate/{gate_id}/state/{state}", response_class=HTMLResponse)
@inject
def set_gate_state(
        request: Request,
        carpark: Annotated[Carpark, Depends(Provide[ApplicationContainer.carpark])],
        gate_id: str,
        state: str
):
    component = carpark.mqtt_components.get(gate_id)
    if component is None:
        raise HTTPException(status_code=HTTP_404_NOT_FOUND)

    if not isinstance(component, SmartGate):
        raise HTTPException(status_code=HTTP_418_IM_A_TEAPOT, detail=f"The component is not a {SmartGate.__name__}")

    if state == "ON":
        component.open()
    elif state == "OFF":
        component.close()

    return templates.TemplateResponse(request, name="_set_gate_state.html", headers={
        "HX-Trigger": "update-components"
    })


@router.get("/hx-wallet", response_class=HTMLResponse)
@inject
def get_wallets(
        request: Request,
        db: Annotated[Database, Depends(Provide[ApplicationContainer.db])],
):
    with db.session as session:
        stmt = select(DBWallet)
        results = session.scalars(stmt).all()

        return templates.TemplateResponse(request, name="_wallets.html", context={
            "wallets": results
        })


@router.post("/hx-wallet/deposit", response_class=HTMLResponse)
@inject
def deposit_money(
        request: Request,
        db: Annotated[Database, Depends(Provide[ApplicationContainer.db])],
        nfc_id: Annotated[list[str], Form()],
        amount: Annotated[Decimal, Form()]
):
    nfc_id: bytes = bytes([int(bit) for bit in nfc_id if bit != ""])
    with db.session as session:
        stmt = update(DBWallet) \
            .where(DBWallet.nfc == nfc_id) \
            .values(balance=DBWallet.balance + amount)
        result = session.execute(stmt)
        if not result.rowcount:  # noqa
            wallet = DBWallet(nfc=nfc_id, balance=amount)
            session.add(wallet)
        session.commit()

        return templates.TemplateResponse(request, name="_deposit_money.html", headers={
            "HX-Trigger": "update-wallet"
        })


@router.get("/hx-entry/stream", response_class=EventSourceResponse)
def get_entries_stream(request: Request):
    for entry in get_latest_entry(request):
        content = templates.TemplateResponse(request, name="_entries.html", context={
            "entries": [entry]
        })
        yield ServerSentEvent(raw_data=content.body.replace(b"\n", b""), event="entryupdate")


@router.get("/camera/live.mjpg", response_class=MJpegStreamingResponse)
def get_camera_stream():
    return MJpegStreamingResponse(get_camera_frame())


__all__ = (
    "router",
    "output"
)
