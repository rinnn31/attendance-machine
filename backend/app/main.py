from fastapi import FastAPI
from contextlib import asynccontextmanager

from app.core.database import Database

@asynccontextmanager
async def lifespan(app: FastAPI):
    database = Database()
    await database.init_db()

    app.state.database = database

    try:
        yield
    finally:
        await database.close()


if __name__ == "__main__":
    app = FastAPI(lifespan=lifespan)
    app.state.data