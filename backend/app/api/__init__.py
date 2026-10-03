from app.api.employee import router as employee_router
from app.api.biometric import router as biometric_router
from app.api.attendance import router as attendance_router
from app.api.device import router as device_protocol_router

__all__ = [
    "employee_router",
    "biometric_router",
    "attendance_router",
    "device_protocol_router",
]