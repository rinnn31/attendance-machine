import React, { useState } from 'react';
import { Toaster } from 'react-hot-toast';
import { Layout } from './components/Layout';
import { EmployeeList } from './pages/EmployeeList';
import { AttendanceStats } from './pages/AttendanceStats';
import { DeviceList } from './pages/DeviceList';

const App: React.FC = () => {
    const [activeTab, setActiveTab] = useState<'employee' | 'attendance' | 'device'>('employee');

    return (
        <>
            <Toaster position="top-right" toastOptions={{ duration: 3000 }} />
            <Layout activeTab={activeTab} setActiveTab={setActiveTab}>
                {activeTab === 'employee' ? <EmployeeList /> : activeTab === 'attendance' ? <AttendanceStats /> : <DeviceList />}
            </Layout>
        </>
    );
};

export default App;