from fastapi import APIRouter, Depends
import base64

from app.device_protocol.manager import DeviceManager
from app.api.deps import get_device_manager
from app.models.biometric import DBBiometricType
from app.schemas import Response

router = APIRouter(tags=["Device"], prefix="/devices")

@router.get(path="/")
async def list_devices(device_manager: DeviceManager = Depends(get_device_manager)) -> Response[list[str]]:
    return Response.make_success(data=device_manager.get_device_ids())

@router.post(path="/{device_id}/collect/{biometric_type}", response_model=Response)
async def collect_data(device_id: str, biometric_type: str, device_manager: DeviceManager = Depends(get_device_manager)):
    try:
        biometric_type_enum = DBBiometricType(biometric_type)
    except ValueError:
        return Response.make_error(message=f"Invalid biometric type: {biometric_type}")

    success, data = await device_manager.collect_data(device_id, biometric_type_enum)
    if success:
        b64_data = base64.b64encode(data).decode('utf-8')
        return Response.make_success(data=b64_data)
    else:
        return Response.make_error(message="Failed to collect data from the device.")

@router.post(path="/{device_id}/cancel_collect")
async def cancel_collect_data(device_id: str, device_manager: DeviceManager = Depends(get_device_manager)) -> Response[None]:
    await device_manager.cancel_collect_data(device_id)
    return Response.make_success()
