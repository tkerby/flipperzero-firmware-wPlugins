<template>
  <div class="space-y-6">
    <!-- 输入区域 -->
    <div class="ark-panel p-6">
      <h1 class="text-2xl font-medium mb-4">{{ t('tlv.title') }}</h1>
      <p class="text-ark-text-secondary mb-6">{{ t('tlv.description') }}</p>
      
      <div class="space-y-4">
        <div>
          <label class="block text-sm font-medium mb-2">{{ t('tlv.input.label') }}</label>
          <textarea
            v-model="hexData"
            rows="4"
            class="ark-input"
            :placeholder="t('tlv.input.placeholder')"
          ></textarea>
        </div>
        
        <div class="flex space-x-4">
          <button @click="parseTLV" class="ark-button-primary">
            {{ t('tlv.buttons.parse') }}
          </button>
          <button @click="loadSample" class="ark-button-secondary">
            {{ t('tlv.buttons.loadSample') }}
          </button>
          <button @click="clearData" class="ark-button-secondary">
            {{ t('tlv.buttons.clear') }}
          </button>
        </div>
      </div>
    </div>

    <!-- 解析结果 -->
    <div v-if="parseResult.length > 0" class="ark-panel p-6">
      <h2 class="text-xl font-medium mb-4">{{ t('tlv.results.title') }}</h2>
      
      <div class="overflow-x-auto">
        <table class="ark-table">
          <thead>
            <tr>
              <th>{{ t('tlv.results.tag') }}</th>
              <th>{{ t('tlv.results.length') }}</th>
              <th>{{ t('tlv.results.value') }}</th>
              <th>{{ t('tlv.results.raw') }}</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="(item, index) in parseResult" :key="index">
              <td>{{ item.tag }}</td>
              <td>{{ item.length }}</td>
              <td>{{ item.value }}</td>
              <td class="font-mono text-sm">{{ item.raw }}</td>
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
import { useI18n } from 'vue-i18n';

const { t } = useI18n();
const hexData = ref('');
const parseResult = ref([]);
const loading = ref(false);
const error = ref('');

const parseTLV = async () => {
  if (!hexData.value) return;
  
  loading.value = true;
  error.value = '';
  
  try {
    // 模拟解析过程
    await new Promise(resolve => setTimeout(resolve, 1000));
    
    // 这里是模拟的解析结果
    parseResult.value = [
      {
        tag: '6F',
        length: '20',
        value: 'File Control Information (FCI) Template',
        raw: '6F20840E325041592E5359532E4444463031A50E8801015F2D046672656E'
      }
    ];
  } catch (err) {
    console.error('Failed to parse TLV data:', err);
    error.value = '数据格式错误或包含无效字符';
  } finally {
    loading.value = false;
  }
};

const loadSample = () => {
  hexData.value = '6F20840E325041592E5359532E4444463031A50E8801015F2D046672656E';
};

const clearData = () => {
  hexData.value = '';
  parseResult.value = [];
  error.value = '';
};
</script> 