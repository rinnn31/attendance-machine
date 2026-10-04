import React, { useState } from 'react';
import { Toaster } from 'react-hot-toast';
import { Layout } from './components/Layout';
import { EmployeeList } from './pages/EmployeeList';
import { AttendanceStats } from './pages/AttendanceStats';

const App: React.FC = () => {
    const [activeTab, setActiveTab] = useState<'employee' | 'attendance'>('employee');

    return (
        <>
            <Toaster position="top-right" toastOptions={{ duration: 3000 }} />
            <Layout activeTab={activeTab} setActiveTab={setActiveTab}>
                {activeTab === 'employee' ? <EmployeeList /> : <AttendanceStats />}
            </Layout>
        </>
    );
};

export default App;