from fastapi import APIRouter, Depends

from app.device_protocol.manager import DeviceManager
from app.api.deps import get_device_manager
from app.models.biometric import BiometricType
from app.schemas import Response

router = APIRouter(tags=["Device"], prefix="/devices")

@router.get(path="/", response_model=Response[list[str]])
async def list_devices(device_manager: DeviceManager = Depends(get_device_manager)):
    return Response(data=list(device_manager.get_device_ids()))

@router.post(path="/{device_id}/collect/{biometric_type}", response_model=Response)
async def collect_data(device_id: str, biometric_type: str, device_manager: DeviceManager = Depends(get_device_manager)):
    try:
        biometric_type_enum = BiometricType(biometric_type)
    except ValueError:
        return Response(success=False, message=f"Invalid biometric type: {biometric_type}")

    success, data = await device_manager.collect_data(device_id, biometric_type_enum)
    if success:
        return Response(data=data)
    else:
        return Response(success=False, message="Failed to collect data from the device.")

@router.post(path="/{device_id}/cancel_collect", response_model=Response[None])
async def cancel_collect_data(device_id: str, device_manager: DeviceManager = Depends(get_device_manager)):
    await device_manager.cancel_collect_data(device_id)
