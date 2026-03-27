from pydantic import BaseModel
from typing_extensions import Literal

type State = Literal["ON", "OFF"]


class PermitJoinRequest(BaseModel):
    time: int
    device: str | None = None


class SmartParkingSpacePayload(BaseModel):
    total: int | None = None
    available: int | None = None


class SmartGatePayload(BaseModel):
    nfc_data: bytes | None = None
    screen_text: str | None = None
    gate: State | None = None


__all__ = (
    "PermitJoinRequest",
    "SmartParkingSpacePayload",
    "SmartGatePayload"
)
