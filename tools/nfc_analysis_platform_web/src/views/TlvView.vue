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
        
        <div class="flex flex-wrap gap-4">
          <button 
            @click="parseTLV" 
            class="px-4 py-2 rounded-md bg-ark-blue text-white hover:bg-ark-blue-dark transition-colors duration-200 flex items-center" 
            :disabled="loading"
          >
            <span v-if="loading" class="flex items-center">
              <svg class="animate-spin -ml-1 mr-2 h-4 w-4 text-white" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24">
                <circle class="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" stroke-width="4"></circle>
                <path class="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
              </svg>
              {{ t('tlv.buttons.parsing') }}
            </span>
            <span v-else>{{ t('tlv.buttons.parse') }}</span>
          </button>
          
          <button 
            @click="clearData" 
            class="px-4 py-2 rounded-md bg-ark-panel-light text-ark-text hover:bg-ark-panel border border-ark-border transition-colors duration-200" 
            :disabled="loading"
          >
            {{ t('tlv.buttons.clear') }}
          </button>
          
          <div class="flex-grow"></div>
          
          <!-- 提取特定标签 -->
          <div class="flex items-center space-x-2">
            <input
              v-model="extractTags"
              type="text"
              class="ark-input w-32"
              :placeholder="t('tlv.extract.tagsPlaceholder')"
              :disabled="loading"
            />
            
            <select 
              v-model="dataType" 
              class="ark-input w-24"
              :disabled="loading"
            >
              <option value="hex">{{ t('tlv.extract.dataTypes.hex') }}</option>
              <option value="utf8">{{ t('tlv.extract.dataTypes.utf8') }}</option>
              <option value="ascii">{{ t('tlv.extract.dataTypes.ascii') }}</option>
              <option value="numeric">{{ t('tlv.extract.dataTypes.numeric') }}</option>
            </select>
            
            <button 
              @click="extractValues" 
              class="px-4 py-2 rounded-md bg-ark-green text-white hover:bg-ark-green-dark transition-colors duration-200" 
              :disabled="loading || !hexData"
            >
              {{ t('tlv.extract.button') }}
            </button>
          </div>
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
              <th>{{ t('tlv.results.description') }}</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="(item, index) in parseResult" :key="index" :class="{'bg-ark-panel-light': item.level > 0}">
              <td class="font-mono">
                <div class="flex items-center">
                  <span v-if="item.level > 0" class="inline-block mr-2" :style="`margin-left: ${item.level * 16}px`">└</span>
                  <span 
                    @click="copyTag(item.tag)" 
                    :class="['cursor-pointer px-2 py-0.5 rounded text-xs font-medium', getTagColorClass(index)]"
                  >
                    {{ item.tag }}
                  </span>
                </div>
              </td>
              <td>{{ item.length }}</td>
              <td class="font-mono text-sm max-w-xs truncate">{{ item.value }}</td>
              <td>{{ item.description || '-' }}</td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>

    <!-- 提取结果 -->
    <div v-if="extractResult.length > 0" class="ark-panel p-6">
      <h2 class="text-xl font-medium mb-4">{{ t('tlv.extract.title') }}</h2>
      
      <div class="overflow-x-auto">
        <table class="ark-table">
          <thead>
            <tr>
              <th>{{ t('tlv.results.tag') }}</th>
              <th>{{ t('tlv.results.value') }}</th>
              <th>{{ t('tlv.results.description') }}</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="(item, index) in extractResult" :key="index">
              <td class="font-mono">
                <span 
                  @click="copyTag(item.tag)" 
                  :class="['cursor-pointer px-2 py-0.5 rounded text-xs font-medium', getTagColorClass(index)]"
                >
                  {{ item.tag }}
                </span>
              </td>
              <td class="font-mono text-sm max-w-xs truncate">{{ item.value }}</td>
              <td>{{ item.description || '-' }}</td>
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
          <h3 class="font-medium">{{ t('tlv.error.title') }}</h3>
          <p class="mt-1 text-sm">{{ error }}</p>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref } from 'vue';
import { useI18n } from 'vue-i18n';
import { parseTlvData, extractTlvValues } from '@/api/tlv';
import toast from '@/utils/toast';

const { t } = useI18n();
const hexData = ref('');
const parseResult = ref([]);
const extractTags = ref('');
const extractResult = ref([]);
const loading = ref(false);
const error = ref('');
const dataType = ref('hex'); // 默认数据类型为 hex

// 标签颜色类
const tagColorClasses = [
  'bg-ark-blue-light/10 text-ark-blue-light',
  'bg-ark-green-light/10 text-ark-green-light',
  'bg-ark-yellow-light/10 text-ark-yellow-light',
  'bg-ark-red-light/10 text-ark-red-light',
  'bg-ark-purple-light/10 text-ark-purple-light',
  'bg-ark-cyan-light/10 text-ark-cyan-light',
  'bg-ark-orange-light/10 text-ark-orange-light',
  'bg-ark-pink-light/10 text-ark-pink-light'
];

// 获取标签颜色类
const getTagColorClass = (index) => {
  // 使用标签的索引来选择颜色，确保相同的标签有相同的颜色
  return tagColorClasses[index % tagColorClasses.length];
};

// 复制标签
const copyTag = (tag) => {
  navigator.clipboard.writeText(tag)
    .then(() => {
      toast.success(t('tlv.copy.success', { tag }));
    })
    .catch(() => {
      toast.error(t('tlv.copy.error'));
    });
};

// 解析 TLV 数据
const parseTLV = async () => {
  if (!hexData.value) return;
  
  loading.value = true;
  error.value = '';
  extractResult.value = [];
  
  try {
    const result = await parseTlvData(hexData.value);
    
    // 处理嵌套 TLV 结构
    parseResult.value = flattenTlvStructure(result.structure);
  } catch (err) {
    console.error('Failed to parse TLV data:', err);
    error.value = err.message || t('tlv.error.parseError');
    parseResult.value = [];
  } finally {
    loading.value = false;
  }
};

// 提取特定标签的值
const extractValues = async () => {
  if (!hexData.value || !extractTags.value) return;
  
  loading.value = true;
  error.value = '';
  
  try {
    // 将输入的标签转换为数组
    const tags = extractTags.value.split(',').map(tag => tag.trim());
    
    const result = await extractTlvValues(hexData.value, tags, dataType.value);
    
    // 将结果转换为数组格式以便显示
    extractResult.value = Object.entries(result.values || {}).map(([tag, value]) => ({
      tag,
      value,
      description: ''
    }));
    
  } catch (err) {
    console.error('Failed to extract TLV values:', err);
    error.value = err.message || t('tlv.error.extractError');
    extractResult.value = [];
  } finally {
    loading.value = false;
  }
};

// 将嵌套的 TLV 结构扁平化为带层级的数组
const flattenTlvStructure = (tlvData, level = 0, result = []) => {
  if (!tlvData || !Array.isArray(tlvData)) return result;
  
  tlvData.forEach(item => {
    // 添加当前项，并标记其层级
    result.push({
      ...item,
      level
    });
    
    // 如果有子项，递归处理
    if (item.children && Array.isArray(item.children)) {
      flattenTlvStructure(item.children, level + 1, result);
    }
  });
  
  return result;
};

const clearData = () => {
  hexData.value = '';
  parseResult.value = [];
  extractResult.value = [];
  extractTags.value = '';
  error.value = '';
};
</script> 