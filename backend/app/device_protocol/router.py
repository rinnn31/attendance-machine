from fastapi import WebSocket, WebSocketDisconnect, APIRouter, Header, status

from app.core import get_settings
from app.device_protocol.manager import DeviceManager

router = APIRouter(tags=["device_protocol"], prefix="/device_protocol")

@router.websocket("/ws")
async def websocket_endpoint(
    websocket: WebSocket,
    authorization: str | None = Header(default=None),
    x_client_id: str | None = Header(default=None),
):
    settings = get_settings()
    if settings.AUTHORIZATION_TOKEN and settings.AUTHORIZATION_TOKEN != authorization:
        await websocket.close(status.WS_1008_POLICY_VIOLATION)
        return

    device_manager : DeviceManager = websocket.app.state.device_manager
    if not x_client_id or device_manager.add_device(x_client_id, websocket) is False:
        await websocket.close(status.WS_1008_POLICY_VIOLATION)
        return

    try:
        await websocket.accept()
        while True:
            data = await websocket.receive_bytes()
            await device_manager.handle_device_message(x_client_id, data)
            
    except WebSocketDisconnect:
        device_manager.remove_device(x_client_id)

