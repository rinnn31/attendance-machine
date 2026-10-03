from typing import Optional
from sqlalchemy.ext.asyncio import AsyncSession
import base64

from app.models.biometric import BiometricType, Biometric
from app.recognition import FaceRecognition, FingerprintRecognition
from app.schemas.schemas import EmployeeBiometricsResponse

class BiometricService:
    def __init__(self, session: AsyncSession, face_recognition: FaceRecognition, fingerprint_recognition: FingerprintRecognition):
        self._session = session
        self._face_recognition = face_recognition
        self._fingerprint_recognition = fingerprint_recognition

    async def get_employee_biometrics(self, employee_id: int) -> EmployeeBiometricsResponse:
        result = await self._session.execute(
            "SELECT type, COUNT(*) FROM biometrics WHERE employee_id = :employee_id GROUP BY type",
            {"employee_id": employee_id}
        )
        counts = {row[0]: row[1] for row in result.fetchall()}

        return EmployeeBiometricsResponse(
            employee_id=employee_id,
            total_face_biometrics=counts.get(BiometricType.FACE.value, 0),
            total_fingerprint_biometrics=counts.get(BiometricType.FINGERPRINT.value, 0)
        )

    async def add_face_data(self, employee_id: int, face_datas: list[str]) -> int:
        success_count = 0

        for face_data_b64 in face_datas:
            face_data = base64.b64decode(face_data_b64)

            info = self._face_recognition.extract_faces_data(face_data)
            if info is None or len(info) != 1 or info[0].score < 0.5:
                continue

            embedding = info[0].embedding
            biometric = Biometric(
                employee_id=employee_id,
                type=BiometricType.FACE.value,
                data=embedding
            )

            self._session.add(biometric)

            self._face_recognition.add_face_embedding(str(employee_id), embedding)

            success_count += 1
            
        await self._session.commit()

        return success_count

    async def add_fingerprint_data(self, employee_id: int, fingerprint_datas: list[str]) -> int:
        success_count = 0

        for fingerprint_data_b64 in fingerprint_datas:
            fingerprint_data = base64.b64decode(fingerprint_data_b64)

            data = self._fingerprint_recognition.extract_fingerprint_data(fingerprint_data)
            if data is None or data.score < 0.5:
                continue

            biometric = Biometric(
                employee_id=employee_id,
                type=BiometricType.FINGERPRINT.value,
                data=data.template
            )

            self._session.add(biometric)

            self._fingerprint_recognition.add_fingerprint_template(str(employee_id), data.template)

            success_count += 1
            
        await self._session.commit()
        return success_count

    async def clear_biometric_data(self, employee_id: int, type: BiometricType) -> int:
        result = await self._session.execute(
            "DELETE FROM biometrics WHERE employee_id = :employee_id AND type = :type RETURNING id", 
            {"employee_id": employee_id, "type": type.value}
        )
    
        deleted_ids = result.scalars().all()
        await self._session.commit()

        if type == BiometricType.FACE:
            self._face_recognition.remove_face_embeddings(str(employee_id))
        elif type == BiometricType.FINGERPRINT:
            self._fingerprint_recognition.remove_fingerprint_templates(str(employee_id))

        return len(deleted_ids)

    async def get_all_biometric_data(self, employee_id: Optional[int] = None) -> list[Biometric]:
        query = "SELECT * FROM biometrics"
        params = {}

        if employee_id is not None:
            query += " WHERE employee_id = :employee_id"
            params["employee_id"] = employee_id

        result = await self._session.execute(query, params)
        biometrics = result.scalars().all()

        return biometrics

    async def get_biometric_data(self, employee_id: int, type: BiometricType) -> list[Biometric]:
        result = await self._session.execute(
            "SELECT * FROM biometrics WHERE employee_id = :employee_id AND type = :type",
            {"employee_id": employee_id, "type": type.value}
        )
        biometrics = result.scalars().all()

        return biometrics