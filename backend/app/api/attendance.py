from typing import Optional
from fastapi import APIRouter, Depends
from sqlalchemy.ext.asyncio import AsyncSession

from app.schemas import ByEmployeeAttendanceDetailResponse, ByDateAttendanceDetailResponse, Response
from app.api.deps import get_database
from app.service.attendance_service import AttendanceService

router = APIRouter(tags=["attendance"], prefix="/attendances")

@router.get(path="/")
async def get_attendance_info(
    by: str = "employee",
    # For by="employee" option
    employee_id: Optional[int] = None,
    start_date: Optional[str] = None,
    end_date: Optional[str] = None,
    # For by="date" option
    date: Optional[str] = None,

    db: AsyncSession = Depends(get_database)
): 
    attendance_service = AttendanceService(db)
    
    if by == "employee":
        if employee_id is None or start_date is None or end_date is None:
            return Response.make_error("Missing required parameters for 'by=employee' option. Required: employee_id, start_date, end_date.")
        attendance_details = await attendance_service.get_employee_attendance(employee_id, start_date, end_date)
    elif by == "date":
        if date is None:
            return Response.make_error("Missing required parameter for 'by=date' option. Required: date.")
        attendance_details = await attendance_service.get_date_attendance(date)
    else:
        return Response.make_error(message="Invalid 'by' parameter. Must be either 'employee' or 'date'.")

    return Response.make_success(attendance_details)
    