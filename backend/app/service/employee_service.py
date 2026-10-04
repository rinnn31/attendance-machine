from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select

from app.models.employee import Employee
from app.schemas import EmployeeResponse, CreateEmployeeRequest, UpdateEmployeeRequest

class EmployeeService:
    def __init__(self, session: AsyncSession):
        self._session = session

    async def get_employee(self, employee_id: int) -> EmployeeResponse | None:
        employee = await self._session.get(Employee, employee_id)
        if employee:
            return EmployeeResponse(
                id=employee.id,
                full_name=employee.full_name,
                phone_number=employee.phone_number,
                email=employee.email,
                address=employee.address
            )
        return None

    async def list_employees(self) -> list[EmployeeResponse]:
        result = await self._session.execute(select(Employee))
        employees = result.scalars().all()
        return [
            EmployeeResponse(
                id=employee.id,
                full_name=employee.full_name,
                phone_number=employee.phone_number,
                email=employee.email,
                address=employee.address
            )
            for employee in employees
        ]

    async def create_employee(self, employee_data: CreateEmployeeRequest):
        new_employee = Employee(
            full_name=employee_data.full_name,
            phone_number=employee_data.phone_number,
            email=employee_data.email,
            address=employee_data.address
        )
        self._session.add(new_employee)
        await self._session.commit()
        await self._session.refresh(new_employee)

    async def update_employee(self, employee_id: int, employee_data: UpdateEmployeeRequest) -> bool:
        employee = await self._session.get(Employee, employee_id)
        if not employee:
            return False

        if employee_data.full_name is not None:
            employee.full_name = employee_data.full_name
        if employee_data.phone_number is not None:
            employee.phone_number = employee_data.phone_number
        if employee_data.email is not None:
            employee.email = employee_data.email

        await self._session.commit()
        return True

    async def delete_employee(self, employee_id: int) -> bool:
        employee = await self._session.get(Employee, employee_id)
        if not employee:
            return False

        await self._session.delete(employee)
        await self._session.commit()
        return True