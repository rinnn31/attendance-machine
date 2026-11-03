import React, { useEffect, useState } from 'react';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import { 
    faServer, 
    faCheck, 
    faTimes, 
    faTrash,
    faRefresh,
    faMobileAlt
} from '@fortawesome/free-solid-svg-icons';
import { deviceApi } from '../api/deviceApi';
import toast from 'react-hot-toast';

export const DeviceList: React.FC = () => {
    const [allDevices, setAllDevices] = useState<{ [deviceId: string]: string }>({});
    const [loading, setLoading] = useState<boolean>(false);

    const fetchDeviceData = async () => {
        setLoading(true);
        try {
            const [allRes] = await Promise.all([
                deviceApi.getAllDevices(),
                deviceApi.getOnlineDevices()
            ]);

            if (allRes.success && allRes.data) {
                setAllDevices(allRes.data);
            }
        } catch (error) {
            console.error(error);
            toast.error('Lỗi khi tải danh sách thiết bị');
        } finally {
            setLoading(false);
        }
    };

    useEffect(() => {
        fetchDeviceData();
    }, []);

    const handleApprove = async (deviceId: string) => {
        try {
            const res = await deviceApi.approveDevice(deviceId);
            if (res.success) {
                toast.success(`Đã phê duyệt thiết bị ${deviceId}`);
                fetchDeviceData();
            }
        } catch (error) {
            console.error(error);
            toast.error('Phê duyệt thất bại');
        }
    };

    const handleReject = async (deviceId: string) => {
        if (window.confirm(`Xác nhận từ chối kết nối thiết bị ${deviceId}?`)) {
            try {
                const res = await deviceApi.rejectDevice(deviceId);
                if (res.success) {
                    toast.success(`Đã từ chối thiết bị ${deviceId}`);
                    fetchDeviceData();
                }
            } catch (error) {
                console.error(error);
                toast.error('Thao tác từ chối thất bại');
            }
        }
    };

    const handleForget = async (deviceId: string) => {
        if (window.confirm(`Xác nhận xóa/quên thiết bị ${deviceId}?`)) {
            try {
                const res = await deviceApi.forgetDevice(deviceId);
                if (res.success) {
                    toast.success(`Đã xóa thiết bị ${deviceId}`);
                    fetchDeviceData();
                }
            } catch (error) {
                console.error(error);
                toast.error('Xóa thiết bị thất bại');
            }
        }
    };

    const deviceEntries = Object.entries(allDevices);

    return (
        <div className="p-6 max-w-7xl mx-auto">
            {/* Header */}
            <div className="flex justify-between items-center mb-6">
                <h1 className="text-2xl font-bold text-gray-800 flex items-center gap-2">
                    <FontAwesomeIcon icon={faServer} className="text-blue-600" /> Quản Lý Thiết Bị Chấm Công
                </h1>
                <button
                    onClick={fetchDeviceData}
                    disabled={loading}
                    className="bg-gray-800 hover:bg-gray-900 text-white px-4 py-2 rounded-lg font-medium flex items-center gap-2 transition-colors disabled:opacity-50"
                >
                    <FontAwesomeIcon icon={faRefresh} className={loading ? 'fa-spin' : ''} /> Làm mới
                </button>
            </div>

            {/* Content Table */}
            <div className="bg-white rounded-xl shadow-sm border border-gray-100 overflow-hidden">
                <table className="w-full text-left border-collapse">
                    <thead>
                        <tr className="bg-gray-50 border-b border-gray-200">
                            <th className="p-4 text-lg font-semibold text-gray-600">ID Thiết Bị</th>
                            <th className="p-4 text-lg font-semibold text-gray-600">Phê Duyệt Server</th>
                            <th className="p-4 text-lg font-semibold text-gray-600 text-right">Thao Tác</th>
                        </tr>
                    </thead>
                    <tbody>
                        {deviceEntries.map(([deviceId, status]) => {
                            const isApproved = status === 'connected'

                            return (
                                <tr key={deviceId} className="border-b border-gray-100 hover:bg-gray-50 transition-colors">
                                    {/* ID Thiết Bị */}
                                    <td className="p-4 text-lg text-gray-800 font-bold">
                                        <div className="flex items-center gap-3">
                                            <div className="w-10 h-10 rounded-lg bg-blue-50 text-blue-600 flex items-center justify-center font-bold text-base">
                                                <FontAwesomeIcon icon={faMobileAlt} />
                                            </div>
                                            <span>{deviceId}</span>
                                        </div>
                                    </td>

                                    {/* Trạng Thái Phê Duyệt */}
                                    <td className="p-4 text-lg">
                                        {isApproved ? (
                                            <span className="bg-blue-100 text-blue-700 px-3 py-1 rounded-md text-base font-semibold">
                                                Đã kết nối
                                            </span>
                                        ) : (
                                            <span className="bg-amber-100 text-amber-700 px-3 py-1 rounded-md text-base font-semibold">
                                                Chờ phê duyệt (Pending)
                                            </span>
                                        )}
                                    </td>

                                    {/* Thao tác */}
                                    <td className="p-4 text-sm text-right space-x-2">
                                        {!isApproved && (
                                            <button
                                                onClick={() => handleApprove(deviceId)}
                                                className="bg-green-600 hover:bg-green-700 text-white px-3 py-1.5 rounded-lg font-medium inline-flex items-center gap-1.5 transition-colors shadow-sm"
                                                title="Chấp nhận kết nối thiết bị"
                                            >
                                                <FontAwesomeIcon icon={faCheck} /> Chấp nhận
                                            </button>
                                        )}

                                        {!isApproved && (
                                            <button
                                                onClick={() => handleReject(deviceId)}
                                                className="bg-amber-500 hover:bg-amber-600 text-white px-3 py-1.5 rounded-lg font-medium inline-flex items-center gap-1.5 transition-colors shadow-sm"
                                                title="Từ chối kết nối"
                                            >
                                                <FontAwesomeIcon icon={faTimes} /> Từ chối
                                            </button>
                                        )}

                                        <button
                                            onClick={() => handleForget(deviceId)}
                                            className="bg-gray-100 hover:bg-red-50 text-red-600 px-3 py-1.5 rounded-lg font-medium inline-flex items-center gap-1.5 transition-colors border border-gray-200"
                                            title="Xóa thiết bị"
                                        >
                                            <FontAwesomeIcon icon={faTrash} /> Xóa thiết bị
                                        </button>
                                    </td>
                                </tr>
                            );
                        })}

                        {deviceEntries.length === 0 && (
                            <tr>
                                <td colSpan={4} className="p-12 text-center text-gray-500 font-medium">
                                    Chưa có thiết bị nào trong hệ thống.
                                </td>
                            </tr>
                        )}
                    </tbody>
                </table>
            </div>
        </div>
    );
};