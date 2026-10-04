import { apiClient } from './client';
import type { ApiResponse } from '../types';

export const deviceApi = {
    getDevices: async (): Promise<ApiResponse<string[]>> => {
        const response = await apiClient.get('/devices/');
        return response.data;
    },

    collectData: async (deviceId: string, type: string): Promise<ApiResponse<any>> => {
        const response = await apiClient.post(`/devices/${deviceId}/collect/${type}`);
        return response.data;
    },

    cancelCollect: async (deviceId: string): Promise<ApiResponse<any>> => {
        const response = await apiClient.post(`/devices/${deviceId}/cancel_collect`);
        return response.data;
    }
};