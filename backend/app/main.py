from fastapi import FastAPI
from contextlib import asynccontextmanager

from app.core.database import Database
from app.device_protocol import DeviceManager
from app.api import employee_router, biometric_router, attendance_router
from app.device_protocol import device_protocol_router
from app.recognition import FaceRecognition, FingerprintRecognition
from app.service.biometric_service import BiometricService

@asynccontextmanager
async def lifespan(app: FastAPI):
    database = Database()
    await database.init_db()

    app.state.database = database
    app.state.device_manager = DeviceManager()
    app.state.face_recognition = FaceRecognition()
    app.state.fingerprint_recognition = FingerprintRecognition()

    db_session = database.get_session()
    biometric_service = BiometricService(db_session, app.state.face_recognition, app.state.fingerprint_recognition)
    try:
        biometric_data = await biometric_service.get_all_biometric_data()
        for biometric in biometric_data:
            if biometric.type == "face":
                app.state.face_recognition.add_face_embedding(str(biometric.employee_id), biometric.data)
            elif biometric.type == "fingerprint":
                app.state.fingerprint_recognition.add_fingerprint_template(str(biometric.employee_id), biometric.data)
    finally:
        await db_session.close()
        
    try:
        yield
    finally:
        await database.close()


if __name__ == "__main__":
    app = FastAPI(lifespan=lifespan)
    app.include_router(employee_router, prefix="/api")
    app.include_router(biometric_router, prefix="/api")
    app.include_router(attendance_router, prefix="/api")
    app.include_router(device_protocol_router)