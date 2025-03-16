<template>
  <div class="space-y-6">
    <!-- 文件上传区域 -->
    <div class="ark-panel p-6">
      <h1 class="text-2xl font-medium mb-6">NARD 数据解析</h1>
      
      <div class="space-y-4">
        <div class="border-2 border-dashed border-ark-border rounded-lg p-6 text-center">
          <input
            type="file"
            ref="fileInput"
            class="hidden"
            accept=".nard"
            @change="handleFileChange"
          >
          
          <div class="space-y-2">
            <svg class="mx-auto h-12 w-12 text-ark-text opacity-50" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M7 16a4 4 0 01-.88-7.903A5 5 0 1115.9 6L16 6a5 5 0 011 9.9M15 13l-3-3m0 0l-3 3m3-3v12" />
            </svg>
            
            <div class="text-sm">
              <button class="ark-btn-secondary" @click="triggerFileInput">
                选择 NARD 文件
              </button>
              <p class="mt-2 text-ark-text opacity-70">或将文件拖放到此处</p>
            </div>
          </div>
        </div>
        
        <div v-if="selectedFile" class="flex items-center justify-between p-4 bg-ark-panel-light rounded-lg">
          <div class="flex items-center space-x-3">
            <svg class="h-6 w-6 text-ark-text opacity-70" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 12h6m-6 4h6m2 5H7a2 2 0 01-2-2V5a2 2 0 012-2h5.586a1 1 0 01.707.293l5.414 5.414a1 1 0 01.293.707V19a2 2 0 01-2 2z" />
            </svg>
            <span class="font-medium">{{ selectedFile.name }}</span>
            <span class="text-sm text-ark-text opacity-70">{{ formatFileSize(selectedFile.size) }}</span>
          </div>
          
          <button 
            class="ark-btn-danger"
            @click="clearFile"
          >
            移除
          </button>
        </div>
        
        <div class="flex space-x-4">
          <button 
            @click="parseNard" 
            class="ark-btn-primary"
            :disabled="!selectedFile || loading"
          >
            <span class="flex items-center">
              <svg v-if="loading" class="animate-spin -ml-1 mr-2 h-4 w-4" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24">
                <circle class="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" stroke-width="4"></circle>
                <path class="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
              </svg>
              解析数据
            </span>
          </button>
        </div>
      </div>
    </div>

    <!-- 解析结果 -->
    <div v-if="parsedData" class="ark-panel p-6">
      <h2 class="text-lg font-medium mb-4">解析结果</h2>
      
      <!-- 基本信息 -->
      <div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6 mb-8">
        <div class="p-4 bg-ark-panel-light rounded-lg">
          <h3 class="text-sm font-medium mb-2">版本</h3>
          <p class="text-2xl font-mono">{{ parsedData.version }}</p>
        </div>
        
        <div class="p-4 bg-ark-panel-light rounded-lg">
          <h3 class="text-sm font-medium mb-2">设备类型</h3>
          <p class="text-2xl font-mono">{{ parsedData.device_type }}</p>
        </div>
        
        <div class="p-4 bg-ark-panel-light rounded-lg">
          <h3 class="text-sm font-medium mb-2">UID</h3>
          <p class="text-2xl font-mono">{{ parsedData.uid }}</p>
        </div>
      </div>
      
      <!-- 数据块 -->
      <div class="space-y-6">
        <h3 class="text-lg font-medium">数据块</h3>
        
        <div class="overflow-x-auto">
          <table class="ark-table">
            <thead>
              <tr>
                <th>块号</th>
                <th>数据</th>
                <th>状态</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="block in parsedData.blocks" :key="block.index">
                <td class="font-mono">{{ block.index }}</td>
                <td class="font-mono break-all">{{ block.data }}</td>
                <td>
                  <span 
                    class="px-2 py-1 text-xs rounded-full"
                    :class="{
                      'bg-green-100 text-green-800': block.status === 'valid',
                      'bg-red-100 text-red-800': block.status === 'error',
                      'bg-yellow-100 text-yellow-800': block.status === 'warning'
                    }"
                  >
                    {{ block.status }}
                  </span>
                </td>
              </tr>
            </tbody>
          </table>
        </div>
      </div>
    </div>

    <!-- 错误提示 -->
    <div v-if="error" class="ark-panel p-6 border-ark-red">
      <div class="flex items-start space-x-3 text-ark-red">
        <svg class="w-5 h-5 flex-shrink-0" viewBox="0 0 20 20" fill="currentColor">
          <path fill-rule="evenodd" d="M10 18a8 8 0 100-16 8 8 0 000 16zM8.707 7.293a1 1 0 00-1.414 1.414L8.586 10l-1.293 1.293a1 1 0 101.414 1.414L10 11.414l1.293 1.293a1 1 0 001.414-1.414L11.414 10l1.293-1.293a1 1 0 00-1.414-1.414L10 8.586 8.707 7.293z" clip-rule="evenodd" />
        </svg>
        <div>
          <h3 class="font-medium">解析错误</h3>
          <p class="mt-1 text-sm">{{ error }}</p>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref } from 'vue';

const fileInput = ref(null);
const selectedFile = ref(null);
const parsedData = ref(null);
const loading = ref(false);
const error = ref('');

const triggerFileInput = () => {
  fileInput.value.click();
};

const handleFileChange = (event) => {
  const file = event.target.files[0];
  if (file && file.name.endsWith('.nard')) {
    selectedFile.value = file;
    error.value = '';
  } else {
    error.value = '请选择有效的 NARD 文件';
    event.target.value = '';
  }
};

const clearFile = () => {
  selectedFile.value = null;
  parsedData.value = null;
  error.value = '';
  if (fileInput.value) {
    fileInput.value.value = '';
  }
};

const formatFileSize = (bytes) => {
  if (bytes === 0) return '0 B';
  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return `${parseFloat((bytes / Math.pow(k, i)).toFixed(2))} ${sizes[i]}`;
};

const parseNard = async () => {
  if (!selectedFile.value) return;
  
  loading.value = true;
  error.value = '';
  
  try {
    // 模拟解析过程
    await new Promise(resolve => setTimeout(resolve, 1000));
    
    // 这里是模拟的解析结果
    parsedData.value = {
      version: '1.0.0',
      device_type: 'NTAG215',
      uid: '04:1A:2B:3C:4D:5E:6F',
      blocks: [
        {
          index: '00',
          data: '04 1A 2B 3C 4D 5E 6F 80',
          status: 'valid'
        },
        {
          index: '01',
          data: '48 00 00 E1 10 12 00 00',
          status: 'valid'
        },
        {
          index: '02',
          data: 'FF FF FF FF FF FF FF FF',
          status: 'warning'
        }
      ]
    };
  } catch (err) {
    console.error('Failed to parse NARD file:', err);
    error.value = '无法解析 NARD 文件，请检查文件格式是否正确';
  } finally {
    loading.value = false;
  }
};
</script> 