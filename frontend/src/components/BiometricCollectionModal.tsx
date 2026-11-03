import React, { useState, useEffect, useRef } from 'react';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import {
    faUpload,
    faMicrochip,
    faCamera,
    faFingerprint,
    faSyncAlt,
    faSpinner,
    faTimes,
    faCloudUploadAlt,
    faChevronDown,
    faCheck,
    faServer
} from '@fortawesome/free-solid-svg-icons';
import toast from 'react-hot-toast';
import { deviceApi } from '../api/deviceApi';
import { biometricApi } from '../api/biometricApi';

interface Props {
    employeeId: number;
    isOpen: boolean;
    onClose: () => void;
}

interface DropdownOption<T> {
    label: string;
    value: T;
    icon?: any;
}

function CustomSelect<T extends string>({
    label,
    options,
    value,
    onChange,
    disabled = false,
    placeholder = 'Chọn một mục'
}: {
    label?: string;
    options: DropdownOption<T>[];
    value: T;
    onChange: (val: T) => void;
    disabled?: boolean;
    placeholder?: string;
}) {
    const [isOpen, setIsOpen] = useState(false);
    const containerRef = useRef<HTMLDivElement>(null);

    const selectedOption = options.find(opt => opt.value === value);

    useEffect(() => {
        const handleClickOutside = (event: MouseEvent) => {
            if (containerRef.current && !containerRef.current.contains(event.target as Node)) {
                setIsOpen(false);
            }
        };
        document.addEventListener('mousedown', handleClickOutside);
        return () => document.removeEventListener('mousedown', handleClickOutside);
    }, []);

    return (
        <div className="relative w-full" ref={containerRef}>
            {label && <label className="block text-sm font-semibold text-gray-700 mb-2">{label}</label>}
            <button
                type="button"
                disabled={disabled}
                onClick={() => setIsOpen(!isOpen)}
                className={`w-full flex items-center justify-between px-4 py-3 bg-gray-50/70 border border-gray-200 rounded-xl text-left transition-all duration-200 ${disabled
                        ? 'opacity-60 cursor-not-allowed bg-gray-100'
                        : 'hover:bg-white hover:border-blue-400 focus:outline-none focus:ring-4 focus:ring-blue-100 focus:bg-white focus:border-blue-500'
                    } ${isOpen ? 'bg-white border-blue-500 ring-4 ring-blue-100 shadow-sm' : ''}`}
            >
                <div className="flex items-center gap-3 overflow-hidden">
                    {selectedOption?.icon && (
                        <FontAwesomeIcon icon={selectedOption.icon} className="text-blue-600 text-base shrink-0" />
                    )}
                    <span className={`text-sm font-medium truncate ${selectedOption ? 'text-gray-800' : 'text-gray-400'}`}>
                        {selectedOption ? selectedOption.label : placeholder}
                    </span>
                </div>
                <FontAwesomeIcon
                    icon={faChevronDown}
                    className={`text-gray-400 text-xs transition-transform duration-200 shrink-0 ml-2 ${isOpen ? 'rotate-180 text-blue-500' : ''}`}
                />
            </button>

            {isOpen && (
                <div className="absolute z-50 left-0 right-0 mt-2 bg-white border border-gray-100 rounded-2xl shadow-xl py-1.5 max-h-56 overflow-y-auto animate-in fade-in zoom-in-95 duration-150">
                    {options.length === 0 ? (
                        <div className="px-4 py-3 text-sm text-gray-400 text-center">Không có lựa chọn nào</div>
                    ) : (
                        options.map((opt) => {
                            const isSelected = opt.value === value;
                            return (
                                <button
                                    key={opt.value}
                                    type="button"
                                    onClick={() => {
                                        onChange(opt.value);
                                        setIsOpen(false);
                                    }}
                                    className={`w-full flex items-center justify-between px-4 py-2.5 text-sm font-medium transition-colors ${isSelected
                                            ? 'bg-blue-50/80 text-blue-700 font-semibold'
                                            : 'text-gray-700 hover:bg-gray-50 hover:text-gray-900'
                                        }`}
                                >
                                    <div className="flex items-center gap-3">
                                        {opt.icon && (
                                            <FontAwesomeIcon
                                                icon={opt.icon}
                                                className={`text-base ${isSelected ? 'text-blue-600' : 'text-gray-400'}`}
                                            />
                                        )}
                                        <span>{opt.label}</span>
                                    </div>
                                    {isSelected && <FontAwesomeIcon icon={faCheck} className="text-blue-600 text-xs" />}
                                </button>
                            );
                        })
                    )}
                </div>
            )}
        </div>
    );
}

