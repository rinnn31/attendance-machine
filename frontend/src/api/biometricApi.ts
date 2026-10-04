import { apiClient } from './client';
import type { ApiResponse, EmployeeBiometricsResponse, AddBiometricRequest } from '../types';

export const biometricApi = {
    getEmployeeBiometrics: async (employeeId: number): Promise<ApiResponse<EmployeeBiometricsResponse>> => {
        const response = await apiClient.get(`/biometrics/${employeeId}`);
        return response.data;
    },

    addBiometric: async (data: AddBiometricRequest): Promise<ApiResponse<number>> => {
        const response = await apiClient.post('/biometrics/', data);
        return response.data;
    },

    deleteBiometric: async (employeeId: number, type: 'face' | 'fingerprint'): Promise<ApiResponse<null>> => {
        const response = await apiClient.delete(`/biometrics/${employeeId}/${type}`);
        return response.data;
    }
};