from typing import Optional
import numpy as np
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select, delete, func
import base64

from app.models.biometric import DBBiometricType, Biometric
from app.recognition import FaceRecognition, FingerprintRecognition
from app.schemas.schemas import EmployeeBiometricsResponse

class BiometricService:
    def __init__(self, session: AsyncSession, face_recognition: FaceRecognition, fingerprint_recognition: FingerprintRecognition):
        self._session = session
        self._face_recognition = face_recognition
        self._fingerprint_recognition = fingerprint_recognition

    async def get_employee_biometrics(self, employee_id: int) -> EmployeeBiometricsResponse:
        stmt = select(Biometric.type, func.count()).where(Biometric.employee_id == employee_id).group_by(Biometric.type)
        result = await self._session.execute(stmt)

        counts = {b_type: count for b_type, count in result.tuples().all()}

        return EmployeeBiometricsResponse(
            employee_id=employee_id,
            total_face_biometrics=counts.get(DBBiometricType.FACE, 0),
            total_fingerprint_biometrics=counts.get(DBBiometricType.FINGERPRINT, 0)
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
                type=DBBiometricType.FACE.value,
                data=np.array(embedding, dtype=np.float32).tobytes()
            )

            self._session.add(biometric)

            self._face_recognition.add_face_embedding(employee_id, embedding)

            success_count += 1
            
        await self._session.commit()

        return success_count

    async def add_fingerprint_data(self, employee_id: int, fingerprint_datas: list[str]) -> int:
        success_count = 0

        for fingerprint_data_b64 in fingerprint_datas:
            fingerprint_data = base64.b64decode(fingerprint_data_b64)

            data = self._fingerprint_recognition.extract_fingerprint_data(fingerprint_data)
            if data is None:
                continue

            biometric = Biometric(
                employee_id=employee_id,
                type=DBBiometricType.FINGERPRINT.value,
                data=data.to_iso19794_2()
            )

            self._session.add(biometric)

            self._fingerprint_recognition.add_fingerprint_template(employee_id, data.to_iso19794_2())

            success_count += 1
            
        await self._session.commit()
        return success_count

    async def clear_biometric_data(self, employee_id: int, type: DBBiometricType) -> int:
        stmt = delete(Biometric).where(Biometric.employee_id == employee_id, Biometric.type == type.value)

        result = await self._session.execute(stmt)
    
        count = result.rowcount
        await self._session.commit()

        if type == DBBiometricType.FACE:
            self._face_recognition.remove_face_embeddings(employee_id)
        elif type == DBBiometricType.FINGERPRINT:
            self._fingerprint_recognition.remove_fingerprint_templates(employee_id)

        return count

    async def get_all_biometric_data(self, employee_id: Optional[int] = None) -> list[Biometric]:
        stmt = select(Biometric)

        if employee_id is not None:
            stmt = stmt.where(Biometric.employee_id == employee_id)

        result = await self._session.execute(stmt)
        return list(result.scalars().all())

    async def get_biometric_data(self, employee_id: int, type: DBBiometricType) -> list[Biometric]:
        stmt = select(Biometric).where(Biometric.employee_id == employee_id, Biometric.type == type.value)

        result = await self._session.execute(stmt)
        return list(result.scalars().all())