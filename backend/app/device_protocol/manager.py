import re
import secrets

from fastapi import WebSocket, status
import asyncio

from sqlalchemy import select

from app.device_protocol.protocol import (
    parse_message,
    parse_attendance_request_payload,
    parse_collect_data_response_payload,
    build_collect_data_request_payload,
    build_cancel_collect_data_request_payload,
    build_attendance_response_payload,
    build_set_auth_command_payload,
    BiometricType,
    MessageType,
    DeviceToServerCommand
)
from app.recognition.face_recognition import FaceRecognition
from app.recognition.fingerprint_recognition import FingerprintRecognition
from app.service.attendance_service import AttendanceService
from app.models import DBBiometricType, Device

PENDING_APPROVAL_TIMEOUT_S = 300.0
MAX_PENDING_DEVICES = 20
READY_SIGNAL = "ready"      # text frame: device is authenticated, may send data
PENDING_SIGNAL = "pending"  # text frame: device is waiting for admin approval
DEVICE_ID_PATTERN = re.compile(r"^[A-Za-z0-9_.:-]{1,64}$")


def _extract_auth_key(authorization: str | None) -> str:
    """The ESP client sends `Authorization: Basic <key>`; accept a bare key too."""
    if not authorization:
        return ""
    value = authorization.strip()
    if value[:6].lower() == "basic ":
        value = value[6:].strip()
    return value


