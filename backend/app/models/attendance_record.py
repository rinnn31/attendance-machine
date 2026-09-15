from __future__ import annotations
from typing import TYPE_CHECKING
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy import Enum, ForeignKey

from app.models.base import Base
from app.models.biometric import BiometricType

if TYPE_CHECKING:
    from app.models.employee import Employee\

class AttendanceRecord(Base):
    __tablename__ = "attendance_records"

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    employee_id: Mapped[int] = mapped_column(ForeignKey("employees.id"), nullable=False)
    timestamp: Mapped[int] = mapped_column(nullable=False)
    biometric_type: Mapped[BiometricType] = mapped_column(Enum(BiometricType), nullable=False)

    employee: Mapped[Employee] = relationship("Employee", back_populates="attendance_records")
