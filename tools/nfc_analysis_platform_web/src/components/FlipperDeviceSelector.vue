<template>
  <div class="device-selector-container">
    <div class="device-selector-header">
      <h3 class="device-selector-title">{{ title || t('nard.deviceSelector.title') }}</h3>
      <button 
        @click="refreshDevices" 
        class="refresh-button"
        :class="{ 'refreshing': isLoading }"
        :disabled="isLoading"
      >
        <svg xmlns="http://www.w3.org/2000/svg" class="refresh-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
          <path d="M23 4v6h-6"></path>
          <path d="M1 20v-6h6"></path>
          <path d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10"></path>
          <path d="M20.49 15a9 9 0 0 1-14.85 3.36L1 14"></path>
        </svg>
      </button>
    </div>
    
    <div class="device-selector-content">
      <div v-if="isLoading" class="loading-container">
        <div class="loading-spinner"></div>
        <span>{{ t('nard.deviceSelector.loading') }}</span>
      </div>
      
      <div v-else-if="error" class="error-container">
        <svg xmlns="http://www.w3.org/2000/svg" class="error-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
          <circle cx="12" cy="12" r="10"></circle>
          <line x1="12" y1="8" x2="12" y2="12"></line>
          <line x1="12" y1="16" x2="12.01" y2="16"></line>
        </svg>
        <span>{{ error }}</span>
      </div>
      
      <div v-else-if="devices.length === 0" class="no-devices-container">
        <svg xmlns="http://www.w3.org/2000/svg" class="no-devices-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
          <rect x="2" y="4" width="20" height="16" rx="2" ry="2"></rect>
          <line x1="2" y1="8" x2="22" y2="8"></line>
          <line x1="8" y1="16" x2="16" y2="16"></line>
        </svg>
        <span>{{ t('nard.deviceSelector.noDevices') }}</span>
      </div>
      
      <div v-else class="devices-list">
        <div 
          v-for="device in allDevices" 
          :key="device.id" 
          class="device-item"
          :class="{ 'selected': selectedDeviceId === device.id }"
          @click="selectDevice(device)"
        >
          <div class="device-icon">
            <svg v-if="device.id === 'auto'" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
              <path d="M5 12h14"></path>
              <path d="M12 5v14"></path>
            </svg>
            <svg v-else xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
              <rect x="5" y="2" width="14" height="20" rx="2" ry="2"></rect>
              <line x1="12" y1="18" x2="12.01" y2="18"></line>
            </svg>
          </div>
          <div class="device-info">
            <div class="device-name">{{ getDeviceName(device) }}</div>
            <div v-if="device.id !== 'auto'" class="device-path">{{ device.path || device.serial_port }}</div>
          </div>
          <div class="device-select-indicator">
            <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
              <polyline points="20 6 9 17 4 12"></polyline>
            </svg>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import axios from 'axios';

const { t } = useI18n();

const props = defineProps({
  title: {
    type: String,
    default: ''
  }
});

const emit = defineEmits(['device-selected', 'device-status-changed']);

const devices = ref([]);
const selectedDeviceId = ref('auto');
const isLoading = ref(false);
const error = ref('');
const deviceStatus = ref('no-device'); // 新增：设备状态 'no-device', 'available', 'selected'

// 添加自动选择选项
const allDevices = computed(() => {
  return [
    { id: 'auto', name: t('nard.deviceSelector.autoSelect') },
    ...devices.value
  ];
});

// 获取设备名称
const getDeviceName = (device) => {
  if (device.id === 'auto') {
    return t('nard.deviceSelector.autoSelect');
  }
  
  return device.name || (device.is_serial 
    ? `${t('nard.deviceSelector.serialDevice')} (${device.serial_port})` 
    : `Flipper Zero (${device.id})`);
};

// 更新设备状态
const updateDeviceStatus = () => {
  const oldStatus = deviceStatus.value;
  const newStatus = devices.value.length === 0 ? 'no-device' : 
                   (selectedDeviceId.value === 'auto' ? 'available' : 'selected');
  
  // 只有在状态发生变化时才更新并发送事件
  if (oldStatus !== newStatus) {
    console.log(`设备状态从 ${oldStatus} 变为 ${newStatus}`);
    deviceStatus.value = newStatus;
    emit('device-status-changed', newStatus);
  }
};

