from __future__ import annotations
from typing import TYPE_CHECKING
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy import Enum, ForeignKey, String, Integer, TIMESTAMP
from datetime import datetime

from app.models.base import Base
from app.models.biometric import DBBiometricType

if TYPE_CHECKING:
    from app.models.employee import Employee

class AttendanceRecord(Base):
    __tablename__ = "attendance_records"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    employee_id: Mapped[int] = mapped_column(Integer, ForeignKey("employees.id"), nullable=False)
    timestamp: Mapped[datetime] = mapped_column(TIMESTAMP, nullable=False)
    biometric_type: Mapped[DBBiometricType] = mapped_column(Enum(DBBiometricType), nullable=False)
    device_id: Mapped[str] = mapped_column(String(50), nullable=False)

    employee: Mapped[Employee] = relationship("Employee", back_populates="attendance_records")
