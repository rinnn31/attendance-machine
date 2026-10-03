from pydantic import BaseModel, Field
from typing import Optional

class Response[T](BaseModel):
    success: bool
    message: Optional[str] = None
    data: Optional[T] = None

    @classmethod
    def success(cls, data: Optional[T] = None) -> "Response[T]":
        return cls(success=True, data=data)

    @classmethod
    def error(cls, message: str) -> "Response[None]":
        return cls(success=False, message=message)

class CreateEmployeeRequest(BaseModel):
    full_name: str
    phone_number: str
    email: str
    address: str

class UpdateEmployeeRequest(BaseModel):
    full_name: Optional[str] = None
    phone_number: Optional[str] = None
    email: Optional[str] = None
    address: Optional[str] = None

class EmployeeResponse(BaseModel):
    id: int
    full_name: str
    phone_number: str
    email: str
    address: str

class EmployeeBiometricsResponse(BaseModel):
    employee_id: int
    total_face_biometrics: int
    total_fingerprint_biometrics: int

class AddBiometricRequest(BaseModel):
    employee_id: int
    biometric_type: str
    biometric_datas: list[str]

class ByDateAttendanceDetailResponse(BaseModel):
    employee_id: int
    full_name: str
    device_id: str
    timestamp: str
    biometric_type: str

class ByEmployeeAttendanceDetailResponse(BaseModel):
    timestamp: str
    device_id: str
    biometric_type: str