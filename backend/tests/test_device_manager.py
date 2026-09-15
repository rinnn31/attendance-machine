import pytest
import asyncio
from unittest.mock import AsyncMock

from app.device_protocol.manager import DeviceManager
from app.device_protocol.protocol import (
    BiometricType,
    MessageType,
    DeviceToServerCommand,
    parse_message,
    build_request_message,
    build_response_message
)


@pytest.fixture
def manager():
    return DeviceManager()


@pytest.fixture
def mock_ws():
    ws = AsyncMock()
    ws.send_bytes = AsyncMock()
    return ws

def test_add_and_remove_device(manager: DeviceManager, mock_ws):
    device_id = "ESP32_DEVICE_01"
    
    assert manager.add_device(device_id, mock_ws) is True
    assert manager.add_device(device_id, mock_ws) is False

    assert manager.remove_device(device_id) is True
    assert manager.remove_device(device_id) is False

@pytest.mark.asyncio
async def test_handle_attendance_request_success(manager: DeviceManager, mock_ws):
    device_id = "ESP32_DEVICE_01"
    manager.add_device(device_id, mock_ws)

    received_events = []

    async def fake_attendance_handler(timestamp: int, bio_type: BiometricType, data: bytes) -> bool:
        received_events.append((timestamp, bio_type, data))
        return True

    manager.set_attendance_request_handler(fake_attendance_handler)

    fake_timestamp = 1715000000
    fake_image = b"\xff\xd8\xff\xe0_fake_jpeg_data"
    req_payload = (
        BiometricType.FACE.value.to_bytes(1, "big")
        + fake_timestamp.to_bytes(8, "little", signed=True)
        + fake_image
    )
    req_id = 100
    raw_packet = build_request_message(
        request_id=req_id,
        command_id=DeviceToServerCommand.ATTENDANCE_REQUEST.value,
        payload=req_payload
    )

    await manager.handle_device_message(device_id, raw_packet)

    assert len(received_events) == 1
    ts, bio, img = received_events[0]
    assert ts == fake_timestamp
    assert bio == BiometricType.FACE
    assert img == fake_image

    mock_ws.send_bytes.assert_called_once()
    sent_bytes = mock_ws.send_bytes.call_args[0][0]
    header, payload = parse_message(sent_bytes)
    assert header.message_type == MessageType.RESPONSE
    assert header.request_id == req_id
    assert payload == b"\x01"  # ACK Success


@pytest.mark.asyncio
async def test_collect_data_success(manager: DeviceManager, mock_ws):
    device_id = "ESP32_DEVICE_01"
    manager.add_device(device_id, mock_ws)

    fake_collected_image = b"\xff\xd8_face_captured_jpeg"

    async def simulate_esp32_response(sent_bytes: bytes):
        header, _ = parse_message(sent_bytes)
        await asyncio.sleep(0.01)
        resp_payload = b"\x01" + fake_collected_image
        resp_packet = build_response_message(header.request_id, resp_payload)
        await manager.handle_device_message(device_id, resp_packet)

    mock_ws.send_bytes.side_effect = simulate_esp32_response

    success, data = await manager.collect_data(device_id, BiometricType.FACE)

    assert success is True
    assert data == fake_collected_image
