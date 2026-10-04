from __future__ import annotations
from typing import TYPE_CHECKING
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy import Enum, ForeignKey, Integer, LargeBinary
import enum

from app.models.base import Base

if TYPE_CHECKING:
    from app.models.employee import Employee

class DBBiometricType(str, enum.Enum):
    FINGERPRINT = "FINGERPRINT"
    FACE = "FACE"

class Biometric(Base):
    __tablename__ = "biometrics"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    employee_id: Mapped[int] = mapped_column(Integer, ForeignKey("employees.id"), nullable=False)
    data: Mapped[bytes] = mapped_column(LargeBinary, nullable=False)
    type: Mapped[DBBiometricType] = mapped_column(Enum(DBBiometricType), nullable=False)

    employee: Mapped[Employee] = relationship("Employee", back_populates="biometrics")
