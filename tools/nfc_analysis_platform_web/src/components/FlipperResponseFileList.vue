<template>
  <div class="response-file-list-container" :class="{ 'disabled': disabled }">
    <div class="response-file-list-header">
      <h3 class="response-file-list-title">{{ title || t('nard.responseFiles.title') }}</h3>
      <button 
        @click="refreshFiles" 
        class="refresh-button"
        :class="{ 'refreshing': isLoading }"
        :disabled="isLoading || disabled"
      >
        <svg xmlns="http://www.w3.org/2000/svg" class="refresh-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
          <path d="M23 4v6h-6"></path>
          <path d="M1 20v-6h6"></path>
          <path d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10"></path>
          <path d="M20.49 15a9 9 0 0 1-14.85 3.36L1 14"></path>
        </svg>
      </button>
    </div>
    
    <div class="response-file-list-content" ref="fileListContent" :class="{ 'is-scrolling': isScrolling }">
      <div v-if="isLoading" class="loading-container">
        <div class="loading-spinner"></div>
        <span>{{ t('nard.responseFiles.loading') }}</span>
      </div>
      
      <div v-else-if="error" class="error-container">
        <svg xmlns="http://www.w3.org/2000/svg" class="error-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
          <circle cx="12" cy="12" r="10"></circle>
          <line x1="12" y1="8" x2="12" y2="12"></line>
          <line x1="12" y1="16" x2="12.01" y2="16"></line>
        </svg>
        <span>{{ error }}</span>
      </div>
      
      <div v-else-if="files.length === 0" class="no-files-container">
        <svg xmlns="http://www.w3.org/2000/svg" class="no-files-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
          <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"></path>
          <polyline points="14 2 14 8 20 8"></polyline>
          <line x1="12" y1="18" x2="12" y2="12"></line>
          <line x1="9" y1="15" x2="15" y2="15"></line>
        </svg>
        <span>{{ t('nard.responseFiles.noFiles') }}</span>
      </div>
      
      <div v-else class="files-list">
        <div 
          v-for="file in files" 
          :key="file.id" 
          class="file-item"
          :class="{ 'selected': selectedFileId === file.id, 'disabled': disabled }"
          @click="selectFile(file)"
        >
          <div class="file-icon">
            <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
              <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"></path>
              <polyline points="14 2 14 8 20 8"></polyline>
              <path d="M16 13H8"></path>
              <path d="M16 17H8"></path>
              <path d="M10 9H8"></path>
            </svg>
          </div>
          <div class="file-info">
            <div class="file-name">{{ file.name || file.id }}</div>
          </div>
          <div class="file-select-indicator">
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
import { ref, computed, onMounted, onUnmounted, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import axios from 'axios';

const { t } = useI18n();

const props = defineProps({
  title: {
    type: String,
    default: ''
  },
  devicePath: {
    type: String,
    default: ''
  },
  serialPort: {
    type: String,
    default: ''
  },
  useSerial: {
    type: Boolean,
    default: false
  },
  disabled: {
    type: Boolean,
    default: false
  }
});

const emit = defineEmits(['file-selected']);

const files = ref([]);
const selectedFileId = ref('');
const selectedFileContent = ref('');
const isLoading = ref(false);
const error = ref('');
const fileListContent = ref(null); // 引用滚动容器
const isScrolling = ref(false); // 是否正在滚动
let scrollTimer = null; // 滚动计时器

// 处理滚动事件
const handleScroll = () => {
  isScrolling.value = true;
  
  // 清除之前的计时器
  if (scrollTimer) {
    clearTimeout(scrollTimer);
  }
  
  // 设置新的计时器，滚动停止1.5秒后隐藏滚动条
  scrollTimer = setTimeout(() => {
    isScrolling.value = false;
  }, 1500);
};

// 刷新文件列表
const refreshFiles = async () => {
  // 如果组件被禁用，不执行刷新
  if (props.disabled) {
    return;
  }
  
  isLoading.value = true;
  error.value = '';
  
  try {
    // 构建请求参数
    const params = {};
    
    if (props.devicePath) {
      params.device_path = props.devicePath;
    }
    
    if (props.serialPort) {
      params.serial_port = props.serialPort;
    }
    
    // 如果是自动选择设备或明确指定了使用串口，设置use_serial=true
    if (props.useSerial || (!props.devicePath && !props.serialPort)) {
      params.use_serial = true;
    }
    
    // 调用API获取文件列表
    const response = await axios.get('/api/nard/flipper/files', { params });
    
    if (response.data.code === 0) {
      // 更新文件列表
      files.value = response.data.data || [];
      
      // 如果当前选中的文件不在列表中，清除选择
      if (selectedFileId.value && !files.value.find(f => f.id === selectedFileId.value)) {
        selectedFileId.value = '';
        selectedFileContent.value = '';
        emit('file-selected', null);
      }
    } else {
      // 处理API错误
      error.value = response.data.message || t('nard.responseFiles.errorLoading');
    }
  } catch (err) {
    console.error('加载响应文件列表失败:', err);
    error.value = t('nard.responseFiles.errorLoading');
  } finally {
    isLoading.value = false;
  }
};

// 选择文件
const selectFile = async (file) => {
  // 如果组件被禁用，不执行选择
  if (props.disabled) {
    return;
  }
  
  selectedFileId.value = file.id;
  
  try {
    // 构建请求参数
    const params = {};
    
    if (props.devicePath) {
      params.device_path = props.devicePath;
    }
    
    if (props.serialPort) {
      params.serial_port = props.serialPort;
    }
    
    // 如果是自动选择设备或明确指定了使用串口，设置use_serial=true
    if (props.useSerial || (!props.devicePath && !props.serialPort)) {
      params.use_serial = true;
    }
    
    // 调用API获取文件内容
    const response = await axios.get(`/api/nard/flipper/files/${file.id}`, { params });
    
    if (response.data.code === 0) {
      // 更新文件内容
      selectedFileContent.value = response.data.data.content || '';
      
      // 发送选中文件事件
      emit('file-selected', {
        ...file,
        content: selectedFileContent.value
      });
    } else {
      // 处理API错误
      error.value = response.data.message || t('nard.responseFiles.errorLoadingContent');
    }
  } catch (err) {
    console.error('加载文件内容失败:', err);
    error.value = t('nard.responseFiles.errorLoadingContent');
  }
};

// 清空文件列表和选择
const clearFiles = () => {
  files.value = [];
  selectedFileId.value = '';
  selectedFileContent.value = '';
  error.value = '';
};

// 监听设备路径变化
watch(() => props.devicePath, (newPath, oldPath) => {
  if (newPath !== oldPath) {
    // 如果设备路径变化，刷新文件列表
    if (newPath) {
      refreshFiles();
    } else {
      // 如果设备路径为空，清空文件列表
      clearFiles();
    }
  }
});

// 监听禁用状态变化
watch(() => props.disabled, (newValue, oldValue) => {
  if (newValue !== oldValue) {
    if (newValue) {
      // 如果组件被禁用，清空文件列表
      clearFiles();
    } else if (props.devicePath || props.serialPort) {
      // 如果组件被启用且有设备路径，刷新文件列表
      refreshFiles();
    }
  }
});

// 组件挂载时，如果有设备路径且未禁用，加载文件列表
onMounted(() => {
  if ((props.devicePath || props.serialPort) && !props.disabled) {
    refreshFiles();
  }
  
  // 添加滚动事件监听
  if (fileListContent.value) {
    fileListContent.value.addEventListener('scroll', handleScroll);
  }
});

// 组件卸载时，移除滚动事件监听
onUnmounted(() => {
  if (fileListContent.value) {
    fileListContent.value.removeEventListener('scroll', handleScroll);
  }
  
  // 清除滚动计时器
  if (scrollTimer) {
    clearTimeout(scrollTimer);
  }
});

// 暴露方法给父组件
defineExpose({
  refreshFiles,
  clearFiles
});
</script>

<style scoped>
.response-file-list-container {
  background-color: rgba(22, 27, 34, 0.7);
  overflow: hidden;
  transition: all 0.3s ease;
}

.response-file-list-container.disabled {
  opacity: 0.6;
  pointer-events: none;
}

.response-file-list-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 12px 16px;
  background-color: rgba(28, 33, 40, 0.7);
  border-bottom: 1px solid rgba(33, 38, 45, 1);
}

