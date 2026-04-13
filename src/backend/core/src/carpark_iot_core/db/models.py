from datetime import datetime
from decimal import Decimal
from enum import Enum, auto

from sqlalchemy import String, DateTime, Numeric, Enum as SAEnum
from sqlalchemy.orm import DeclarativeBase, mapped_column, Mapped


class Base(DeclarativeBase):
    pass


class Entry(Base):
    class EntryType(Enum):
        Entry = auto()
        Exit = auto()

    __tablename__ = "entry"

    timestamp: Mapped[datetime] = mapped_column(DateTime(), default=datetime.now, primary_key=True)
    license_plate: Mapped[str] = mapped_column(String(20))
    gate_id: Mapped[str] = mapped_column(String(20))
    type: Mapped[EntryType] = mapped_column(SAEnum(EntryType))
    price: Mapped[Decimal | None] = mapped_column(Numeric(7, 2), nullable=True)

    __table_args__ = ({
        "timescaledb_hypertable": {
            "time_column_name": "timestamp"
        }
    })


__all__ = (
    "Base",
    "Entry"
)