// 刷新设备列表
const refreshDevices = async () => {
  isLoading.value = true;
  error.value = '';
  
  console.log('开始刷新设备列表...');
  console.log('刷新前状态:', {
    deviceCount: devices.value.length,
    selectedDeviceId: selectedDeviceId.value,
    deviceStatus: deviceStatus.value
  });
  
  try {
    console.log('发送API请求获取设备列表...');
    const response = await axios.get('/api/nard/flipper/devices');
    console.log('API响应:', response.data);
    
    if (response.data.code === 0) {
      const newDevices = response.data.data || [];
      const oldDevicesCount = devices.value.length;
      
      // 更新设备列表
      devices.value = newDevices;
      console.log('刷新后设备列表:', devices.value);
      
      // 如果设备列表为空，强制重置为自动选择并更新状态
      if (devices.value.length === 0) {
        console.log('设备列表为空，处理无设备状态');
        
        // 重置选择
        if (selectedDeviceId.value !== 'auto') {
          console.log('重置设备选择为auto');
          selectedDeviceId.value = 'auto';
          emit('device-selected', { id: 'auto' });
        }
        
        // 强制更新状态为无设备
        console.log('强制更新状态为no-device');
        deviceStatus.value = 'no-device';
        emit('device-status-changed', 'no-device');
      } else {
        console.log('设备列表不为空，检查选中设备是否存在');
        
        // 如果当前选中的设备不在列表中，重置为自动选择
        if (selectedDeviceId.value !== 'auto' && 
            !devices.value.find(d => d.id === selectedDeviceId.value)) {
          console.log('选中的设备不在列表中，重置为auto');
          selectedDeviceId.value = 'auto';
          emit('device-selected', { id: 'auto' });
        }
        
        // 更新设备状态
        updateDeviceStatus();
      }
      
      console.log('刷新后状态:', {
        deviceCount: devices.value.length,
        selectedDeviceId: selectedDeviceId.value,
        deviceStatus: deviceStatus.value
      });
    } else {
      console.error('API返回错误:', response.data.message, '错误代码:', response.data.code);
      
      // 根据错误码或消息使用对应的翻译
      if (response.data.message === '未找到Flipper Zero设备' || response.data.code === 404) {
        error.value = t('nard.deviceSelector.deviceNotFound');
      } else {
        error.value = response.data.message || t('nard.deviceSelector.errorLoading');
      }
      
      // 无论错误代码是什么，当API返回错误时，都清空设备列表并更新状态
      console.log('API返回错误，清空设备列表');
      devices.value = [];
      
      // 重置选择
      if (selectedDeviceId.value !== 'auto') {
        console.log('重置设备选择为auto');
        selectedDeviceId.value = 'auto';
        emit('device-selected', { id: 'auto' });
      }
      
      // 强制更新状态为无设备
      console.log('强制更新状态为no-device');
      deviceStatus.value = 'no-device';
      emit('device-status-changed', 'no-device');
      
      console.log('错误处理完成，最终状态:', {
        deviceCount: devices.value.length,
        selectedDeviceId: selectedDeviceId.value,
        deviceStatus: deviceStatus.value
      });
    }
  } catch (err) {
    console.error('刷新设备列表失败:', err);
    error.value = t('nard.deviceSelector.errorLoading');
    
    // 发生错误时，清空设备列表并更新状态
    devices.value = [];
    selectedDeviceId.value = 'auto'; // 强制重置为自动选择
    emit('device-selected', { id: 'auto' });
    
    // 强制更新状态为无设备
    deviceStatus.value = 'no-device';
    emit('device-status-changed', 'no-device');
  } finally {
    isLoading.value = false;
  }
};

// 选择设备
const selectDevice = (device) => {
  selectedDeviceId.value = device.id;
  emit('device-selected', device);
  
  // 更新设备状态
  updateDeviceStatus();
};

// 监听设备列表变化
watch(devices, (newDevices) => {
  console.log('设备列表变化:', newDevices.length ? '有设备' : '无设备');
  
  // 如果设备列表为空，强制更新状态为无设备
  if (newDevices.length === 0 && deviceStatus.value !== 'no-device') {
    console.log('设备列表变为空，强制更新状态为no-device');
    deviceStatus.value = 'no-device';
    emit('device-status-changed', 'no-device');
  } else {
    updateDeviceStatus();
  }
}, { deep: true });

// 组件挂载时加载设备列表
onMounted(() => {
  refreshDevices();
});
</script>