export const BiometricCollectionModal: React.FC<Props> = ({ employeeId, isOpen, onClose }) => {
    const [activeTab, setActiveTab] = useState<'UPLOAD' | 'DEVICE'>('UPLOAD');

    const [devices, setDevices] = useState<string[]>([]);
    const [selectedDevice, setSelectedDevice] = useState<string>('');
    const [isCollecting, setIsCollecting] = useState<boolean>(false);
    const [isLoadingDevices, setIsLoadingDevices] = useState<boolean>(false);
    const [deviceBiometricType, setDeviceBiometricType] = useState<'FACE' | 'FINGERPRINT'>('FACE');
    const [devicePreviews, setDevicePreviews] = useState<string[]>([]);

    const [uploadBiometricType, setUploadBiometricType] = useState<'FACE' | 'FINGERPRINT'>('FACE');
    const [uploadFiles, setUploadFiles] = useState<{ name: string, base64: string }[]>([]);
    const [isUploading, setIsUploading] = useState<boolean>(false);
    const fileInputRef = useRef<HTMLInputElement>(null);

    const biometricTypeOptions: DropdownOption<'FACE' | 'FINGERPRINT'>[] = [
        { label: 'Khuôn mặt (Face ID)', value: 'FACE', icon: faCamera },
        { label: 'Dấu vân tay (Fingerprint)', value: 'FINGERPRINT', icon: faFingerprint }
    ];

    const fetchDevices = async () => {
        setIsLoadingDevices(true);
        try {
            const res = await deviceApi.getOnlineDevices();
            if (res.success && res.data) {
                setDevices(res.data);
                if (res.data.length > 0) {
                    setSelectedDevice(prev => (prev && res.data!.includes(prev) ? prev : res.data![0]));
                }
            }
        } catch {
            toast.error('Không thể tải danh sách thiết bị');
        } finally {
            setIsLoadingDevices(false);
        }
    };

    useEffect(() => {
        if (isOpen) {
            fetchDevices();
        } else {
            setIsCollecting(false);
            setIsUploading(false);
            setUploadFiles([]);
            setDevicePreviews([]);
            if (fileInputRef.current) {
                fileInputRef.current.value = '';
            }
        }
    }, [isOpen]);

    const handleToggleDeviceCollect = async () => {
        if (isCollecting) {
            if (selectedDevice) {
                try {
                    await deviceApi.cancelCollect(selectedDevice);
                    toast.success('Đã hủy quá trình thu thập trên thiết bị');
                } catch (error) {
                    console.error(error);
                } finally {
                    setIsCollecting(false);
                }
            }
        } else {
            if (!selectedDevice) {
                toast.error('Vui lòng chọn thiết bị');
                return;
            }

            setIsCollecting(true);

            try {
                const res = await deviceApi.collectData(selectedDevice, deviceBiometricType);
                if (res.success && res.data) {
                    setDevicePreviews(prev => [...prev, res.data]);
                }
            } catch (error) {
                console.error(error);
                //toast.error('Thu thập thất bại. Thiết bị có thể đã bị hủy hoặc mất kết nối!');
            } finally {
                setIsCollecting(false);
            }
        }
    };

    const handleConfirmPreview = async () => {
        if (devicePreviews.length === 0) return;
        try {
            let dataToSend = devicePreviews.map(preview => {
                if (preview.startsWith('data:image')) {
                    return preview.split(',')[1];
                } else {
                    return preview;
                }
            })
            const addRes = await biometricApi.addBiometric({
                employee_id: employeeId,
                biometric_type: deviceBiometricType,
                biometric_datas: dataToSend
            });

            if (addRes.success) {
                toast.success(`Lưu ${devicePreviews.length} dữ liệu sinh trắc học thành công!`);
                onClose();
            }
        } catch (error) {
            toast.error('Lưu dữ liệu thất bại!');
        }
    };

    const fileToBase64 = (file: File): Promise<string> => {
        return new Promise((resolve, reject) => {
            const reader = new FileReader();
            reader.readAsDataURL(file);
            reader.onload = () => resolve(reader.result as string);
            reader.onerror = (error) => reject(error);
        });
    };

    const handleFileChange = async (e: React.ChangeEvent<HTMLInputElement>) => {
        if (e.target.files) {
            const filesArray = Array.from(e.target.files);
            const newFiles = await Promise.all(
                filesArray.map(async file => ({
                    name: file.name,
                    base64: await fileToBase64(file)
                }))
            );
            setUploadFiles(prev => [...prev, ...newFiles]);
        }
        if (fileInputRef.current) {
            fileInputRef.current.value = '';
        }
    };

    const handleSubmitUpload = async () => {
        if (uploadFiles.length === 0) {
            toast.error('Vui lòng chọn ít nhất một hình ảnh');
            return;
        }

        setIsUploading(true);
        try {
            const dataToSend = uploadFiles.map(file => {
                if (file.base64.startsWith('data:image')) {
                    return file.base64.split(',')[1];
                } else {
                    return file.base64;
                }
            });

            const res = await biometricApi.addBiometric({
                employee_id: employeeId,
                biometric_type: uploadBiometricType,
                biometric_datas: dataToSend
            });

            if (res.success) {
                toast.success(`Đã lưu ${uploadFiles.length} hình ảnh sinh trắc học thành công!`);
                onClose();
            }
        } catch (error) {
            console.error(error);
            toast.error('Tải lên thất bại. Vui lòng thử lại!');
        } finally {
            setIsUploading(false);
        }
    };

    if (!isOpen) return null;

    const deviceOptions: DropdownOption<string>[] = devices.map(d => ({
        label: d,
        value: d,
        icon: faServer
    }));

    return (
        <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-gray-900/50 backdrop-blur-xs animate-in fade-in duration-200">
            <div className="bg-white rounded-3xl shadow-2xl w-full max-w-2xl border border-gray-100 overflow-hidden transform transition-all flex flex-col max-h-[90vh]">

                <div className="flex justify-between items-center px-8 py-5 border-b border-gray-100 shrink-0">
                    <div>
                        <h2 className="text-2xl font-bold text-gray-800">Thêm Dữ Liệu Sinh Trắc Học</h2>
                        <p className="text-xs text-gray-400 mt-0.5">Nhân viên ID: #{employeeId}</p>
                    </div>
                    <button
                        onClick={onClose}
                        className="text-gray-400 hover:text-red-500 bg-gray-50 hover:bg-red-50 w-9 h-9 rounded-full flex items-center justify-center transition-all"
                    >
                        <FontAwesomeIcon icon={faTimes} className="text-base" />
                    </button>
                </div>

                <div className="flex border-b border-gray-100 bg-gray-50/70 p-2 gap-2 shrink-0">
                    <button
                        type="button"
                        onClick={() => setActiveTab('UPLOAD')}
                        disabled={isCollecting}
                        className={`flex-1 py-3 rounded-2xl font-semibold text-sm flex items-center justify-center gap-2.5 transition-all duration-200 ${activeTab === 'UPLOAD'
                                ? 'bg-white text-blue-600 shadow-sm border border-gray-200/60'
                                : 'text-gray-500 hover:text-gray-700 hover:bg-gray-100/70'
                            }`}
                    >
                        <FontAwesomeIcon icon={faUpload} className="text-base" />
                        <span>Upload ảnh</span>
                    </button>

                    <button
                        type="button"
                        onClick={() => setActiveTab('DEVICE')}
                        disabled={isUploading}
                        className={`flex-1 py-3 rounded-2xl font-semibold text-sm flex items-center justify-center gap-2.5 transition-all duration-200 ${activeTab === 'DEVICE'
                                ? 'bg-white text-blue-600 shadow-sm border border-gray-200/60'
                                : 'text-gray-500 hover:text-gray-700 hover:bg-gray-100/70'
                            }`}
                    >
                        <FontAwesomeIcon icon={faMicrochip} className="text-base" />
                        <span>Lấy từ thiết bị</span>
                    </button>
                </div>

                <div className="p-8 overflow-y-auto">
                    {activeTab === 'UPLOAD' ? (
                        <div className="space-y-6">
                            <CustomSelect
                                label="Loại sinh trắc học"
                                options={biometricTypeOptions}
                                value={uploadBiometricType}
                                onChange={(val) => setUploadBiometricType(val)}
                                disabled={isUploading}
                            />

                            <div>
                                <label className="block text-sm font-semibold text-gray-700 mb-2">
                                    Hình ảnh sinh trắc học (Chọn nhiều)
                                </label>
                                <input
                                    type="file"
                                    ref={fileInputRef}
                                    multiple
                                    accept="image/*"
                                    className="hidden"
                                    onChange={handleFileChange}
                                />

                                <div
                                    onClick={() => fileInputRef.current?.click()}
                                    className="flex flex-col items-center justify-center p-8 border-2 border-dashed border-blue-200 rounded-2xl bg-blue-50/20 text-blue-600 hover:bg-blue-50/60 hover:border-blue-400 cursor-pointer transition-all duration-200 group"
                                >
                                    <div className="w-14 h-14 rounded-full bg-blue-100/70 flex items-center justify-center mb-3 group-hover:scale-110 transition-transform">
                                        <FontAwesomeIcon icon={faCloudUploadAlt} className="text-2xl text-blue-600" />
                                    </div>
                                    <p className="text-base font-semibold text-gray-800">Kéo thả hoặc nhấn để chọn ảnh</p>
                                    <p className="text-xs text-gray-400 mt-1">Hỗ trợ các định dạng PNG, JPG, JPEG, WEBP</p>
                                </div>

                                {uploadFiles.length > 0 && (
                                    <div className="mt-4">
                                        <p className="text-sm font-semibold text-gray-700 mb-3">
                                            Đã chọn ({uploadFiles.length} ảnh):
                                        </p>
                                        <div className="grid grid-cols-3 sm:grid-cols-4 gap-4">
                                            {uploadFiles.map((file, idx) => (
                                                <div key={idx} className="relative aspect-square rounded-xl overflow-hidden border border-gray-200 bg-gray-50 group">
                                                    <img src={file.base64} alt={file.name} className="w-full h-full object-cover" />
                                                    <button
                                                        type="button"
                                                        onClick={() => setUploadFiles(prev => prev.filter((_, i) => i !== idx))}
                                                        className="absolute top-1.5 right-1.5 bg-white/90 hover:bg-red-500 hover:text-white text-gray-700 w-7 h-7 rounded-full flex items-center justify-center transition-colors shadow-sm"
                                                    >
                                                        <FontAwesomeIcon icon={faTimes} className="text-sm" />
                                                    </button>
                                                </div>
                                            ))}
                                        </div>
                                    </div>
                                )}
                            </div>

                            <div className="flex justify-end pt-4 border-t border-gray-100">
                                <button
                                    type="button"
                                    onClick={handleSubmitUpload}
                                    disabled={isUploading || uploadFiles.length === 0}
                                    className="bg-blue-600 hover:bg-blue-700 disabled:bg-blue-300 text-white px-7 py-3 rounded-xl font-semibold flex items-center gap-2.5 shadow-sm shadow-blue-200 transition-all"
                                >
                                    {isUploading ? (
                                        <>
                                            <FontAwesomeIcon icon={faSpinner} spin />
                                            <span>Đang lưu...</span>
                                        </>
                                    ) : (
                                        <>
                                            <FontAwesomeIcon icon={faUpload} />
                                            <span>Lưu dữ liệu {uploadFiles.length > 0 && `(${uploadFiles.length})`}</span>
                                        </>
                                    )}
                                </button>
                            </div>
                        </div>
                    ) : (
                        <div className="space-y-6">
                            <div>
                                <label className="block text-sm font-semibold text-gray-700 mb-2">
                                    Thiết bị thu thập
                                </label>
                                <div className="flex gap-3">
                                    <div className="flex-1">
                                        <CustomSelect
                                            options={deviceOptions}
                                            value={selectedDevice}
                                            onChange={(val) => setSelectedDevice(val)}
                                            disabled={isCollecting || isLoadingDevices}
                                            placeholder={isLoadingDevices ? "Đang tải danh sách..." : "Chọn thiết bị máy đọc"}
                                        />
                                    </div>

                                    <button
                                        type="button"
                                        onClick={fetchDevices}
                                        disabled={isCollecting || isLoadingDevices}
                                        className="border border-gray-200 bg-gray-50/70 hover:bg-gray-100 text-gray-600 px-4.5 rounded-xl transition-all flex items-center justify-center shrink-0 self-stretch"
                                        title="Tải lại danh sách thiết bị"
                                    >
                                        <FontAwesomeIcon
                                            icon={faSyncAlt}
                                            className={`text-base ${isLoadingDevices ? 'animate-spin text-blue-600' : ''}`}
                                        />
                                    </button>
                                </div>
                            </div>

                            <CustomSelect
                                label="Kiểu dữ liệu sinh trắc học"
                                options={biometricTypeOptions}
                                value={deviceBiometricType}
                                onChange={(val) => setDeviceBiometricType(val)}
                                disabled={isCollecting}
                            />

                            {isCollecting && (
                                <div className="flex flex-col items-center justify-center p-6 bg-blue-50/80 border border-blue-100 rounded-2xl text-blue-700 animate-pulse">
                                    <FontAwesomeIcon icon={faSpinner} spin className="text-3xl mb-3 text-blue-600" />
                                    <p className="text-sm font-bold">Đang chờ dữ liệu từ thiết bị {selectedDevice}...</p>
                                    <span className="text-xs text-blue-500 mt-1">Vui lòng hướng mặt vào camera hoặc áp ngón tay vào cảm biến</span>
                                </div>
                            )}

                            {devicePreviews.length > 0 && (
                                <div className="mt-4">
                                    <p className="text-sm font-semibold text-gray-700 mb-3">
                                        Dữ liệu đã thu thập ({devicePreviews.length} ảnh):
                                    </p>
                                    <div className="grid grid-cols-3 sm:grid-cols-4 gap-4">
                                        {devicePreviews.map((preview, idx) => (
                                            <div key={idx} className="relative aspect-square rounded-xl overflow-hidden border border-green-200 bg-green-50 flex items-center justify-center group">
                                                <img src={'data:image/jpg;base64,' + preview} alt="Device Preview" className="w-full h-full object-cover" />
                                                <button
                                                    type="button"
                                                    onClick={() => setDevicePreviews(prev => prev.filter((_, i) => i !== idx))}
                                                    className="absolute top-1.5 right-1.5 bg-white/90 hover:bg-red-500 hover:text-white text-gray-700 w-7 h-7 rounded-full flex items-center justify-center transition-colors shadow-sm"
                                                >
                                                    <FontAwesomeIcon icon={faTimes} className="text-sm" />
                                                </button>
                                            </div>
                                        ))}
                                    </div>
                                </div>
                            )}

                            <div className="flex justify-between items-center pt-4 border-t border-gray-100 gap-4">
                                <button
                                    type="button"
                                    onClick={handleToggleDeviceCollect}
                                    className={`px-6 py-3 rounded-xl font-semibold flex items-center justify-center gap-2.5 transition-all shadow-sm ${isCollecting
                                            ? 'bg-red-50 hover:bg-red-100 text-red-600 border border-red-200 ring-2 ring-red-100'
                                            : 'bg-blue-50 hover:bg-blue-100 text-blue-700 border border-blue-200'
                                        }`}
                                >
                                    {isCollecting ? (
                                        <>
                                            <FontAwesomeIcon icon={faTimes} className="text-base" />
                                            <span>Hủy quá trình</span>
                                        </>
                                    ) : (
                                        <>
                                            <FontAwesomeIcon icon={deviceBiometricType === 'FACE' ? faCamera : faFingerprint} className="text-base" />
                                            <span>Chụp thêm ảnh</span>
                                        </>
                                    )}
                                </button>

                                <button
                                    type="button"
                                    onClick={handleConfirmPreview}
                                    disabled={devicePreviews.length === 0 || isCollecting}
                                    className="px-8 py-3 rounded-xl font-semibold text-white bg-green-600 hover:bg-green-700 disabled:bg-gray-300 shadow-sm transition-all flex items-center justify-center gap-2"
                                >
                                    <FontAwesomeIcon icon={faCheck} />
                                    <span>Xác Nhận Lưu {devicePreviews.length > 0 && `(${devicePreviews.length})`}</span>
                                </button>
                            </div>
                        </div>
                    )}
                </div>
            </div>
        </div>
    );
};