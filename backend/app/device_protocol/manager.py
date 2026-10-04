from fastapi import WebSocket
import asyncio

from app.device_protocol.protocol import (
    parse_message,
    parse_attendance_request_payload,
    parse_collect_data_response_payload,
    build_collect_data_request_payload,
    build_cancel_collect_data_request_payload,
    build_attendance_response_payload,
    BiometricType,
    MessageType,
    DeviceToServerCommand
)
from app.recognition.face_recognition import FaceRecognition
from app.recognition.fingerprint_recognition import FingerprintRecognition
from app.service.attendance_service import AttendanceService
from app.models import DBBiometricType

class DeviceManager:
    def __init__(self):
        self._devices : dict[str, WebSocket] = {}

        self._device_response_futures: dict[str, dict[int, asyncio.Future[bytes]]] = {}
        self._device_request_id_counters: dict[str, int] = {}

    def get_device_ids(self) -> list[str]:
        return list(self._devices.keys())

    def add_device(self, device_id: str, websocket: WebSocket) -> bool:
        if device_id in self._devices:
            return False
        self._devices[device_id] = websocket
        return True

    def remove_device(self, device_id: str) -> bool:
        if device_id in self._devices:
            del self._devices[device_id]

            for _ , fut in self._device_response_futures.get(device_id, {}).items():
                if not fut.done():
                    fut.cancel()
            self._device_response_futures.pop(device_id, None)

            self._device_request_id_counters.pop(device_id, None)
            return True
        
        return False

    def _generate_request_id(self, device_id: str) -> int:
        if device_id not in self._device_request_id_counters:
            self._device_request_id_counters[device_id] = 0

        request_id = self._device_request_id_counters[device_id]
        self._device_request_id_counters[device_id] = (request_id + 1) % 65536
        return request_id

    async def collect_data(self, device_id: str, biometric_type: BiometricType) -> tuple[bool, bytes]:
        websocket = self._devices.get(device_id)
        if not websocket:
            return False, b""

        request_id = self._generate_request_id(device_id)
        payload = build_collect_data_request_payload(request_id, biometric_type)

        fut = asyncio.get_running_loop().create_future()
        self._device_response_futures.setdefault(device_id, {})[request_id] = fut

        try:
            await websocket.send_bytes(payload)
            data = await asyncio.wait_for(fut, timeout=60.0)
            return parse_collect_data_response_payload(data)
        finally:
            self._device_response_futures[device_id].pop(request_id, None)

    async def cancel_collect_data(self, device_id: str):
        websocket = self._devices.get(device_id)
        if not websocket:
            return

        request_id = self._generate_request_id(device_id)
        payload = build_cancel_collect_data_request_payload(request_id)

        try:
            await websocket.send_bytes(payload)
        except Exception as e:
            print(f"Error sending cancel collect data request to {device_id}: {e}")

    async def handle_device_message(self, device_id: str, message: bytes):
        try:
            websocket = self._devices.get(device_id)
            if not websocket:
                return
            
            header, payload = parse_message(message)
            if header.message_type == MessageType.REQUEST:
                match header.command_id:
                    case DeviceToServerCommand.ATTENDANCE_REQUEST.value:
                        biometric_type, timestamp, biometric_data = parse_attendance_request_payload(payload)
                        ret = await self.handle_attendance_request(device_id, timestamp, biometric_type, biometric_data)
                        await websocket.send_bytes(build_attendance_response_payload(header.request_id, ret))
                    case _:
                        pass
            
            else:
                fut = self._device_response_futures.get(device_id, {}).get(header.request_id)
                if fut and not fut.done():
                    fut.set_result(payload)

        except Exception as e:
            print(f"Error handling device message from {device_id}: {e}")
            pass

    async def handle_attendance_request(self, device_id: str, timestamp: int, biometric_type: BiometricType, biometric_data: bytes) -> bool:
        websocket = self._devices.get(device_id)
        if not websocket:
            return False

        db = websocket.app.state.database.get_session()
        face_recognition: FaceRecognition = websocket.app.state.face_recognition
        fingerprint_recognition: FingerprintRecognition = websocket.app.state.fingerprint_recognition

        attendance_service = AttendanceService(db, face_recognition, fingerprint_recognition)
        try:
            biometric_type_db = DBBiometricType.FACE if biometric_type == BiometricType.FACE else DBBiometricType.FINGERPRINT
            return await attendance_service.handle_attendance_request(device_id, timestamp, biometric_type_db, biometric_data)
        finally:
            await db.close()