<style scoped>
.device-selector-container {
  background-color: rgba(22, 27, 34, 0.7);
  border: 1px solid rgba(33, 38, 45, 1);
  border-radius: 8px;
  overflow: hidden;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.2);
  transition: all 0.3s ease;
}

.device-selector-container:hover {
  box-shadow: 0 6px 16px rgba(0, 0, 0, 0.3);
  border-color: rgba(56, 189, 248, 0.4);
}

.device-selector-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 12px 16px;
  background-color: rgba(28, 33, 40, 0.7);
  border-bottom: 1px solid rgba(33, 38, 45, 1);
}

.device-selector-title {
  margin: 0;
  font-size: 16px;
  font-weight: 500;
  color: rgba(201, 209, 217, 1);
}

.refresh-button {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 32px;
  height: 32px;
  border-radius: 6px;
  background-color: transparent;
  border: 1px solid rgba(33, 38, 45, 1);
  color: rgba(201, 209, 217, 0.8);
  cursor: pointer;
  transition: all 0.2s ease;
}

.refresh-button:hover {
  background-color: rgba(33, 38, 45, 0.8);
  color: rgba(201, 209, 217, 1);
  border-color: rgba(56, 189, 248, 0.4);
}

.refresh-button:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

.refresh-icon {
  width: 18px;
  height: 18px;
}

.refreshing .refresh-icon {
  animation: spin 1.2s linear infinite;
}

@keyframes spin {
  0% { transform: rotate(0deg); }
  100% { transform: rotate(360deg); }
}

.device-selector-content {
  padding: 16px;
  max-height: 300px;
  overflow-y: auto;
}

.loading-container,
.error-container,
.no-devices-container {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  padding: 24px 16px;
  text-align: center;
  color: rgba(201, 209, 217, 0.8);
}

.loading-spinner {
  width: 32px;
  height: 32px;
  border: 3px solid rgba(56, 189, 248, 0.2);
  border-top-color: rgba(56, 189, 248, 0.8);
  border-radius: 50%;
  animation: spin 1s linear infinite;
  margin-bottom: 12px;
}

.error-icon,
.no-devices-icon {
  width: 32px;
  height: 32px;
  margin-bottom: 12px;
  color: rgba(248, 81, 73, 0.8);
}

.no-devices-icon {
  color: rgba(201, 209, 217, 0.6);
}

.devices-list {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.device-item {
  display: flex;
  align-items: center;
  padding: 12px;
  border-radius: 6px;
  background-color: rgba(33, 38, 45, 0.5);
  border: 1px solid rgba(33, 38, 45, 1);
  cursor: pointer;
  transition: all 0.2s ease;
}

.device-item:hover {
  background-color: rgba(33, 38, 45, 0.8);
  border-color: rgba(56, 189, 248, 0.4);
}

.device-item.selected {
  background-color: rgba(56, 189, 248, 0.1);
  border-color: rgba(56, 189, 248, 0.6);
}

.device-icon {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 36px;
  height: 36px;
  border-radius: 8px;
  background-color: rgba(28, 33, 40, 0.7);
  margin-right: 12px;
}

.device-icon svg {
  width: 20px;
  height: 20px;
  color: rgba(56, 189, 248, 0.8);
}

.device-info {
  flex: 1;
}

.device-name {
  font-size: 14px;
  font-weight: 500;
  color: rgba(201, 209, 217, 1);
  margin-bottom: 2px;
}

.device-path {
  font-size: 12px;
  color: rgba(201, 209, 217, 0.6);
  font-family: monospace;
}

.device-select-indicator {
  width: 24px;
  height: 24px;
  opacity: 0;
  transition: opacity 0.2s ease;
}

.device-select-indicator svg {
  width: 24px;
  height: 24px;
  color: rgba(56, 189, 248, 0.8);
}

.device-item.selected .device-select-indicator {
  opacity: 1;
}

/* 自定义滚动条 */
.device-selector-content::-webkit-scrollbar {
  width: 6px;
}

.device-selector-content::-webkit-scrollbar-track {
  background: rgba(22, 27, 34, 0.3);
  border-radius: 3px;
}

.device-selector-content::-webkit-scrollbar-thumb {
  background: rgba(56, 189, 248, 0.3);
  border-radius: 3px;
}

.device-selector-content::-webkit-scrollbar-thumb:hover {
  background: rgba(56, 189, 248, 0.5);
}
</style> 