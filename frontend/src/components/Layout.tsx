import React from 'react';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import { faUsers, faChartBar } from '@fortawesome/free-solid-svg-icons';

interface Props {
    children: React.ReactNode;
    activeTab: 'employee' | 'attendance';
    setActiveTab: (tab: 'employee' | 'attendance') => void;
}

export const Layout: React.FC<Props> = ({ children, activeTab, setActiveTab }) => {
    return (
        <div className="flex h-screen bg-slate-50 font-sans text-gray-800">
            <div className="w-64 bg-white border-r border-gray-200 flex flex-col shadow-sm z-10 shrink-0">
                <div className="h-20 flex items-center gap-3 px-6 border-b border-gray-100">
                    <img
                        src="/logo.png"
                        alt="AttendX Logo"
                        className="w-15 h-15 object-cover"
                    />
                    <span className="font-bold text-2xl text-gray-800 tracking-tight">
                        ATTENDX
                    </span>
                </div>
                <nav className="flex-1 py-6 space-y-2 px-4">
                    <button
                        onClick={() => setActiveTab('employee')}
                        className={`w-full flex items-center gap-4 px-4 py-3 rounded-xl transition-all duration-200 ${activeTab === 'employee' ? 'bg-blue-50 text-blue-700 shadow-sm border border-blue-100' : 'text-gray-600 hover:bg-gray-50 hover:text-blue-600 border border-transparent'}`}
                    >
                        <FontAwesomeIcon icon={faUsers} className="text-lg w-6" />
                        <span className="font-semibold whitespace-nowrap">Nhân Viên</span>
                    </button>
                    <button
                        onClick={() => setActiveTab('attendance')}
                        className={`w-full flex items-center gap-4 px-4 py-3 rounded-xl transition-all duration-200 ${activeTab === 'attendance' ? 'bg-blue-50 text-blue-700 shadow-sm border border-blue-100' : 'text-gray-600 hover:bg-gray-50 hover:text-blue-600 border border-transparent'}`}
                    >
                        <FontAwesomeIcon icon={faChartBar} className="text-lg w-6" />
                        <span className="font-semibold whitespace-nowrap">Chấm Công</span>
                    </button>
                </nav>
            </div>

            <div className="flex-1 flex flex-col overflow-hidden relative">
                <header className="h-20 bg-white shadow-sm border-b border-gray-100 flex items-center px-8 z-0">
                    <h2 className="text-xl font-bold text-gray-800 tracking-tight">Hệ Thống Quản Lý Chấm Công</h2>
                </header>
                <main className="flex-1 overflow-x-hidden overflow-y-auto bg-slate-50 p-8">
                    {children}
                </main>
            </div>
        </div>
    );
};