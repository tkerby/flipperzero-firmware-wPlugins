import axios from 'axios';

// 创建 axios 实例
const api = axios.create({
  baseURL: '/api',
  timeout: 10000,
  headers: {
    'Content-Type': 'application/json'
  }
});

// 响应拦截器
api.interceptors.response.use(
  response => {
    const res = response.data;
    if (res.code === 0) {
      return res.data;
    }
    // 处理错误
    console.error('API Error:', res.message);
    return Promise.reject(new Error(res.message));
  },
  error => {
    console.error('Request Error:', error);
    return Promise.reject(error);
  }
);

export default api; 