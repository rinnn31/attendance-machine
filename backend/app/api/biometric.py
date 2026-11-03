from fastapi import APIRouter, Depends

from app.api.deps import get_database, get_face_recognition, get_fingerprint_recognition
from app.models.biometric import DBBiometricType
from app.service.biometric_service import BiometricService
from app.schemas import Response, EmployeeBiometricsResponse, AddBiometricRequest

router = APIRouter(tags=["biometric"], prefix="/biometrics")

@router.get(path="/{employee_id}")
async def get_employee_biometrics_info(
    employee_id: int,
    db=Depends(get_database),
    face_recognition=Depends(get_face_recognition),
    fingerprint_recognition=Depends(get_fingerprint_recognition)
) -> Response[EmployeeBiometricsResponse]:
    
    biometric_service = BiometricService(db, face_recognition, fingerprint_recognition)
    return Response.make_success(await biometric_service.get_employee_biometrics(employee_id))


@router.post(path="/")
async def add_biometric_data(
    request: AddBiometricRequest, 
    db=Depends(get_database),
    face_recognition=Depends(get_face_recognition),
    fingerprint_recognition=Depends(get_fingerprint_recognition)
) -> Response[int]:
    biometric_service = BiometricService(db, face_recognition, fingerprint_recognition)

    if request.biometric_type == DBBiometricType.FACE.value:
        success_count = await biometric_service.add_face_data(request.employee_id, request.biometric_datas)
    elif request.biometric_type == DBBiometricType.FINGERPRINT.value:
        success_count = await biometric_service.add_fingerprint_data(request.employee_id, request.biometric_datas)
    else:
        return Response.make_error("Invalid biometric type")

    return Response.make_success(success_count)

@router.delete(path="/{employee_id}/{biometric_type}")
async def delete_biometric_data(
    employee_id: int, 
    biometric_type: str, 
    db=Depends(get_database),
    face_recognition=Depends(get_face_recognition),
    fingerprint_recognition=Depends(get_fingerprint_recognition)
) -> Response[None]:
    biometric_type = DBBiometricType.FACE if biometric_type.lower() == "face" else DBBiometricType.FINGERPRINT
    
    biometric_service = BiometricService(db, face_recognition, fingerprint_recognition)
    await biometric_service.clear_biometric_data(employee_id, biometric_type)
    return Response.make_success()
