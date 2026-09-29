from fastapi import APIRouter, Depends

from app.api.deps import get_database
from app.models.biometric import BiometricType
from app.service.biometric_service import BiometricService
from app.schemas import Response, EmployeeBiometricsResponse, AddBiometricRequest

router = APIRouter(tags=["biometric"], prefix="/biometrics")

@router.get("/{employee_id}", response_model=Response[EmployeeBiometricsResponse])
async def get_employee_biometrics_info(
    employee_id: int,
    db=Depends(get_database)
) -> Response[EmployeeBiometricsResponse]:
    
    biometric_service = BiometricService(db)
    return Response.success(await biometric_service.get_employee_biometrics(employee_id))


@router.post()
async def add_biometric_data(
    request: AddBiometricRequest, 
    db=Depends(get_database)
):
    biometric_service = BiometricService(db)

    if request.biometric_type == BiometricType.FACE.value:
        success_count = await biometric_service.add_face_data(request.employee_id, request.biometric_datas)
    elif request.biometric_type == BiometricType.FINGERPRINT.value:
        success_count = await biometric_service.add_fingerprint_data(request.employee_id, request.biometric_datas)
    else:
        return Response.error("Invalid biometric type")

    return Response.success(success_count)

@router.delete("/{employee_id}/{biometric_type}")
async def delete_biometric_data(
    employee_id: int, 
    biometric_type: BiometricType, 
    db=Depends(get_database)
):
    biometric_service = BiometricService(db)
    await biometric_service.clear_biometric_data(employee_id, biometric_type)
    return Response.success()
