from fastapi import WebSocket, WebSocketDisconnect, APIRouter, Header, status

from app.core import get_settings
from app.device_protocol.manager import DeviceManager

router = APIRouter(tags=["device_protocol"])

@router.websocket("/ws")
async def websocket_endpoint(
    websocket: WebSocket,
    authorization: str | None = Header(default=None),
    x_client_id: str | None = Header(default=None),
):
    await websocket.accept()

    device_manager: DeviceManager = websocket.app.state.device_manager

    authenticated = False
    if x_client_id:
        authenticated = await device_manager.authenticate_or_queue_device(websocket, x_client_id, authorization)

    if not authenticated:
        try:
            await websocket.close(status.WS_1008_POLICY_VIOLATION)
        except RuntimeError:
            pass
        return

    try:
        while True:
            data = await websocket.receive_bytes()
            await device_manager.handle_device_message(x_client_id, data)

    except WebSocketDisconnect:
        pass
    finally:
        device_manager.remove_device(x_client_id, websocket)