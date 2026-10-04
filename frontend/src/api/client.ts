import axios from 'axios';
import toast from 'react-hot-toast';

export const apiClient = axios.create({
    baseURL: 'http://localhost:8000/api',
    headers: {
        'Content-Type': 'application/json',
    },
});

apiClient.interceptors.response.use(
    (response) => {
        if (response.data && response.data.success === false) {
            toast.error(response.data.message || 'Thao tác không thành công');
        }
        return response;
    },
    (error) => {
        const message = error.response?.data?.message || 'Lỗi kết nối đến máy chủ. Vui lòng thử lại sau.';
        toast.error(message);
        return Promise.reject(error);
    }
);