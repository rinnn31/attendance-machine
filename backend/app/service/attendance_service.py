import numpy as np
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from datetime import datetime, timedelta
import cv2
import hashlib

from app.core.config import get_settings
from app.recognition.face_recognition import FaceData
from app.schemas import ByEmployeeAttendanceDetailResponse, ByDateAttendanceDetailResponse
from app.models import AttendanceRecord, Employee, DBBiometricType
from app.recognition import FaceRecognition, FingerprintRecognition

class AttendanceService:
    def __init__(self, db: AsyncSession, face_recognition: FaceRecognition = None, fingerprint_recognition: FingerprintRecognition = None):
        self._db = db
        self._face_recognition = face_recognition
        self._fingerprint_recognition = fingerprint_recognition

    async def get_employee_attendance(self, employee_id: int, start_date: str, end_date: str) -> list[ByEmployeeAttendanceDetailResponse]:
        start_time = datetime.strptime(start_date, "%Y-%m-%d")
        end_time = datetime.strptime(end_date, "%Y-%m-%d") + timedelta(days=1)

        stmt = select(AttendanceRecord.timestamp, AttendanceRecord.device_id, AttendanceRecord.biometric_type, AttendanceRecord.face_capture_image)\
               .where(AttendanceRecord.employee_id == employee_id)\
               .where(AttendanceRecord.timestamp >= start_time)\
               .where(AttendanceRecord.timestamp < end_time)\
               .order_by(AttendanceRecord.timestamp.asc())
        result = await self._db.execute(stmt)
               
        attendance_records = result.mappings().all()

        attendance_details = [
            ByEmployeeAttendanceDetailResponse(
                timestamp=int(record["timestamp"].timestamp()),
                device_id=record["device_id"],
                biometric_type=record["biometric_type"],
                face_capture_image=("/files/"+ record["face_capture_image"]) if record["face_capture_image"] else None
            )
            for record in attendance_records
        ]

        return attendance_details

    async def get_date_attendance(self, date: str) -> list[ByDateAttendanceDetailResponse]:
        start_time = datetime.strptime(date, "%Y-%m-%d")
        end_time = start_time + timedelta(days=1)

        stmt = select(AttendanceRecord.employee_id, AttendanceRecord.device_id, AttendanceRecord.timestamp, AttendanceRecord.biometric_type, AttendanceRecord.face_capture_image, Employee.full_name)\
                .join(Employee, AttendanceRecord.employee_id == Employee.id)\
                .where(AttendanceRecord.timestamp >= start_time)\
                .where(AttendanceRecord.timestamp < end_time)\
                .order_by(AttendanceRecord.timestamp.asc())
        result = await self._db.execute(stmt)
        attendance_records = result.mappings().all()

        attendance_details = [
            ByDateAttendanceDetailResponse(
                employee_id=record["employee_id"],
                full_name=record["full_name"],
                device_id=record["device_id"],
                timestamp=int(record["timestamp"].timestamp()),
                biometric_type=record["biometric_type"],
                face_capture_image=("/files/"+ record["face_capture_image"]) if record["face_capture_image"] else None
            )
            for record in attendance_records
        ]

        return attendance_details

    def _select_primary_face(self, faces: list[FaceData], img_width: int, img_height: int) -> FaceData | None:
        if not faces:
            return None

        img_cx, img_cy = img_width / 2.0, img_height / 2.0
        valid_candidates = []

        for face in faces:
            x1, y1, x2, y2 = face.bounding_box
            face_w = x2 - x1
            face_h = y2 - y1
            face_cx = (x1 + x2) / 2.0
            face_cy = (y1 + y2) / 2.0

            print(f"""
            Face size: {face_w}x{face_h}, min size: {img_width * 0.2}x{img_height * 0.2}
            Face center: ({face_cx}, {face_cy}), image center: ({img_cx}, {img_cy}), max offset: ({img_width * 0.25}, {img_height * 0.25})
            Face pose: {face.pose}, max allowed pose: (20, 20)
            """)

            # Bỏ qua khuôn mặt quá nhỏ
            if face_h < img_height * 0.2:
                continue

            # Bỏ qua các khuôn mặt ở rìa
            if abs(face_cx - img_cx) > img_width * 0.25 or abs(face_cy - img_cy) > img_height * 0.25:
                continue

            # Bỏ qua các khuôn mặt bị nghiêng quá nhiều
            if abs(face.pose[0]) > 20 or abs(face.pose[1]) > 20:
                continue

            # Diện tích bounding box làm trọng số ưu tiên
            area = face_w * face_h
            valid_candidates.append((area, face))

        if not valid_candidates:
            return None

        # Sắp xếp lấy khuôn mặt có diện tích lớn nhất (gần camera nhất)
        valid_candidates.sort(key=lambda x: x[0], reverse=True)
        return valid_candidates[0][1]
  
    async def handle_attendance_request(self, device_id: str, timestamp: int, biometric_type: DBBiometricType, biometric_data: bytes) -> str:
        if biometric_type == DBBiometricType.FACE and not self._face_recognition:
            raise ValueError("FaceRecognition instance is not provided for face biometric type.")
        if biometric_type == DBBiometricType.FINGERPRINT and not self._fingerprint_recognition:
            raise ValueError("FingerprintRecognition instance is not provided for fingerprint biometric type.")

        employee_id = None
        face_capture_image_file_name = None

        try:
            if biometric_type == DBBiometricType.FACE:
                image_size = get_settings().FACE_IMAGE_DATA_SIZE
                width, height = map(int, image_size.split('x'))
                
                embeddings = self._face_recognition.extract_faces_data(biometric_data)
                if embeddings:
                    primary_face = self._select_primary_face(embeddings, width, height)
                    if primary_face and primary_face.score >= 0.5:
                        employee_id = self._face_recognition.recognize_face(primary_face.embedding)

                        img = cv2.imdecode(np.frombuffer(biometric_data, np.uint8), cv2.IMREAD_COLOR)
                        x1, y1, x2, y2 = primary_face.bounding_box
                        img = cv2.rectangle(img, (x1, y1), (x2, y2), (0, 255, 0), 2)

                        face_capture_image_file_name = hashlib.md5(f"{employee_id}_{timestamp}".encode()).hexdigest() + ".jpg"
                        print(f"Saving face capture image to {get_settings().FACE_CAPTURE_IMAGE_PATH + face_capture_image_file_name}")
                        cv2.imwrite(get_settings().FACE_CAPTURE_IMAGE_PATH + face_capture_image_file_name, img)

                
            elif biometric_type == DBBiometricType.FINGERPRINT:
                employee_id = self._fingerprint_recognition.recognize_fingerprint(biometric_data)

            if employee_id:
                employee = await self._db.get(Employee, employee_id)
                if not employee:
                    return None
                
                attendance_record = AttendanceRecord(
                    employee_id=employee_id,
                    timestamp=datetime.fromtimestamp(timestamp),
                    biometric_type=biometric_type,
                    device_id=device_id,
                    face_capture_image=face_capture_image_file_name
                )
                self._db.add(attendance_record)
                await self._db.commit()

                print(f"Attendance recorded for employee_id {employee_id} with name {employee.full_name} at {datetime.fromtimestamp(timestamp)} from device {device_id} using {biometric_type.name}."  )
                return employee.full_name
                
            return None
        except Exception as e:
            print(f"Error handling attendance request: {e}")
            return None