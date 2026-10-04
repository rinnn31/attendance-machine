import { apiClient } from './client';
import type { ApiResponse, Employee, CreateEmployeeRequest } from '../types';

export const employeeApi = {
    getEmployee: async (id: number): Promise<ApiResponse<Employee>> => {
        const response = await apiClient.get(`/employees/${id}`);
        return response.data;
    },

    listEmployees: async (): Promise<ApiResponse<Employee[]>> => {
        const response = await apiClient.get('/employees/');
        return response.data;
    },

    createEmployee: async (data: CreateEmployeeRequest): Promise<ApiResponse<null>> => {
        const response = await apiClient.post('/employees/', data);
        return response.data;
    },

    updateEmployee: async (id: number, data: CreateEmployeeRequest): Promise<ApiResponse<null>> => {
        const response = await apiClient.put(`/employees/${id}`, data);
        return response.data;
    },

    deleteEmployee: async (id: number): Promise<ApiResponse<null>> => {
        const response = await apiClient.delete(`/employees/${id}`);
        return response.data;
    }
};