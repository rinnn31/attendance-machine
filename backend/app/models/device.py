from sqlalchemy import Column, Integer, String, ForeignKey

from app.models.base import Base

class Device(Base):
    __tablename__ = "devices"

    id = Column(Integer, primary_key=True, autoincrement=True)
    device_id = Column(String(50), unique=True, nullable=False)
    device_auth_key = Column(String(255), nullable=True)