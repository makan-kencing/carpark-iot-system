from datetime import datetime
from decimal import Decimal
from typing import Sequence

from pydantic import BaseModel, ConfigDict

from carpark_iot_core.db.models import Entry as DBEntry


class Entry(BaseModel):
    timestamp: datetime
    license_plate: str
    gate_id: str
    type: DBEntry.EntryType
    price: Decimal | None

    model_config = ConfigDict(from_attributes=True)


class EntryCursor(BaseModel):
    data: Sequence[Entry]
    count: int
    cursor: datetime