.response-file-list-title {
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

.response-file-list-content {
  padding: 16px;
  max-height: 300px;
  overflow-y: auto;
  scrollbar-width: thin; /* Firefox */
  scrollbar-color: transparent rgba(22, 27, 34, 0.3); /* Firefox - 默认隐藏滑块 */
}

.response-file-list-content.is-scrolling,
.response-file-list-content:hover {
  scrollbar-color: rgba(56, 189, 248, 0.3) rgba(22, 27, 34, 0.3); /* Firefox - 滚动或悬停时显示 */
}

.loading-container,
.error-container,
.no-files-container {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  padding: 2rem 1rem;
  text-align: center;
  color: var(--ark-text-secondary);
}

.loading-spinner {
  width: 2rem;
  height: 2rem;
  border: 2px solid rgba(56, 189, 248, 0.1);
  border-top-color: rgba(56, 189, 248, 0.8);
  border-radius: 50%;
  margin-bottom: 0.75rem;
  animation: spin 1s linear infinite;
}

.error-icon,
.no-files-icon {
  width: 2rem;
  height: 2rem;
  margin-bottom: 0.75rem;
}

.error-icon {
  color: var(--ark-red-light);
}

.files-list {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.file-item {
  display: flex;
  align-items: center;
  padding: 0.75rem;
  border-radius: 0.375rem;
  background-color: rgba(255, 255, 255, 0.02);
  border: 1px solid rgba(255, 255, 255, 0.05);
  cursor: pointer;
  transition: all 0.2s ease;
}

.file-item:hover {
  background-color: rgba(255, 255, 255, 0.05);
  border-color: rgba(56, 189, 248, 0.2);
}

.file-item.selected {
  background-color: rgba(56, 189, 248, 0.1);
  border-color: rgba(56, 189, 248, 0.6);
}

.file-icon {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 36px;
  height: 36px;
  border-radius: 8px;
  background-color: rgba(28, 33, 40, 0.7);
  margin-right: 12px;
}

.file-icon svg {
  width: 20px;
  height: 20px;
  color: rgba(56, 189, 248, 0.8);
}

.file-info {
  flex: 1;
  min-width: 0;
}

.file-name {
  font-size: 14px;
  font-weight: 500;
  color: rgba(201, 209, 217, 1);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.file-select-indicator {
  width: 24px;
  height: 24px;
  opacity: 0;
  transition: opacity 0.2s ease;
}

.file-select-indicator svg {
  width: 24px;
  height: 24px;
  color: rgba(56, 189, 248, 0.8);
}

.file-item.selected .file-select-indicator {
  opacity: 1;
}

/* 自定义滚动条 */
.response-file-list-content::-webkit-scrollbar {
  width: 6px;
}

.response-file-list-content::-webkit-scrollbar-track {
  background: rgba(22, 27, 34, 0.3);
  border-radius: 3px;
}

.response-file-list-content::-webkit-scrollbar-thumb {
  background: transparent; /* 默认透明 */
  border-radius: 3px;
  transition: background-color 0.3s ease;
}

.response-file-list-content.is-scrolling::-webkit-scrollbar-thumb,
.response-file-list-content:hover::-webkit-scrollbar-thumb {
  background: rgba(56, 189, 248, 0.3); /* 滚动或悬停时显示 */
}

.response-file-list-content::-webkit-scrollbar-thumb:hover {
  background: rgba(56, 189, 248, 0.5);
}

@keyframes spin {
  0% { transform: rotate(0deg); }
  100% { transform: rotate(360deg); }
}
</style> 