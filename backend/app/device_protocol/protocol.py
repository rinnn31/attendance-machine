from dataclasses import dataclass
import enum

FIXED_HEADER_LENGTH = 3
FIXED_REQUEST_HEADER_LENGTH = FIXED_HEADER_LENGTH + 1

class MessageType(enum.Enum):
    REQUEST = 1
    RESPONSE = 2

class ServerToDeviceCommand(enum.Enum):
    COLLECT_FACE_DATA = 1
    COLLECT_FINGERPRINT_DATA = 2
    CANCEL_COLLECTING_DATA = 3

class DeviceToServerCommand(enum.Enum):
    ATTENDANCE_REQUEST = 1

class BiometricType(enum.Enum):
    FACE = 0
    FINGERPRINT = 1

@dataclass(slots=True)
class MessageHeader:
    message_type: MessageType
    request_id: int
    command_id: int | None

def parse_message(message: bytes) -> tuple[MessageHeader, bytes]:
    if len(message) < FIXED_HEADER_LENGTH:
        raise ValueError("Message is too short to contain a valid header.")

    message_type = message[0]

    if message_type == MessageType.REQUEST.value:
        if len(message) < FIXED_REQUEST_HEADER_LENGTH:
            raise ValueError("Request message is too short to contain a valid header.")
        command_id = message[3]
        payload = message[4:]

    elif message_type == MessageType.RESPONSE.value:
        command_id = None
        payload = message[3:]

    else:
        raise ValueError(f"Unknown message type: {message_type}")

    request_id = int.from_bytes(message[1:3], byteorder='big')

    return MessageHeader(message_type=MessageType(message_type), request_id=request_id, command_id=command_id), payload

def build_response_message(request_id: int, payload: bytes) -> bytes:
    message_type = MessageType.RESPONSE.value.to_bytes(1, byteorder='big')
    request_id_bytes = request_id.to_bytes(2, byteorder='big')
    return message_type + request_id_bytes + payload

def build_request_message(request_id: int, command_id: int, payload: bytes) -> bytes:
    message_type = MessageType.REQUEST.value.to_bytes(1, byteorder='big')
    request_id_bytes = request_id.to_bytes(2, byteorder='big')
    command_id_byte = command_id.to_bytes(1, byteorder='big')
    return message_type + request_id_bytes + command_id_byte + payload

def parse_attendance_request_payload(payload: bytes) -> tuple[BiometricType, int, bytes]:
    if len(payload) <= 9:
        raise ValueError("Payload is too short to contain a valid attendance request.")

    biometric_type = payload[0]
    if biometric_type not in (BiometricType.FACE.value, BiometricType.FINGERPRINT.value):
        raise ValueError(f"Unknown biometric type: {biometric_type}")
    
    timestamp = int.from_bytes(payload[1:9], byteorder='little')
    data = payload[9:]

    return BiometricType(biometric_type), timestamp, data

def parse_collect_data_response_payload(payload: bytes) -> tuple[bool, bytes]:
    if len(payload) <= 1:
        raise ValueError("Payload is too short to contain a valid collect data response.")

    success = payload[0] == 1
    data = payload[1:]

    return success, data

def build_collect_data_request_payload(request_id: int, biometric_type: BiometricType) -> bytes:
    return build_request_message(
        request_id,
        ServerToDeviceCommand.COLLECT_FACE_DATA.value if biometric_type == BiometricType.FACE else ServerToDeviceCommand.COLLECT_FINGERPRINT_DATA.value,
        b""
    )

def build_cancel_collect_data_request_payload(request_id: int) -> bytes:
    return build_request_message(
        request_id,
        ServerToDeviceCommand.CANCEL_COLLECTING_DATA.value,
        b""
    )

def build_attendance_response_payload(request_id: int, result: str) -> bytes:
    success = result is not None and result.strip() != ""

    status_byte = b"\x01" if success else b"\x00"
    # Add a null terminator to the name string if success, otherwise return an empty byte string
    name_bytes = result.strip().encode('utf-8') + b"\x00" if success else b""
    return build_response_message(
        request_id,
        status_byte + name_bytes
    )
