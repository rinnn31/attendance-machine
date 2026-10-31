from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from contextlib import asynccontextmanager
from fastapi.staticfiles import StaticFiles
import numpy as np
import os

from app.core.database import Database
from app.device_protocol import DeviceManager
from app.api import employee_router, biometric_router, attendance_router, device_router
from app.device_protocol import device_protocol_router
from app.models.biometric import DBBiometricType
from app.recognition import FaceRecognition, FingerprintRecognition
from app.service.biometric_service import BiometricService
from app.core.config import get_settings    

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
            data = biometric.data
            if biometric.type == DBBiometricType.FACE:
                app.state.face_recognition.add_face_embedding(biometric.employee_id, np.frombuffer(data, dtype=np.float32))
            elif biometric.type == DBBiometricType.FINGERPRINT:
                app.state.fingerprint_recognition.add_fingerprint_template(biometric.employee_id, data)
    finally:
        await db_session.close()
        
    try:
        yield
    finally:
        await database.close()

app = FastAPI(lifespan=lifespan)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"]
)

app.include_router(employee_router, prefix="/api")
app.include_router(biometric_router, prefix="/api")
app.include_router(attendance_router, prefix="/api")
app.include_router(device_router, prefix="/api")
app.include_router(device_protocol_router)

os.makedirs(get_settings().FACE_CAPTURE_IMAGE_PATH, exist_ok=True)
app.mount("/files", StaticFiles(directory=get_settings().FACE_CAPTURE_IMAGE_PATH), name="files")