import React, { useEffect, useState } from 'react';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import { faPlus, faEdit, faTrash, faFingerprint, faCamera } from '@fortawesome/free-solid-svg-icons';
import { employeeApi } from '../api/employeeApi';
import { biometricApi } from '../api/biometricApi';
import type { Employee, EmployeeBiometricsResponse } from '../types';
import { BiometricCollectionModal } from '../components/BiometricCollectionModal';
import { EmployeeFormModal } from '../components/EmployeeFormModal';
import toast from 'react-hot-toast';

export const EmployeeList: React.FC = () => {
    const [employees, setEmployees] = useState<Employee[]>([]);
    const [biometrics, setBiometrics] = useState<Record<number, EmployeeBiometricsResponse>>({});
    const [isFormOpen, setIsFormOpen] = useState(false);
    const [isBioModalOpen, setIsBioModalOpen] = useState(false);
    const [selectedEmployee, setSelectedEmployee] = useState<Employee | null>(null);
    const [editingEmployee, setEditingEmployee] = useState<Employee | null>(null);

    const fetchEmployees = async () => {
        const res = await employeeApi.listEmployees();
        if (res.success && res.data) {
            setEmployees(res.data);
            res.data.forEach(emp => fetchBiometrics(emp.id));
        }
    };

    const fetchBiometrics = async (id: number) => {
        const res = await biometricApi.getEmployeeBiometrics(id);
        if (res.success && res.data) {
            setBiometrics(prev => ({ ...prev, [id]: res.data! }));
        }
    };

    useEffect(() => {
        fetchEmployees();
    }, []);

    const handleDelete = async (id: number) => {
        if (window.confirm('Xác nhận xóa nhân viên này?')) {
            try {
                const res = await employeeApi.deleteEmployee(id);
                if (res.success) {
                    toast.success('Xóa nhân viên thành công');
                    fetchEmployees();
                }
            } catch (error) {
                console.error(error);
            }
        }
    };

    const handleDeleteBiometric = async (employeeId: number, type: 'face' | 'fingerprint') => {
        if (window.confirm('Xác nhận xóa dữ liệu sinh trắc học này?')) {
            try {
                const res = await biometricApi.deleteBiometric(employeeId, type);
                if (res.success) {
                    toast.success('Xóa dữ liệu sinh trắc học thành công');
                    fetchBiometrics(employeeId);
                }
            } catch (error) {
                console.error(error);
            }
        }
    };

    return (
        <div className="p-6 max-w-7xl mx-auto">
            <div className="flex justify-between items-center mb-6">
                <h1 className="text-2xl font-bold text-gray-800">Quản Lý Nhân Viên</h1>
                <button
                    onClick={() => { setEditingEmployee(null); setIsFormOpen(true); }}
                    className="bg-blue-600 hover:bg-blue-700 text-white px-4 py-2 rounded-lg font-medium flex items-center gap-2 transition-colors"
                >
                    <FontAwesomeIcon icon={faPlus} /> Thêm Nhân Viên
                </button>
            </div>

            <div className="bg-white rounded-xl shadow-sm border border-gray-100 overflow-hidden">
                <table className="w-full text-left border-collapse">
                    <thead>
                        <tr className="bg-gray-50 border-b border-gray-200">
                            <th className="p-4 text-sm font-semibold text-gray-600">ID</th>
                            <th className="p-4 text-sm font-semibold text-gray-600">Họ Tên</th>
                            <th className="p-4 text-sm font-semibold text-gray-600">Liên hệ</th>
                            <th className="p-4 text-sm font-semibold text-gray-600">Sinh trắc học</th>
                            <th className="p-4 text-sm font-semibold text-gray-600 text-right">Thao tác</th>
                        </tr>
                    </thead>
                    <tbody>
                        {employees.map(emp => (
                            <tr key={emp.id} className="border-b border-gray-100 hover:bg-gray-50 transition-colors">
                                <td className="p-4 text-sm text-gray-800 font-medium">{emp.id}</td>
                                <td className="p-4 text-sm text-gray-800 font-bold">{emp.full_name}</td>
                                <td className="p-4 text-sm text-gray-600">
                                    <div className="flex flex-col gap-1">
                                        <span>{emp.phone_number}</span>
                                        <span className="text-xs text-gray-400">{emp.email}</span>
                                    </div>
                                </td>
                                <td className="p-4 text-sm text-gray-600">
                                    <div className="flex gap-4">
                                        <div className="flex items-center gap-2">
                                            <FontAwesomeIcon icon={faCamera} className={biometrics[emp.id]?.total_face_biometrics > 0 ? "text-green-500" : "text-gray-300"} />
                                            <span className="font-medium">{biometrics[emp.id]?.total_face_biometrics || 0}</span>
                                            {biometrics[emp.id]?.total_face_biometrics > 0 && (
                                                <button onClick={() => handleDeleteBiometric(emp.id, 'face')} className="text-red-400 hover:text-red-600 ml-1">
                                                    <FontAwesomeIcon icon={faTrash} className="text-xs" />
                                                </button>
                                            )}
                                        </div>
                                        <div className="flex items-center gap-2">
                                            <FontAwesomeIcon icon={faFingerprint} className={biometrics[emp.id]?.total_fingerprint_biometrics > 0 ? "text-blue-500" : "text-gray-300"} />
                                            <span className="font-medium">{biometrics[emp.id]?.total_fingerprint_biometrics || 0}</span>
                                            {biometrics[emp.id]?.total_fingerprint_biometrics > 0 && (
                                                <button onClick={() => handleDeleteBiometric(emp.id, 'fingerprint')} className="text-red-400 hover:text-red-600 ml-1">
                                                    <FontAwesomeIcon icon={faTrash} className="text-xs" />
                                                </button>
                                            )}
                                        </div>
                                        <button
                                            onClick={() => { setSelectedEmployee(emp); setIsBioModalOpen(true); }}
                                            className="ml-2 bg-gray-100 hover:bg-gray-200 text-gray-700 px-2 py-1 rounded text-xs font-medium transition-colors"
                                        >
                                            <FontAwesomeIcon icon={faPlus} />
                                        </button>
                                    </div>
                                </td>
                                <td className="p-4 text-sm text-right space-x-3">
                                    <button onClick={() => { setEditingEmployee(emp); setIsFormOpen(true); }} className="text-blue-500 hover:text-blue-700 transition-colors">
                                        <FontAwesomeIcon icon={faEdit} />
                                    </button>
                                    <button onClick={() => handleDelete(emp.id)} className="text-red-500 hover:text-red-700 transition-colors">
                                        <FontAwesomeIcon icon={faTrash} />
                                    </button>
                                </td>
                            </tr>
                        ))}
                    </tbody>
                </table>
            </div>

            {isFormOpen && (
                <EmployeeFormModal
                    employee={editingEmployee}
                    isOpen={isFormOpen}
                    onClose={() => setIsFormOpen(false)}
                    onSuccess={() => { setIsFormOpen(false); fetchEmployees(); }}
                />
            )}

            {isBioModalOpen && selectedEmployee && (
                <BiometricCollectionModal
                    employeeId={selectedEmployee.id}
                    isOpen={isBioModalOpen}
                    onClose={() => { setIsBioModalOpen(false); fetchBiometrics(selectedEmployee.id); }}
                />
            )}
        </div>
    );
};