import api from './config';

export const getSystemInfo = () => {
  return api.get('/system/info');
}; 