class DeviceManager:
    def __init__(self):
        self._devices : dict[str, WebSocket] = {}
        self._pending_devices: dict[str, dict] = {}

        self._device_response_futures: dict[str, dict[int, asyncio.Future[bytes]]] = {}
        self._device_request_id_counters: dict[str, int] = {}
        self._device_collect_request_ids: dict[str, int] = {}

    def get_device_ids(self) -> list[str]:
        return list(self._devices.keys())

    def _cleanup_device_state(self, device_id: str):
        for fut in self._device_response_futures.pop(device_id, {}).values():
            if not fut.done():
                fut.cancel()
        self._device_request_id_counters.pop(device_id, None)
        self._device_collect_request_ids.pop(device_id, None)

    @staticmethod
    async def _safe_close(websocket: WebSocket, code: int = status.WS_1008_POLICY_VIOLATION):
        try:
            await websocket.close(code)
        except Exception:
            pass

    async def _register_device(self, device_id: str, websocket: WebSocket):
        old = self._devices.get(device_id)
        if old is not None and old is not websocket:
            print(f"Device {device_id} reconnected; closing the previous connection.")
            self._cleanup_device_state(device_id)
            await self._safe_close(old)
        self._devices[device_id] = websocket

    @staticmethod
    async def _wait_for_disconnect(websocket: WebSocket):
        while True:
            message = await websocket.receive()
            if message["type"] == "websocket.disconnect":
                return

    async def authenticate_or_queue_device(self, websocket: WebSocket, device_id: str, authorization: str | None) -> bool:
        if not DEVICE_ID_PATTERN.match(device_id):
            print("Rejected connection with invalid device id.")
            return False

        auth_key = _extract_auth_key(authorization)
        print(f"Device {device_id} connected with auth key: {auth_key if auth_key else '<empty>'}")
        # Short-lived session: never hold a DB session while waiting for approval.
        db = websocket.app.state.database.get_session()
        try:
            device = (await db.execute(select(Device).where(Device.device_id == device_id))).scalar_one_or_none()
            stored_key = device.device_auth_key if device else None
        finally:
            await db.close()

        print(f"Stored key for device {device_id}: {stored_key if stored_key else '<none>'}")
        if auth_key:
            if stored_key and secrets.compare_digest(auth_key.encode(), stored_key.encode()):
                await self._register_device(device_id, websocket)
                try:
                    # Tell the device it may start sending data.
                    await websocket.send_text(READY_SIGNAL)
                except Exception as e:
                    print(f"Failed to send ready signal to {device_id}: {e}")
                    self.remove_device(device_id, websocket)
                    return False
                print(f"Device {device_id} authenticated and added successfully.")
                return True

            print(f"Authorization failed for device {device_id}.")
            # Tell the device its key is no longer valid (empty key) so it can clear it
            # and request approval again, instead of retrying a bad key forever.
            try:
                await self.send_set_auth_command(websocket, "")
            except Exception:
                pass
            return False

        # No key: only a device that has never been approved may enter the approval queue.
        # Otherwise anyone who knows a registered device_id could get its key replaced.
        if stored_key:
            print(f"Device {device_id} already has a key but connected without one. Rejected "
                  f"(forget the device first if it was reset).")
            return False

        if device_id in self._pending_devices:
            print(f"Device {device_id} is already pending approval. Rejected duplicate.")
            return False

        if len(self._pending_devices) >= MAX_PENDING_DEVICES:
            print("Too many pending devices. Rejected.")
            return False

        print(f"Device {device_id} connected with empty auth. Entering pending state for approval...")

        approval_event = asyncio.Event()
        entry = {
            "websocket": websocket,
            "event": approval_event,
            "approved": False
        }
        self._pending_devices[device_id] = entry

        # Tell the device it is waiting for approval; it must not send data until it gets "ready".
        # Anything it sends meanwhile is read and discarded by _wait_for_disconnect below.
        try:
            await websocket.send_text(PENDING_SIGNAL)
        except Exception:
            self._pending_devices.pop(device_id, None)
            return False

        approval_task = asyncio.ensure_future(approval_event.wait())
        disconnect_task = asyncio.ensure_future(self._wait_for_disconnect(websocket))
        try:
            done, _ = await asyncio.wait(
                {approval_task, disconnect_task},
                timeout=PENDING_APPROVAL_TIMEOUT_S,
                return_when=asyncio.FIRST_COMPLETED,
            )
        finally:
            for task in (approval_task, disconnect_task):
                if not task.done():
                    task.cancel()
            if self._pending_devices.get(device_id) is entry:
                self._pending_devices.pop(device_id, None)

        if disconnect_task in done:
            print(f"Device {device_id} disconnected while pending.")
            return False

        if approval_task not in done:
            print(f"Device {device_id} approval timed out after {int(PENDING_APPROVAL_TIMEOUT_S // 60)} minutes.")
            return False

        if not entry["approved"]:
            print(f"Device {device_id} was rejected.")
            return False

        new_auth_key = secrets.token_hex(32)
        db = websocket.app.state.database.get_session()
        try:
            device = (await db.execute(select(Device).where(Device.device_id == device_id))).scalar_one_or_none()
            if device:
                device.device_auth_key = new_auth_key
            else:
                db.add(Device(device_id=device_id, device_auth_key=new_auth_key))
            await db.commit()
        finally:
            await db.close()

        await self._register_device(device_id, websocket)
        try:
            await self.send_set_auth_command(websocket, new_auth_key)
        except Exception as e:
            print(f"Failed to send auth key to {device_id}: {e}")
            self.remove_device(device_id, websocket)
            return False

        print(f"Device {device_id} approved and assigned new auth key.")
        return True

    async def send_set_auth_command(self, websocket: WebSocket, auth_key: str):
        request_id = 0
        message = build_set_auth_command_payload(request_id, auth_key)
        await websocket.send_bytes(message)

    def approve_device(self, device_id: str) -> bool:
        pending = self._pending_devices.get(device_id)
        if pending:
            pending["approved"] = True
            pending["event"].set()
            return True
        return False

    def reject_device(self, device_id: str) -> bool:
        pending = self._pending_devices.get(device_id)
        if pending:
            pending["approved"] = False
            pending["event"].set()
            return True
        return False

    def get_devices(self) -> dict[str, str]:
        result = {device_id: "pending" for device_id in self._pending_devices.keys()}
        result.update({device_id: "connected" for device_id in self._devices.keys()})
        return result

    async def forget_device(self, device_id: str, db=None) -> bool:
        """Disconnect the device (if online) and delete its record, so it can pair again.

        Pass `db` to forget an offline device; otherwise a session is taken from the
        connected socket's app, which only works while the device is online.
        """
        ws = self._devices.pop(device_id, None)
        pending = self._pending_devices.get(device_id)
        self._cleanup_device_state(device_id)

        if pending:
            pending["approved"] = False
            pending["event"].set()

        for sock in (ws, pending["websocket"] if pending else None):
            if sock is not None:
                try:
                    asyncio.get_running_loop().create_task(self._safe_close(sock))
                except RuntimeError:
                    pass

        owns_session = False
        if db is None and ws is not None:
            db = ws.app.state.database.get_session()
            owns_session = True
        if db is None:
            return False

        try:
            device = (await db.execute(select(Device).where(Device.device_id == device_id))).scalar_one_or_none()
            if not device:
                return False
            await db.delete(device)
            await db.commit()
            return True
        finally:
            if owns_session:
                await db.close()

    def remove_device(self, device_id: str, websocket: WebSocket | None = None) -> bool:
        current = self._devices.get(device_id)
        if current is None:
            return False

        # A late disconnect of an old socket must not evict the newer connection.
        if websocket is not None and current is not websocket:
            return False

        del self._devices[device_id]
        self._cleanup_device_state(device_id)
        return True

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

        print(f"Sending collect data request to {device_id} with request_id {request_id} and biometric_type {biometric_type}")

        fut = asyncio.get_running_loop().create_future()
        self._device_response_futures.setdefault(device_id, {})[request_id] = fut
        self._device_collect_request_ids[device_id] = request_id
        try:
            await websocket.send_bytes(payload)
            data = await asyncio.wait_for(fut, timeout=60.0)
            if data is None:
                return False, b""
            
            return parse_collect_data_response_payload(data)
        finally:
            self._device_response_futures.get(device_id, {}).pop(request_id, None)

    async def cancel_collect_data(self, device_id: str):
        websocket = self._devices.get(device_id)
        if not websocket:
            return

        request_id = self._generate_request_id(device_id)
        payload = build_cancel_collect_data_request_payload(request_id)

        collect_request_id = self._device_collect_request_ids.get(device_id)
        if collect_request_id is not None:
            fut = self._device_response_futures.get(device_id, {}).get(collect_request_id)
            if fut and not fut.done():
                fut.cancel()
            self._device_collect_request_ids.pop(device_id, None)

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
            print(f"Received message from {device_id} with requestid: {header.request_id} and payload length: {len(payload)}")
            if header.message_type == MessageType.REQUEST:
                match header.command_id:
                    case DeviceToServerCommand.ATTENDANCE_REQUEST.value:
                        biometric_type, timestamp, biometric_data = parse_attendance_request_payload(payload)
                        print(f"Received attendance request from {device_id} with biometric_type {biometric_type}, timestamp {timestamp}, and data length {len(biometric_data)}")
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

    async def handle_attendance_request(self, device_id: str, timestamp: int, biometric_type: BiometricType, biometric_data: bytes) -> str:
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