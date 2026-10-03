import os

from sqlalchemy.ext.asyncio import AsyncSession

from app.core.config import get_settings
from app.models.biometric import BiometricType
from app.recognition.face_recognition import FaceData
from app.schemas import ByEmployeeAttendanceDetailResponse, ByDateAttendanceDetailResponse
from app.models import AttendanceRecord
from app.recognition import FaceRecognition, FingerprintRecognition

class AttendanceService:
    def __init__(self, db: AsyncSession, face_recognition: FaceRecognition = None, fingerprint_recognition: FingerprintRecognition = None):
        self._db = db
        self._face_recognition = face_recognition
        self._fingerprint_recognition = fingerprint_recognition

    def get_employee_attendance(self, employee_id: int, start_date: str, end_date: str) -> list[ByEmployeeAttendanceDetailResponse]:
        query = f"""
            SELECT timestamp, device_id, biometric_type
            FROM attendance_records
            WHERE employee_id = :employee_id AND timestamp BETWEEN :start_date AND :end_date
            ORDER BY timestamp ASC
        """
        result = self._db.execute(query, {"employee_id": employee_id, "start_date": start_date, "end_date": end_date})
        attendance_records = result.fetchall()

        attendance_details = [
            ByEmployeeAttendanceDetailResponse(
                timestamp=record.timestamp,
                device_id=record.device_id,
                biometric_type=record.biometric_type
            )
            for record in attendance_records
        ]

        return attendance_details

    def get_date_attendance(self, date: str) -> list[ByDateAttendanceDetailResponse]:
        query = f"""
            SELECT employee_id, full_name, device_id, timestamp, biometric_type
            FROM attendance_records
            JOIN employees ON attendance_records.employee_id = employees.id
            WHERE DATE(FROM_UNIXTIME(timestamp)) = :date
            ORDER BY timestamp ASC
        """
        result = self._db.execute(query, {"date": date})
        attendance_records = result.fetchall()

        attendance_details = [
            ByDateAttendanceDetailResponse(
                employee_id=record.employee_id,
                full_name=record.full_name,
                device_id=record.device_id,
                timestamp=record.timestamp,
                biometric_type=record.biometric_type
            )
            for record in attendance_records
        ]

        return attendance_details

    def _check_valid_face(self, face_data: FaceData, image_width: int, image_height: int) -> bool:
        img_cx, img_cy = image_width / 2.0, image_height / 2.0

        face_cx = (face_data.bounding_box[0] + face_data.bounding_box[2]) / 2.0
        face_cy = (face_data.bounding_box[1] + face_data.bounding_box[3]) / 2.0

        # Kiểm tra xem khuôn mặt có đang ở chính giữa ảnh không
        if abs(face_cx - img_cx) > image_width * 0.3 or abs(face_cy - img_cy) > image_height * 0.3:
            return False

        # Kiểm tra xem khuôn mặt có đang nhìn thẳng vào camera không
        if abs(face_data.pose[0]) > 15 or abs(face_data.pose[1]) > 15:
            return False

        return True

    def handle_attendance_request(self, timestamp: int, biometric_type: BiometricType, biometric_data: bytes) -> bool:
        if biometric_type == BiometricType.FACE and not self._face_recognition:
            raise ValueError("FaceRecognition instance is not provided for face biometric type.")
        if biometric_type == BiometricType.FINGERPRINT and not self._fingerprint_recognition:
            raise ValueError("FingerprintRecognition instance is not provided for fingerprint biometric type.")



        if biometric_type == BiometricType.FACE:
            image_size = get_settings().FACE_IMAGE_DATA_SIZE
            width, height = map(int, image_size.split('x'))

            embeddings = self._face_recognition.extract_faces_data(biometric_data)
            for face_data in embeddings:
                ret = self._check_valid_face(face_data, width, height)
                if ret:
                    employee_id = self._face_recognition.recognize_face(face_data.embedding)
                    # Do chỉ có thể có một khuôn mặt nằm chinh giữa ảnh,
                    # nên nếu đã nhận diện được một khuôn mặt hợp lệ thì không cần kiểm tra các khuôn mặt khác
                    break

        elif biometric_type == BiometricType.FINGERPRINT:
            employee_id = self._fingerprint_recognition.recognize_fingerprint(biometric_data)

        if employee_id:
            attendance_record = AttendanceRecord(
                employee_id=employee_id,
                timestamp=timestamp,
                biometric_type=biometric_type
            )
            self._db.add(attendance_record)
            self._db.commit()
            return True

        return False