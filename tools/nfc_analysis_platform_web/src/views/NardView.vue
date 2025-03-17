<template>
  <div class="nard-view">
    <h1 class="text-2xl font-medium mb-4">{{ t('nard.title') }}</h1>
    <p class="text-ark-text-secondary mb-6">{{ t('nard.description') }}</p>
    
    <div class="grid grid-cols-1 lg:grid-cols-3 gap-6">
      <!-- 左侧设备选择器 -->
      <div class="lg:col-span-1">
        <FlipperDeviceSelector 
          :title="t('nard.deviceSelector.title')"
          @device-selected="handleDeviceSelected"
        />
        
        <div class="mt-6 ark-panel p-4">
          <h4 class="text-sm font-medium mb-2">{{ t('nard.status.title') }}</h4>
          <div class="flex items-center space-x-2">
            <div 
              class="w-3 h-3 rounded-full"
              :class="{
                'bg-ark-green-light': selectedDevice && !error,
                'bg-ark-red-light': error,
                'bg-ark-yellow-light': !selectedDevice && !error
              }"
            ></div>
            <span class="text-sm">
              {{ getStatusText() }}
            </span>
          </div>
        </div>
      </div>
      
      <!-- 右侧 APDU 解析模板选择 -->
      <div class="lg:col-span-2">
        <div class="ark-panel p-4">
          <h3 class="text-lg font-medium mb-3">{{ t('nard.templates.title') }}</h3>
          <p class="text-ark-text-secondary mb-4">{{ t('nard.templates.description') }}</p>
          
          <NfcCardGrid
            :loading="isLoading"
            :error="error"
            :isEmpty="!templates.length && !isLoading && !error"
            :loadingText="t('nard.templates.loading')"
            :emptyText="t('nard.templates.empty')"
            :onRetry="loadTemplates"
            :columns="2"
            :gap="16"
            :maxHeight="500"
          >
            <NfcCard
              v-for="(template, index) in templates"
              :key="index"
              :title="template.title"
              :id="template.id"
              :content="template.content"
              :theme="template.theme"
              :selected="selectedTemplateIndex === index"
              :viewText="t('nard.templates.use')"
              @click="selectTemplate(index)"
              @view="viewTemplateDetails(index)"
            >
            </NfcCard>
          </NfcCardGrid>
        </div>
      </div>
    </div>
    
    <!-- 模板详情对话框 -->
    <div 
      v-if="showTemplateDetails" 
      class="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50"
      @click.self="showTemplateDetails = false"
    >
      <div class="bg-ark-panel rounded-lg w-full max-w-3xl max-h-[80vh] overflow-hidden flex flex-col">
        <div class="flex justify-between items-center p-4 border-b border-ark-border">
          <h3 class="text-lg font-medium">{{ selectedTemplate ? selectedTemplate.title : '' }}</h3>
          <button @click="showTemplateDetails = false" class="text-ark-text-secondary hover:text-ark-text">
            <svg xmlns="http://www.w3.org/2000/svg" class="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12" />
            </svg>
          </button>
        </div>
        
        <div class="p-4 overflow-y-auto flex-grow">
          <div v-if="selectedTemplate" class="space-y-4">
            <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
              <div class="p-3 bg-ark-panel rounded-lg">
                <h4 class="text-xs text-ark-text-secondary mb-1">{{ t('nard.templates.id') }}</h4>
                <p class="font-mono">{{ selectedTemplate.id }}</p>
              </div>
              
              <div class="p-3 bg-ark-panel rounded-lg">
                <h4 class="text-xs text-ark-text-secondary mb-1">{{ t('nard.templates.title') }}</h4>
                <p class="font-medium">{{ selectedTemplate.title }}</p>
              </div>
            </div>
            
            <div class="border-t border-ark-border pt-4">
              <h4 class="text-sm font-medium mb-3">{{ t('nard.templates.content') }}</h4>
              <div class="bg-ark-panel rounded-lg p-3 overflow-x-auto">
                <pre class="text-xs font-mono whitespace-pre-wrap">{{ selectedTemplate.content }}</pre>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue';
import { useI18n } from 'vue-i18n';
import FlipperDeviceSelector from '../components/FlipperDeviceSelector.vue';
import NfcCard from '../components/NfcCard.vue';
import NfcCardGrid from '../components/NfcCardGrid.vue';
import axios from 'axios';

const { t } = useI18n();

// 状态变量
const selectedDevice = ref(null);
const isLoading = ref(false);
const error = ref('');
const templates = ref([]);
const selectedTemplateIndex = ref(-1);
const showTemplateDetails = ref(false);
const selectedTemplateData = ref(null); // 存储选中的模板完整信息

