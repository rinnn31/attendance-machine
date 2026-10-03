from __future__ import annotations
from typing import TYPE_CHECKING
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy import Integer, String

from app.models.base import Base

if TYPE_CHECKING:
    from app.models.biometric import Biometric
    from app.models.attendance_record import AttendanceRecord

class Employee(Base):
    __tablename__ = "employees"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    full_name: Mapped[str] = mapped_column(String(60), nullable=False)
    phone_number: Mapped[str] = mapped_column(String(15), nullable=False, unique=True)
    email: Mapped[str] = mapped_column(String(100), nullable=False, unique=True)
    address: Mapped[str] = mapped_column(String(150), nullable=False)

    biometrics: Mapped[list[Biometric]] = relationship("Biometric", back_populates="employee", cascade="all, delete-orphan")
    attendance_records: Mapped[list[AttendanceRecord]] = relationship("AttendanceRecord", back_populates="employee", cascade="all, delete-orphan")
