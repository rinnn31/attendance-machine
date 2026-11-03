import { apiClient } from './client';
import type { ApiResponse } from '../types';

export const deviceApi = {
    getOnlineDevices: async (): Promise<ApiResponse<string[]>> => {
        const response = await apiClient.get('/devices/active');
        return response.data;
    },

    collectData: async (deviceId: string, type: string): Promise<ApiResponse<any>> => {
        const response = await apiClient.post(`/devices/${deviceId}/collect/${type}`);
        return response.data;
    },

    cancelCollect: async (deviceId: string): Promise<ApiResponse<any>> => {
        const response = await apiClient.post(`/devices/${deviceId}/cancel_collect`);
        return response.data;
    },

    getAllDevices: async (): Promise<ApiResponse<{ [deviceId: string]: string }>> => {
        const response = await apiClient.get('/devices');
        return response.data;
    },

    approveDevice: async (deviceId: string): Promise<ApiResponse<any>> => {
        const response = await apiClient.post(`/devices/${deviceId}/approve`);
        return response.data;
    },

    rejectDevice: async (deviceId: string): Promise<ApiResponse<any>> => {
        const response = await apiClient.post(`/devices/${deviceId}/reject`);
        return response.data;
    },

    forgetDevice: async (deviceId: string): Promise<ApiResponse<any>> => {
        const response = await apiClient.delete(`/devices/${deviceId}/forget`);
        return response.data;
    }
};