// frontend/src/pages/Dashboard.jsx
import React, { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import http from '../utils/http';

const DashboardPage = () => {
    const navigate = useNavigate();
    const [user, setUser] = useState(null);
    const [health, setHealth] = useState(null);

    useEffect(() => {
        const token = localStorage.getItem('token');
        if (!token) {
            navigate('/login');
            return;
        }
        // 解码 JWT 获取用户名（简单起见直接解析 payload）
        try {
            const payload = JSON.parse(atob(token.split('.')[1]));
            setUser(payload);
        } catch {
            // ignore
        }
        // 健康检查
        http.get('/health').then(r => setHealth(r.data)).catch(() => {});
    }, [navigate]);

    const handleLogout = () => {
        localStorage.removeItem('token');
        navigate('/login');
    };

    return (
        <div className="min-h-screen bg-gray-50">
            <nav className="bg-white shadow-sm border-b">
                <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
                    <div className="flex justify-between h-16">
                        <div className="flex items-center">
                            <h1 className="text-xl font-semibold text-gray-900">BlessStar Demo</h1>
                        </div>
                        <div className="flex items-center space-x-4">
                            {user && (
                                <span className="text-sm text-gray-600">
                                    {user.sub || '用户'}
                                </span>
                            )}
                            <button
                                onClick={handleLogout}
                                className="px-4 py-2 text-sm text-red-600 hover:text-red-800 border border-red-300 rounded-md hover:bg-red-50"
                            >
                                退出登录
                            </button>
                        </div>
                    </div>
                </div>
            </nav>
            <main className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
                <div className="bg-white shadow rounded-lg p-6">
                    <h2 className="text-lg font-medium text-gray-900 mb-4">系统状态</h2>
                    <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                        <div className="border rounded-lg p-4">
                            <p className="text-sm text-gray-500">认证状态</p>
                            <p className="text-lg font-semibold text-green-600">已登录</p>
                        </div>
                        <div className="border rounded-lg p-4">
                            <p className="text-sm text-gray-500">后端服务</p>
                            <p className="text-lg font-semibold text-green-600">
                                {health ? health.status : '检查中...'}
                            </p>
                        </div>
                    </div>
                </div>
            </main>
        </div>
    );
};

export default DashboardPage;
