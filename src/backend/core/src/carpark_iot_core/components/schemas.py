from pydantic import BaseModel
from typing import Literal, TYPE_CHECKING

if TYPE_CHECKING:
    from carpark_iot_core.components.models import SmartParkingSpace

type State = Literal["ON", "OFF"]


class PermitJoinRequest(BaseModel):
    time: int
    device: str | None = None


class SmartParkingSpaceInput(BaseModel):
    total: int
    remaining: int

    def update(self, o: "SmartParkingSpace") -> None:
        o.total = self.total
        o.remaining = self.remaining

class SmartParkingSpaceOutput(BaseModel):
    total: int | None = None
    remaining: int | None = None


class SmartGateInput(BaseModel):
    gate: State
    display_text: str | None
    nfc_data: str | None


class SmartGateOutput(BaseModel):
    gate: State | None = None
    display_text: str | None = None
    nfc_data: str | None = None
    clear_display: bool | None = None
    clear_nfc: bool | None = None


__all__ = (
    "PermitJoinRequest",
    "SmartGateInput",
    "SmartGateOutput",
    "SmartParkingSpaceInput",
    "SmartParkingSpaceOutput",
)
