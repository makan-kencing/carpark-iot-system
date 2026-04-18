from __future__ import annotations

from datetime import datetime
from decimal import Decimal
from enum import Enum, auto

from sqlalchemy import String, LargeBinary, DateTime, Numeric, ForeignKey, Enum as SAEnum
from sqlalchemy.orm import DeclarativeBase, mapped_column, Mapped, relationship


class Base(DeclarativeBase):
    pass


class Wallet(Base):
    __tablename__ = "wallet"

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    nfc: Mapped[bytes] = mapped_column(LargeBinary(10), unique=True)
    balance: Mapped[Decimal] = mapped_column(Numeric(7, 2), default=Decimal(0))

    entries: Mapped[set[Entry]] = relationship(back_populates="wallet")


class Entry(Base):
    class EntryType(Enum):
        Entry = auto()
        Exit = auto()

    __tablename__ = "entry"

    timestamp: Mapped[datetime] = mapped_column(DateTime(), default=datetime.now, primary_key=True)
    license_plate: Mapped[str] = mapped_column(String(20))
    type: Mapped[EntryType] = mapped_column(SAEnum(EntryType))
    gate_id: Mapped[str] = mapped_column(String(20))
    wallet_id: Mapped[int | None] = mapped_column(ForeignKey("wallet.id"), nullable=True)
    price: Mapped[Decimal | None] = mapped_column(Numeric(7, 2), nullable=True)

    wallet: Mapped[Wallet] = relationship(back_populates="entries")

    __table_args__ = (
        {"timescaledb_hypertable": {"time_column_name": "timestamp"}}
    )


__all__ = (
    "Base",
    "Wallet",
    "Entry",
)
