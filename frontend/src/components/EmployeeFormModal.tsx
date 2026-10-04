import React, { useState, useEffect } from 'react';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import { faTimes, faSave } from '@fortawesome/free-solid-svg-icons';
import toast from 'react-hot-toast';
import { employeeApi } from '../api/employeeApi';
import type { Employee, CreateEmployeeRequest } from '../types';

interface Props {
    employee: Employee | null;
    isOpen: boolean;
    onClose: () => void;
    onSuccess: () => void;
}

export const EmployeeFormModal: React.FC<Props> = ({ employee, isOpen, onClose, onSuccess }) => {
    const [formData, setFormData] = useState<CreateEmployeeRequest>({
        full_name: '',
        phone_number: '',
        email: '',
        address: ''
    });
    const [isSubmitting, setIsSubmitting] = useState(false);

    useEffect(() => {
        if (employee) {
            setFormData({
                full_name: employee.full_name,
                phone_number: employee.phone_number,
                email: employee.email,
                address: employee.address
            });
        } else {
            setFormData({ full_name: '', phone_number: '', email: '', address: '' });
        }
    }, [employee]);

    const handleSubmit = async (e: React.FormEvent) => {
        e.preventDefault();
        setIsSubmitting(true);
        try {
            if (employee) {
                const res = await employeeApi.updateEmployee(employee.id, formData);
                if (res.success) {
                    toast.success('Cập nhật thông tin thành công!');
                    onSuccess();
                }
            } else {
                const res = await employeeApi.createEmployee(formData);
                if (res.success) {
                    toast.success('Thêm nhân viên mới thành công!');
                    onSuccess();
                }
            }
        } catch (error) {
            console.error(error);
        } finally {
            setIsSubmitting(false);
        }
    };

    if (!isOpen) return null;

    return (
        <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-gray-500/30 backdrop-blur-xs">
            <div className="bg-white rounded-2xl shadow-2xl w-full max-w-md border border-gray-100 transform transition-all">
                <div className="flex justify-between items-center px-6 py-4 border-b border-gray-100">
                    <h2 className="text-xl font-bold text-gray-800">{employee ? 'Cập Nhật Nhân Viên' : 'Thêm Nhân Viên'}</h2>
                    <button onClick={onClose} className="text-gray-400 hover:text-red-500 bg-gray-50 hover:bg-red-50 w-8 h-8 rounded-full flex items-center justify-center transition-all">
                        <FontAwesomeIcon icon={faTimes} />
                    </button>
                </div>

                <form onSubmit={handleSubmit} className="p-6 space-y-4">
                    <div>
                        <label className="block text-sm font-semibold text-gray-700 mb-1.5">Họ và tên</label>
                        <input required type="text" value={formData.full_name} onChange={e => setFormData({ ...formData, full_name: e.target.value })} className="w-full border border-gray-200 rounded-xl p-2.5 focus:ring-2 focus:ring-blue-500 focus:border-transparent outline-none transition-all bg-gray-50 focus:bg-white text-gray-700" />
                    </div>
                    <div>
                        <label className="block text-sm font-semibold text-gray-700 mb-1.5">Số điện thoại</label>
                        <input required type="text" value={formData.phone_number} onChange={e => setFormData({ ...formData, phone_number: e.target.value })} className="w-full border border-gray-200 rounded-xl p-2.5 focus:ring-2 focus:ring-blue-500 focus:border-transparent outline-none transition-all bg-gray-50 focus:bg-white text-gray-700" />
                    </div>
                    <div>
                        <label className="block text-sm font-semibold text-gray-700 mb-1.5">Email</label>
                        <input required type="email" value={formData.email} onChange={e => setFormData({ ...formData, email: e.target.value })} className="w-full border border-gray-200 rounded-xl p-2.5 focus:ring-2 focus:ring-blue-500 focus:border-transparent outline-none transition-all bg-gray-50 focus:bg-white text-gray-700" />
                    </div>
                    <div>
                        <label className="block text-sm font-semibold text-gray-700 mb-1.5">Địa chỉ</label>
                        <input required type="text" value={formData.address} onChange={e => setFormData({ ...formData, address: e.target.value })} className="w-full border border-gray-200 rounded-xl p-2.5 focus:ring-2 focus:ring-blue-500 focus:border-transparent outline-none transition-all bg-gray-50 focus:bg-white text-gray-700" />
                    </div>

                    <div className="flex justify-end pt-4 border-t border-gray-50 mt-6">
                        <button disabled={isSubmitting} type="submit" className="bg-blue-600 hover:bg-blue-700 disabled:bg-blue-300 text-white px-6 py-2.5 rounded-xl flex items-center gap-2 font-semibold shadow-sm transition-all">
                            <FontAwesomeIcon icon={faSave} /> {employee ? 'Lưu thay đổi' : 'Tạo mới'}
                        </button>
                    </div>
                </form>
            </div>
        </div>
    );
};