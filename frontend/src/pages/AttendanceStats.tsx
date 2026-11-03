import React, { useState, useEffect, useRef } from 'react';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import { faSearch, faCalendarDay, faUserClock, faUser } from '@fortawesome/free-solid-svg-icons';
import { attendanceApi } from '../api/attendanceApi';
import { apiClient } from '../api/client';
import type { ByDateAttendanceDetailResponse } from '../types';

const getTodayDateString = () => new Date().toISOString().split('T')[0];

const formatTimestamp = (timestamp: number) => {
    const d = new Date(timestamp * 1000);
    if (isNaN(d.getTime())) return 'Invalid Date';
    const pad = (n: number) => n.toString().padStart(2, '0');
    return `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())} ${pad(d.getDate())}/${pad(d.getMonth() + 1)}/${d.getFullYear()}`;
};

export const AttendanceStats: React.FC = () => {
    const [filterType, setFilterType] = useState<'date' | 'employee'>('date');
    const [date, setDate] = useState(getTodayDateString());
    const [startDate, setStartDate] = useState(getTodayDateString());
    const [endDate, setEndDate] = useState(getTodayDateString());
    const [results, setResults] = useState<any[]>([]);

    const [employeeSearch, setEmployeeSearch] = useState('');
    const [selectedEmployeeId, setSelectedEmployeeId] = useState<number | null>(null);
    const [employeeOptions, setEmployeeOptions] = useState<{ id: number, full_name: string }[]>([]);
    const [showDropdown, setShowDropdown] = useState(false);
    const dropdownRef = useRef<HTMLDivElement>(null);

    const filterStateRef = useRef({ filterType, date, selectedEmployeeId, startDate, endDate });

    useEffect(() => {
        filterStateRef.current = { filterType, date, selectedEmployeeId, startDate, endDate };
    }, [filterType, date, selectedEmployeeId, startDate, endDate]);

    useEffect(() => {
        const handleClickOutside = (e: MouseEvent) => {
            if (dropdownRef.current && !dropdownRef.current.contains(e.target as Node)) setShowDropdown(false);
        };
        document.addEventListener('mousedown', handleClickOutside);
        return () => document.removeEventListener('mousedown', handleClickOutside);
    }, []);

    useEffect(() => {
        const fetchEmployees = async () => {
            if (!employeeSearch.trim()) {
                setEmployeeOptions([]);
                return;
            }
            try {
                const res = await apiClient.get('/employees', { params: { find: employeeSearch } });
                if (res.data?.success) setEmployeeOptions(res.data.data);
            } catch (e) {
                console.error('Lỗi tìm kiếm nhân viên');
            }
        };
        const debounce = setTimeout(fetchEmployees, 300);
        return () => clearTimeout(debounce);
    }, [employeeSearch]);

    const handleFilterTypeChange = (type: 'date' | 'employee') => {
        setFilterType(type);
        setDate(getTodayDateString());
        setStartDate(getTodayDateString());
        setEndDate(getTodayDateString());
        setResults([]);
    };

    const handleSearch = async () => {
        if (filterType === 'date' && date) {
            const res = await attendanceApi.getByDate(date);
            if (res.success) setResults(res.data || []);
        } else if (filterType === 'employee' && selectedEmployeeId && startDate && endDate) {
            const res = await attendanceApi.getByEmployee(selectedEmployeeId, startDate, endDate);
            if (res.success) setResults(res.data || []);
        }
    };

    return (
        <div className="p-6 max-w-7xl mx-auto">
            <div className="bg-white rounded-xl shadow-sm border border-gray-100 p-6 mb-6">
                <div className="flex gap-4 mb-6">
                    <button
                        onClick={() => handleFilterTypeChange('date')}
                        className={`px-5 py-2.5 rounded-lg font-medium flex items-center gap-2 transition-colors ${filterType === 'date' ? 'bg-blue-600 text-white shadow-md' : 'bg-gray-100 text-gray-600 hover:bg-gray-200'}`}
                    >
                        <FontAwesomeIcon icon={faCalendarDay} /> Theo Ngày
                    </button>
                    <button
                        onClick={() => handleFilterTypeChange('employee')}
                        className={`px-5 py-2.5 rounded-lg font-medium flex items-center gap-2 transition-colors ${filterType === 'employee' ? 'bg-blue-600 text-white shadow-md' : 'bg-gray-100 text-gray-600 hover:bg-gray-200'}`}
                    >
                        <FontAwesomeIcon icon={faUserClock} /> Theo Nhân Viên
                    </button>
                </div>

                <div className="flex flex-wrap gap-5 items-end">
                    {filterType === 'date' ? (
                        <div className="flex-1 min-w-50">
                            <label className="block text-sm font-semibold text-gray-700 mb-2">Chọn ngày</label>
                            <input
                                type="date"
                                max={getTodayDateString()}
                                value={date}
                                onChange={e => setDate(e.target.value)}
                                className="w-full border-2 border-gray-200 rounded-xl px-4 py-3 text-base text-gray-800 font-medium focus:ring-4 focus:ring-blue-100 focus:border-blue-500 outline-none transition-all cursor-pointer"
                            />
                        </div>
                    ) : (
                        <>
                            <div className="flex-1 min-w-64 relative" ref={dropdownRef}>
                                <label className="block text-sm font-semibold text-gray-700 mb-2">Tên nhân viên</label>
                                <div className="relative">
                                    <div className="absolute inset-y-0 left-0 pl-4 flex items-center pointer-events-none">
                                        <FontAwesomeIcon icon={faUser} className="text-gray-400" />
                                    </div>
                                    <input
                                        type="text"
                                        placeholder="Nhập tên nhân viên..."
                                        value={employeeSearch}
                                        onChange={e => {
                                            setEmployeeSearch(e.target.value);
                                            setShowDropdown(true);
                                            setSelectedEmployeeId(null);
                                        }}
                                        onFocus={() => setShowDropdown(true)}
                                        className="w-full border-2 border-gray-200 rounded-xl pl-11 pr-4 py-3 text-base text-gray-800 font-medium focus:ring-4 focus:ring-blue-100 focus:border-blue-500 outline-none transition-all"
                                    />
                                </div>
                                {showDropdown && employeeOptions.length > 0 && (
                                    <div className="absolute z-50 mt-2 w-full bg-white border border-gray-100 rounded-xl shadow-xl max-h-60 overflow-y-auto">
                                        {employeeOptions.map(emp => (
                                            <div
                                                key={emp.id}
                                                onClick={() => {
                                                    setSelectedEmployeeId(emp.id);
                                                    setEmployeeSearch(`${emp.id} - ${emp.full_name}`);
                                                    setShowDropdown(false);
                                                }}
                                                className="px-4 py-3 hover:bg-blue-50 cursor-pointer border-b border-gray-50 last:border-none transition-colors"
                                            >
                                                <p className="font-semibold text-gray-800">{emp.full_name}</p>
                                                <p className="text-xs text-gray-500">ID: {emp.id}</p>
                                            </div>
                                        ))}
                                    </div>
                                )}
                            </div>
                            <div className="flex-1 min-w-40">
                                <label className="block text-sm font-semibold text-gray-700 mb-2">Từ ngày</label>
                                <input
                                    type="date"
                                    max={endDate}
                                    value={startDate}
                                    onChange={e => setStartDate(e.target.value)}
                                    className="w-full border-2 border-gray-200 rounded-xl px-4 py-3 text-base text-gray-800 font-medium focus:ring-4 focus:ring-blue-100 focus:border-blue-500 outline-none transition-all cursor-pointer"
                                />
                            </div>
                            <div className="flex-1 min-w-40">
                                <label className="block text-sm font-semibold text-gray-700 mb-2">Đến ngày</label>
                                <input
                                    type="date"
                                    min={startDate}
                                    max={getTodayDateString()}
                                    value={endDate}
                                    onChange={e => setEndDate(e.target.value)}
                                    className="w-full border-2 border-gray-200 rounded-xl px-4 py-3 text-base text-gray-800 font-medium focus:ring-4 focus:ring-blue-100 focus:border-blue-500 outline-none transition-all cursor-pointer"
                                />
                            </div>
                        </>
                    )}

                    <button onClick={handleSearch} className="bg-gray-800 hover:bg-gray-900 text-white px-8 py-3 rounded-xl font-semibold flex items-center justify-center gap-2 transition-all shadow-md">
                        <FontAwesomeIcon icon={faSearch} /> Tra cứu
                    </button>
                </div>
            </div>

            <div className="bg-white rounded-xl shadow-sm border border-gray-100 overflow-hidden">
                <table className="w-full text-left border-collapse">
                    <thead>
                        <tr className="bg-gray-50 border-b border-gray-200">
                            {filterType === 'date' && <th className="p-4 text-lg font-semibold text-gray-600">ID / Tên Nhân Viên</th>}
                            <th className="p-4 text-lg font-semibold text-gray-600">Thời gian</th>
                            <th className="p-4 text-lg font-semibold text-gray-600">Thiết bị</th>
                            <th className="p-4 text-lg font-semibold text-gray-600">Hình thức</th>
                            <th className="p-4 text-lg font-semibold text-gray-600">Hình ảnh</th>
                        </tr>
                    </thead>
                    <tbody>
                        {results.map((record, idx) => (
                            <tr key={idx} className="border-b border-gray-100 hover:bg-gray-50 transition-colors">
                                {filterType === 'date' && (
                                    <td className="p-4 text-lg text-gray-800 font-medium">
                                        {(record as ByDateAttendanceDetailResponse).employee_id} - {(record as ByDateAttendanceDetailResponse).full_name}
                                    </td>
                                )}
                                <td className="p-4 text-lg font-mono text-gray-700">{formatTimestamp(record.timestamp)}</td>
                                <td className="p-4 text-lg text-gray-600">
                                    <span className="bg-blue-100 text-blue-700 px-3 py-1 rounded-md text-lg font-semibold">{record.device_id}</span>
                                </td>
                                <td className="p-4 text-lg text-gray-600">
                                    <span className="bg-gray-100 text-gray-700 px-3 py-1 rounded-md text-lg font-semibold">{record.biometric_type === 'FACE' ? 'Khuôn mặt' : 'Vân tay'}</span>
                                </td>
                                <td className="p-4 text-lg text-gray-600">
                                    {record.face_capture_image ? (
                                        <img src={'http://localhost:8000'+record.face_capture_image} alt="Face Capture" className="w-20 h-20 object-cover rounded-lg border border-gray-200" />
                                    ) : (
                                        <span className="text-gray-400 font-medium">Không có</span>
                                    )}
                                </td>
                            </tr>
                        ))}
                        {results.length === 0 && (
                            <tr>
                                <td colSpan={4} className="p-12 text-center text-gray-500 font-medium">Không có dữ liệu chấm công.</td>
                            </tr>
                        )}
                    </tbody>
                </table>
            </div>
        </div>
    );
};