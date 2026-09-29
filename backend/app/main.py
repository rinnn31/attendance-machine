from fastapi import FastAPI
from contextlib import asynccontextmanager

from app.core.database import Database
from app.device_protocol import DeviceManager
from app.api import employee_router, biometric_router
from app.device_protocol import device_protocol_router

@asynccontextmanager
async def lifespan(app: FastAPI):
    database = Database()
    await database.init_db()

    app.state.database = database
    app.state.device_manager = DeviceManager()

    try:
        yield
    finally:
        await database.close()


if __name__ == "__main__":
    app = FastAPI(lifespan=lifespan)
    app.include_router(employee_router, prefix="/api")
    app.include_router(biometric_router, prefix="/api")
    app.include_router(device_protocol_router)