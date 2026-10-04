import { apiClient } from './client';
import type { ApiResponse, ByDateAttendanceDetailResponse, ByEmployeeAttendanceDetailResponse } from '../types';

export const attendanceApi = {
    getByDate: async (date: string): Promise<ApiResponse<ByDateAttendanceDetailResponse[]>> => {
        const response = await apiClient.get('/attendances/', {
            params: { by: 'date', date }
        });
        return response.data;
    },

    getByEmployee: async (employeeId: number, startDate: string, endDate: string): Promise<ApiResponse<ByEmployeeAttendanceDetailResponse[]>> => {
        const response = await apiClient.get('/attendances/', {
            params: { by: 'employee', employee_id: employeeId, start_date: startDate, end_date: endDate }
        });
        return response.data;
    }
};