export interface ApiResponse<T> {
    success: boolean;
    message?: string;
    data?: T;
}

export interface Employee {
    id: number;
    full_name: string;
    phone_number: string;
    email: string;
    address: string;
}

export interface CreateEmployeeRequest {
    full_name: string;
    phone_number: string;
    email: string;
    address: string;
}

export interface EmployeeBiometricsResponse {
    employee_id: number;
    total_face_biometrics: number;
    total_fingerprint_biometrics: number;
}

export interface AddBiometricRequest {
    employee_id: number;
    biometric_type: 'FACE' | 'FINGERPRINT';
    biometric_datas: string[];
}

export interface ByDateAttendanceDetailResponse {
    employee_id: number;
    full_name: string;
    device_id: string;
    timestamp: number;
    biometric_type: string;
}

export interface ByEmployeeAttendanceDetailResponse {
    timestamp: number;
    device_id: string;
    biometric_type: string;
}