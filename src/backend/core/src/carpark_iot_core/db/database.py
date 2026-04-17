import logging

from sqlalchemy import create_engine
from sqlalchemy.orm import scoped_session, sessionmaker, Session

from carpark_iot_core.db.models import Base

logger = logging.getLogger(__name__)


class Database:
    def __init__(self, db_url: str, **kwargs) -> None:
        self.engine = create_engine(db_url, **kwargs)
        self._session_factory = scoped_session(
            sessionmaker(
                self.engine,
                autoflush=False
            )
        )

    def create_database(self) -> None:
        with self.engine.begin() as conn:  # noqa
            Base.metadata.create_all(conn)

    @property
    def session(self) -> Session:
        return self._session_factory()


__all__ = (
    "Database",
)
