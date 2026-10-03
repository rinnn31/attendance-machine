from fastapi import APIRouter, Depends

from app.api.deps import get_database
from app.schemas import Response, EmployeeResponse, CreateEmployeeRequest
from app.service.employee_service import EmployeeService

router = APIRouter(tags=["employee"], prefix="/employees")

@router.get(path="/{employee_id}", response_model=Response[EmployeeResponse])
async def get_employee(employee_id: int, db=Depends(get_database)) -> Response[EmployeeResponse]:
    employee_service = EmployeeService(db)
    return Response.success(await employee_service.get_employee(employee_id))

@router.get(path="/", response_model=Response[list[EmployeeResponse]])
async def list_employees(db=Depends(get_database)) -> Response[list[EmployeeResponse]]:
    employee_service = EmployeeService(db)
    return Response.success(await employee_service.list_employees())

@router.post(path="/")
async def create_employee(employee_data: CreateEmployeeRequest, db=Depends(get_database)):
    employee_service = EmployeeService(db)
    await employee_service.create_employee(employee_data)
    return Response.success()

@router.put(path="/{employee_id}")
async def update_employee(employee_id: int, employee_data: CreateEmployeeRequest, db=Depends(get_database)):
    employee_service = EmployeeService(db)
    await employee_service.update_employee(employee_id, employee_data)
    return Response.success()

@router.delete(path="/{employee_id}")
async def delete_employee(employee_id: int, db=Depends(get_database)):
    employee_service = EmployeeService(db)
    await employee_service.delete_employee(employee_id)
    return Response.success()