// 计算属性
const selectedTemplate = computed(() => {
  if (selectedTemplateIndex.value >= 0 && selectedTemplateIndex.value < templates.value.length) {
    return templates.value[selectedTemplateIndex.value];
  }
  return null;
});

// 处理设备选择
const handleDeviceSelected = (device) => {
  selectedDevice.value = device;
  error.value = '';
};

// 获取状态文本
const getStatusText = () => {
  if (error.value) {
    return error.value;
  }
  
  if (isLoading.value) {
    return t('nard.status.loading');
  }
  
  if (!selectedDevice.value) {
    return t('nard.status.noDevice');
  }
  
  return t('nard.status.ready');
};

// 加载模板
const loadTemplates = async () => {
  isLoading.value = true;
  error.value = '';
  
  try {
    // 模拟加载过程，实际应用中应该调用 API
    await new Promise(resolve => setTimeout(resolve, 1000));
    
    // 模拟数据
    templates.value = [
      {
        id: 'EMV.apdufmt',
        title: 'EMV',
        content: 'EMV Card Information\nApplication Label: {O[1]TAG(50), "ascii"}\nCard Number: {O[3]TAG(9F6B)[0:16], "numeric"}\nExpiry Date: Year: 20{O[3]TAG(9F6B)[17:19], "numeric"}, Month: {O[3]TAG(9F6B)[19:21], "numeric"}',
        theme: 'blue'
      },
      {
        id: 'PBOC.apdufmt',
        title: 'PBOC',
        content: 'PBOC\nCardNumber:{O[2][8:27]}\nName:{O[3]TAG(5f20), "utf-8"}',
        theme: 'red'
      },
      {
        id: 'TRAVEL_CARD_SH.apdufmt',
        title: 'TRAVEL_CARD_SH',
        content: '交通联合公交卡（上海）\n卡号:{O[1]TAG(9f0c)[21:40]}\n有效时间:{O[1]TAG(9f0c)[40:44]}.{O[1]TAG(9f0c)[44:46]}.{O[1]TAG(9f0c)[46:48]}-{O[1]TAG(9f0c)[48:52]}.{O[1]TAG(9f0c)[52:54]}.{O[1]TAG(9f0c)[54:56]}',
        theme: 'green'
      },
      {
        id: 'MIFARE_CLASSIC.apdufmt',
        title: 'MIFARE_CLASSIC',
        content: 'MIFARE Classic Card\nUID: {O[0][0:8]}\nManufacturer: {O[0][8:10]}\nCard Type: {O[0][10:12]}',
        theme: 'purple'
      }
    ];
  } catch (err) {
    console.error('Failed to load templates:', err);
    error.value = t('nard.templates.loadError');
  } finally {
    isLoading.value = false;
  }
};

// 选择模板
const selectTemplate = (index) => {
  if (index >= 0 && index < templates.value.length) {
    selectedTemplateIndex.value = index;
    selectedTemplateData.value = { ...templates.value[index] };
    console.log('Selected template:', selectedTemplateData.value);
  }
};

// 查看模板详情
const viewTemplateDetails = (index) => {
  if (index >= 0 && index < templates.value.length) {
    selectedTemplateIndex.value = index;
    selectedTemplateData.value = { ...templates.value[index] };
    showTemplateDetails.value = true;
    console.log('Viewing template details:', selectedTemplateData.value);
  }
};

// 组件挂载时加载模板
onMounted(() => {
  loadTemplates();
});
</script>

<style scoped>
.nard-view {
  padding: 1.5rem;
}

/* 自定义按钮样式 */
:deep(.ark-btn-primary) {
  @apply bg-ark-blue-light/20 text-ark-blue-light border border-ark-blue-light/30 hover:bg-ark-blue-light/30 transition-colors py-2 px-4 rounded-md;
}

:deep(.ark-btn-secondary) {
  @apply bg-ark-panel text-ark-text border border-ark-border hover:bg-ark-border/20 transition-colors py-2 px-4 rounded-md;
}

:deep(.ark-btn-danger) {
  @apply bg-ark-red-light/20 text-ark-red-light border border-ark-red-light/30 hover:bg-ark-red-light/30 transition-colors py-2 px-4 rounded-md;
}

:deep(.ark-panel) {
  @apply bg-ark-panel rounded-lg border border-ark-border;
}

:deep(.ark-panel-light) {
  @apply bg-ark-panel rounded-lg;
}
</style> 