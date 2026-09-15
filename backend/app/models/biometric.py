from __future__ import annotations
from typing import TYPE_CHECKING
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy import Enum, ForeignKey
import enum

from app.models.base import Base

if TYPE_CHECKING:
    from app.models.employee import Employee

class BiometricType(str, enum.Enum):
    FINGERPRINT = "fingerprint"
    FACE = "face"

class Biometric(Base):
    __tablename__ = "biometrics"

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    employee_id: Mapped[int] = mapped_column(ForeignKey("employees.id"), nullable=False)
    data: Mapped[bytes] = mapped_column(nullable=False)
    type: Mapped[BiometricType] = mapped_column(Enum(BiometricType), nullable=False)

    employee: Mapped[Employee] = relationship("Employee", back_populates="biometrics")
