from app.api.employee import router as employee_router
from app.api.biometric import router as biometric_router

__all__ = [
    "employee_router",
    "biometric_router"
]