import logging
from asyncio import current_task

from sqlalchemy.ext.asyncio import create_async_engine, async_scoped_session, async_sessionmaker, AsyncSession

from carpark_iot_core.db.models import Base

logger = logging.getLogger(__name__)


class AsyncDatabase:
    def __init__(self, db_url: str, **kwargs) -> None:
        self.engine = create_async_engine(db_url, **kwargs)
        self._session_factory = async_scoped_session(
            async_sessionmaker(
                self.engine,
                autoflush=False
            ),
            scopefunc=current_task,
        )

    async def create_database(self) -> None:
        async with self.engine.begin() as conn:  # noqa
            await conn.run_sync(Base.metadata.create_all)

    @property
    def session(self) -> AsyncSession:
        return self._session_factory()


__all__ = (
    "AsyncDatabase",
)
