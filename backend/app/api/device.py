from fastapi import APIRouter, Depends
import base64

from app.device_protocol.manager import DeviceManager, BiometricType
from app.api.deps import get_device_manager
from app.schemas import Response

router = APIRouter(tags=["Device"], prefix="/devices")

@router.get(path="/active")
async def list_active_devices(device_manager: DeviceManager = Depends(get_device_manager)) -> Response[list[str]]:
    return Response.make_success(data=device_manager.get_device_ids())

@router.post(path="/{device_id}/collect/{biometric_type}", response_model=Response)
async def collect_data(device_id: str, biometric_type: str, device_manager: DeviceManager = Depends(get_device_manager)):
    biometric_type_enum = BiometricType.FACE if biometric_type.lower() == "face" else BiometricType.FINGERPRINT

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

@router.get(path="/")
def list_devices(device_manager: DeviceManager = Depends(get_device_manager)) -> Response[dict[str, str]]:
    return Response.make_success(data=device_manager.get_devices())

@router.post(path="/{device_id}/approve")
async def approve_device(device_id: str, device_manager: DeviceManager = Depends(get_device_manager)) -> Response[None]:
    if device_manager.approve_device(device_id):
        return Response.make_success()
    else:
        return Response.make_error(message="Device not found or already approved.")

@router.delete(path="/{device_id}/forget")
async def forget_device(device_id: str, device_manager: DeviceManager = Depends(get_device_manager)) -> Response[None]:
    if await device_manager.forget_device(device_id):
        return Response.make_success()
    else:
        return Response.make_error(message="Device not found.")

@router.post(path="/{device_id}/reject")
async def reject_device(device_id: str, device_manager: DeviceManager = Depends(get_device_manager)) -> Response[None]:
    if device_manager.reject_device(device_id):
        return Response.make_success()
    else:
        return Response.make_error(message="Device not found or already rejected.")