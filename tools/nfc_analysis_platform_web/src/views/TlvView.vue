<template>
  <div class="space-y-6">
    <!-- 输入区域 -->
    <div class="ark-panel p-6">
      <h1 class="text-2xl font-medium mb-6">TLV 数据解析</h1>
      
      <div class="space-y-4">
        <textarea
          v-model="tlvInput"
          class="ark-input h-32 font-mono"
          placeholder="请输入 TLV 数据（十六进制格式）..."
        ></textarea>
        
        <div class="flex space-x-4">
          <button 
            @click="parseTlv" 
            class="ark-btn-primary"
            :disabled="!tlvInput || loading"
          >
            <span class="flex items-center">
              <svg v-if="loading" class="animate-spin -ml-1 mr-2 h-4 w-4" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24">
                <circle class="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" stroke-width="4"></circle>
                <path class="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
              </svg>
              解析数据
            </span>
          </button>
          <button 
            @click="loadSampleData" 
            class="ark-btn-secondary"
            :disabled="loading"
          >
            加载示例
          </button>
          <button 
            @click="clearData" 
            class="ark-btn-secondary"
            :disabled="loading"
          >
            清除
          </button>
        </div>
      </div>
    </div>

    <!-- 解析结果 -->
    <div v-if="parsedData.length > 0" class="ark-panel p-6">
      <h2 class="text-lg font-medium mb-4">解析结果</h2>
      
      <div class="overflow-x-auto">
        <table class="ark-table">
          <thead>
            <tr>
              <th>Tag</th>
              <th>Length</th>
              <th>Value</th>
              <th>描述</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="(item, index) in parsedData" :key="index">
              <td class="font-mono">{{ item.tag }}</td>
              <td class="font-mono">{{ item.length }}</td>
              <td class="font-mono break-all">{{ item.value }}</td>
              <td>{{ item.description }}</td>
            </tr>
          </tbody>
        </table>
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

const tlvInput = ref('');
const parsedData = ref([]);
const loading = ref(false);
const error = ref('');

const parseTlv = async () => {
  if (!tlvInput.value) return;
  
  loading.value = true;
  error.value = '';
  
  try {
    // 模拟解析过程
    await new Promise(resolve => setTimeout(resolve, 1000));
    
    // 这里是模拟的解析结果
    parsedData.value = [
      {
        tag: '6F',
        length: '23',
        value: '840E325041592E5359532E4444463031A511BF0C0E610C4F07A0000000031010870101',
        description: 'FCI 模板'
      },
      {
        tag: '84',
        length: '0E',
        value: '325041592E5359532E444446303',
        description: 'DF 名称'
      },
      {
        tag: 'A5',
        length: '11',
        value: 'BF0C0E610C4F07A0000000031010870101',
        description: 'FCI 专有模板'
      }
    ];
  } catch (err) {
    console.error('Failed to parse TLV data:', err);
    error.value = '数据格式错误或包含无效字符';
  } finally {
    loading.value = false;
  }
};

const loadSampleData = () => {
  tlvInput.value = '6F23840E325041592E5359532E4444463031A511BF0C0E610C4F07A0000000031010870101';
};

const clearData = () => {
  tlvInput.value = '';
  parsedData.value = [];
  error.value = '';
};
</script> 