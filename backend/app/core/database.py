from sqlalchemy.ext.asyncio import create_async_engine, async_sessionmaker, AsyncSession

from app.core import get_settings
from app.models import Base

class Database:
    def __init__(self):
        settings = get_settings()

        self._engine = create_async_engine(settings.DATABASE_URL, echo=True)
        self._async_session = async_sessionmaker(self._engine, expire_on_commit=False, class_=AsyncSession)

    def get_session(self) -> AsyncSession:
        return self._async_session()

    async def close(self):
        await self._engine.dispose()\

    async def init_db(self):
        async with self._engine.begin() as conn:
            await conn.run_sync(Base.metadata.create_all)
