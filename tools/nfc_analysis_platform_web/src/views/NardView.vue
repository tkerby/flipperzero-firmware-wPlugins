<template>
  <div class="nard-view">
    <h1 class="text-2xl font-medium mb-4">{{ t('nard.title') }}</h1>
    <p class="text-ark-text-secondary mb-6">{{ t('nard.description') }}</p>
    
    <div class="grid grid-cols-1 lg:grid-cols-3 gap-6">
      <!-- 左侧设备选择器和响应文件列表 -->
      <div class="lg:col-span-1">
        <FlipperDeviceSelector 
          :title="t('nard.deviceSelector.title')"
          @device-selected="handleDeviceSelected"
          @device-status-changed="handleDeviceStatusChanged"
        />
        
        <div class="mt-6 ark-panel p-4">
          <h4 class="text-sm font-medium mb-2">{{ t('nard.status.title') }}</h4>
          <div class="flex items-center space-x-2">
            <div 
              class="w-3 h-3 rounded-full"
              :class="{
                'bg-ark-green-light': deviceStatus === 'selected',
                'bg-ark-yellow-light': deviceStatus === 'available',
                'bg-ark-red-light': deviceStatus === 'no-device' || error
              }"
            ></div>
            <span class="text-sm">
              {{ getStatusText() }}
            </span>
          </div>
        </div>
        
        <!-- 响应文件列表 -->
        <div class="mt-6 ark-panel">
          <FlipperResponseFileList
            ref="responseFileList"
            :title="t('nard.responseFiles.title')"
            :devicePath="selectedDevice ? selectedDevice.path : ''"
            :serialPort="selectedDevice ? selectedDevice.serial_port : ''"
            :useSerial="selectedDevice ? selectedDevice.is_serial : false"
            :disabled="deviceStatus === 'no-device'"
            @file-selected="handleFileSelected"
          />
        </div>
        
        <!-- 解析按钮 -->
        <div class="mt-6">
          <AnalyzeButton 
            ref="analyzeButton"
            :disabled="!canAnalyze"
            @analyze="handleAnalyze"
            @analyzing-complete="handleAnalyzingComplete"
          />
        </div>
      </div>
      
      <!-- 右侧 APDU 解析模板选择 -->
      <div class="lg:col-span-2">
        <div class="ark-panel p-4">
          <h3 class="text-lg font-medium mb-3">{{ t('nard.templates.title') }}</h3>
          <p class="text-ark-text-secondary mb-4">{{ t('nard.templates.description') }}</p>
          
          <NfcCardGrid
            ref="templateGrid"
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
        
        <!-- 终端输出区域 -->
        <div class="mt-6 ark-panel" style="height: 300px;">
          <TerminalOutput 
            ref="terminalOutput"
            :title="t('nard.terminal.title')"
            :prompt="'>'"
          />
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
import { ref, computed, onMounted, onUnmounted, nextTick, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import { useRoute } from 'vue-router';
import FlipperDeviceSelector from '../components/FlipperDeviceSelector.vue';
import FlipperResponseFileList from '../components/FlipperResponseFileList.vue';
import NfcCard from '../components/NfcCard.vue';
import NfcCardGrid from '../components/NfcCardGrid.vue';
import AnalyzeButton from '../components/AnalyzeButton.vue';
import TerminalOutput from '../components/TerminalOutput.vue';
import axios from 'axios';
import toast from '../utils/toast.js';

const { t } = useI18n();
const route = useRoute();

// 状态变量
const selectedDevice = ref(null);
const isLoading = ref(false);
const error = ref('');
const templates = ref([]);
const selectedTemplateIndex = ref(-1);
const showTemplateDetails = ref(false);
const selectedTemplateData = ref(null); // 存储选中的模板完整信息
const deviceStatus = ref('no-device'); // 设备状态
const templateGrid = ref(null); // 引用NfcCardGrid组件
const responseFileList = ref(null); // 引用FlipperResponseFileList组件
const selectedFile = ref(null); // 存储选中的响应文件
const analyzeButton = ref(null); // 引用AnalyzeButton组件
const terminalOutput = ref(null); // 引用TerminalOutput组件

// 计算属性
const selectedTemplate = computed(() => {
  if (selectedTemplateIndex.value >= 0 && selectedTemplateIndex.value < templates.value.length) {
    return templates.value[selectedTemplateIndex.value];
  }
  return null;
});

// 判断是否可以进行解析
const canAnalyze = computed(() => {
  return (
    deviceStatus.value !== 'no-device' && // 设备已连接
    selectedDevice.value !== null && // 已选择设备
    selectedFile.value !== null && // 已选择响应文件
    selectedTemplate.value !== null // 已选择解析模板
  );
});

// 处理设备选择
const handleDeviceSelected = (device) => {
  selectedDevice.value = device;
  error.value = '';
  
  // 如果设备变更，刷新响应文件列表
  if (responseFileList.value) {
    // 无论是选择了设备还是自动选择，都刷新文件列表
    responseFileList.value.refreshFiles();
  }
};

// 处理设备状态变更
const handleDeviceStatusChanged = (status) => {
  // 先更新状态
  deviceStatus.value = status;
  
  // 根据状态更新选中设备
  if (status === 'no-device') {
    // 如果状态为"无设备"，清空选中的设备
    selectedDevice.value = null;
    // 同时清除可能的错误信息
    error.value = '';
    // 清空文件列表
    if (responseFileList.value) {
      responseFileList.value.clearFiles();
      selectedFile.value = null;
    }
  } else if (status === 'available') {
    // 如果状态为"可用"但没有选中设备或选中的不是auto，设置为自动选择
    if (!selectedDevice.value || selectedDevice.value.id !== 'auto') {
      selectedDevice.value = { id: 'auto', name: t('nard.deviceSelector.autoSelect') };
    }
    // 同时清除可能的错误信息
    error.value = '';
  }
};

// 处理文件选择
const handleFileSelected = (file) => {
  selectedFile.value = file;
};

// 获取状态文本
const getStatusText = () => {
  if (error.value) {
    return error.value;
  }
  
  if (isLoading.value) {
    return t('nard.status.loading');
  }
  
  switch (deviceStatus.value) {
    case 'no-device':
      return t('nard.status.noDevice');
    case 'available':
      return t('nard.status.deviceAvailable');
    case 'selected':
      return selectedDevice.value 
        ? t('nard.status.deviceSelected', { device: selectedDevice.value.name || selectedDevice.value.id })
        : t('nard.status.deviceAvailable');
    default:
      return t('nard.status.ready');
  }
};

// 加载模板
const loadTemplates = async () => {
  isLoading.value = true;
  error.value = '';
  
  try {
    // 调用API获取模板列表
    const response = await axios.get('/api/nard/formats');
    
    if (response.data.code === 0) {
      // 处理API返回的模板列表
      const formatTemplates = response.data.data || [];
      
      // 将API返回的数据转换为组件需要的格式
      templates.value = formatTemplates.map((template, index) => {
        // 为不同模板分配不同的主题颜色
        const themes = ['blue', 'red', 'green', 'purple', 'yellow', 'cyan', 'orange', 'pink', 'indigo', 'teal'];
        const theme = themes[index % themes.length];
        
        return {
          id: template.id,
          title: template.name || template.id.replace(/\.apdufmt$/, ''),
          // 初始时不加载内容，选中或查看时再加载
          content: '',
          path: template.path,
          theme: theme
        };
      });
      
      // 重置选中状态
      selectedTemplateIndex.value = -1;
      selectedTemplateData.value = null;
      
      // 等待加载状态结束并且DOM更新后再检查滚动条，但只检查一次
      setTimeout(async () => {
        await nextTick();
        if (templateGrid.value) {
          templateGrid.value.forceCheckScrollbar();
        }
      }, 300);
    } else {
      // 处理API错误
      error.value = response.data.message || t('nard.templates.loadError');
    }
  } catch (err) {
    console.error('加载模板失败:', err);
    error.value = t('nard.templates.loadError');
  } finally {
    isLoading.value = false;
  }
};

// 获取模板详情
const getTemplateContent = async (templateId) => {
  try {
    // 调用API获取模板内容
    const response = await axios.get(`/api/nard/formats/${templateId}`);
    console.log('获取模板内容响应:', response.data);
    
    if (response.data.code === 0) {
      // 返回API获取的模板内容
      return response.data.data.content || '';
    } else {
      console.error('获取模板内容失败:', response.data.message);
      return '';
    }
  } catch (err) {
    console.error('获取模板内容失败:', err);
    return '';
  }
};

// 选择模板
const selectTemplate = async (index) => {
  if (index >= 0 && index < templates.value.length) {
    selectedTemplateIndex.value = index;
    
    // 获取选中模板的ID
    const templateId = templates.value[index].id;
    
    // 获取模板内容
    const content = await getTemplateContent(templateId);
    
    // 更新模板内容
    if (content) {
      templates.value[index].content = content;
    }
    
    // 更新选中的模板数据
    selectedTemplateData.value = { ...templates.value[index] };
    console.log('选中模板:', selectedTemplateData.value);
  }
};

// 查看模板详情
const viewTemplateDetails = async (index) => {
  if (index >= 0 && index < templates.value.length) {
    selectedTemplateIndex.value = index;
    
    // 获取选中模板的ID
    const templateId = templates.value[index].id;
    
    // 获取模板内容
    const content = await getTemplateContent(templateId);
    
    // 更新模板内容
    if (content) {
      templates.value[index].content = content;
    }
    
    // 更新选中的模板数据
    selectedTemplateData.value = { ...templates.value[index] };
    
    // 显示详情对话框
    showTemplateDetails.value = true;
    console.log('查看模板详情:', selectedTemplateData.value);
  }
};

// 处理解析按钮点击
const handleAnalyze = async () => {
  console.log('开始解析数据...');
  
  try {
    // 1. 重新获取设备列表
    let deviceResponse;
    try {
      deviceResponse = await axios.get('/api/nard/flipper/devices');
      if (deviceResponse.data.code !== 0) {
        throw new Error(deviceResponse.data.message || t('nard.deviceSelector.errorLoading'));
      }
    } catch (err) {
      console.error('获取设备列表失败:', err);
      toast.error(t('nard.deviceSelector.errorLoading'));
      // 恢复按钮状态
      if (analyzeButton.value) {
        analyzeButton.value.setAnalyzing(false);
      }
      return;
    }
    
    // 检查选中的设备是否仍然存在
    const devices = deviceResponse.data.data || [];
    const deviceExists = selectedDevice.value.id === 'auto' || 
                         devices.some(d => d.id === selectedDevice.value.id);
    
    if (!deviceExists) {
      console.error('选中的设备不再可用');
      toast.error(t('nard.analyze.deviceNotAvailable'));
      
      // 刷新设备列表
      if (document.querySelector('.device-selector-container .refresh-button')) {
        document.querySelector('.device-selector-container .refresh-button').click();
      }
      
      // 恢复按钮状态
      if (analyzeButton.value) {
        analyzeButton.value.setAnalyzing(false);
      }
      return;
    }
    
    // 2. 重新获取响应文件内容
    let fileResponse;
    try {
      // 构建请求参数
      const params = {};
      
      if (selectedDevice.value.path) {
        params.device_path = selectedDevice.value.path;
      }
      
      if (selectedDevice.value.serial_port) {
        params.serial_port = selectedDevice.value.serial_port;
      }
      
      // 如果是自动选择设备或明确指定了使用串口，设置use_serial=true
      if (selectedDevice.value.is_serial || selectedDevice.value.id === 'auto') {
        params.use_serial = true;
      }
      
      fileResponse = await axios.get(`/api/nard/flipper/files/${selectedFile.value.id}`, { params });
      if (fileResponse.data.code !== 0) {
        throw new Error(fileResponse.data.message || t('nard.responseFiles.errorLoadingContent'));
      }
    } catch (err) {
      console.error('获取响应文件内容失败:', err);
      toast.error(t('nard.responseFiles.errorLoadingContent'));
      
      // 刷新文件列表
      if (responseFileList.value) {
        responseFileList.value.refreshFiles();
      }
      
      // 恢复按钮状态
      if (analyzeButton.value) {
        analyzeButton.value.setAnalyzing(false);
      }
      return;
    }
    
    // 更新文件内容
    const fileContent = fileResponse.data.data.content || '';
    selectedFile.value = {
      ...selectedFile.value,
      content: fileContent
    };
    
    // 3. 重新获取模板内容
    let templateResponse;
    try {
      templateResponse = await axios.get(`/api/nard/formats/${selectedTemplate.value.id}`);
      if (templateResponse.data.code !== 0) {
        throw new Error(templateResponse.data.message || t('nard.templates.loadError'));
      }
    } catch (err) {
      console.error('获取模板内容失败:', err);
      toast.error(t('nard.templates.loadError'));
      
      // 刷新模板列表
      await loadTemplates();
      
      // 恢复按钮状态
      if (analyzeButton.value) {
        analyzeButton.value.setAnalyzing(false);
      }
      return;
    }
    
    // 更新模板内容
    const templateContent = templateResponse.data.data.content || '';
    templates.value[selectedTemplateIndex.value].content = templateContent;
    selectedTemplateData.value = { ...templates.value[selectedTemplateIndex.value] };
    
    // 4. 所有数据都已更新，可以进行解析
    console.log('所有数据已更新，开始解析...');
    console.log('设备:', selectedDevice.value);
    console.log('响应文件:', selectedFile.value);
    console.log('解析模板:', selectedTemplateData.value);
    
    // 调用解析API
    const analyzeResponse = await axios.post('/api/nard/decode', {
      response_data: selectedFile.value.content,
      format_id: selectedTemplate.value.id,
      format_content: templateContent,
      debug: true
    });
    
    if (analyzeResponse.data.code === 0) {
      // 解析成功，处理解析结果
      console.log('解析结果:', analyzeResponse.data.data);
      toast.success(t('nard.analyze.success'));
      
      // 在终端输出中显示解析结果
      if (terminalOutput.value) {
        // 清空之前的输出
        terminalOutput.value.clear();
        
        // 添加解析成功的消息
        terminalOutput.value.addLines(`${t('nard.terminal.success')}: ${selectedFile.value.name} ${t('nard.terminal.using')} ${selectedTemplate.value.title}`);
        terminalOutput.value.addLines('------------------------');
        
        // 显示解析结果
        const result = analyzeResponse.data.data;
        
        // 显示原始响应数据
        terminalOutput.value.addLines(t('nard.terminal.originalData') + ':');
        terminalOutput.value.addLines(selectedFile.value.content);
        terminalOutput.value.addLines('');
        
        // 显示解析后的数据
        terminalOutput.value.addLines(t('nard.terminal.parseResult') + ':');
        
        // 如果结果是对象，格式化显示
        if (typeof result === 'object') {
          // 递归处理解析结果
          const processResult = (data, prefix = '') => {
            if (Array.isArray(data)) {
              data.forEach((item, index) => {
                if (typeof item === 'object' && item !== null) {
                  terminalOutput.value.addLines(`${prefix}[${index}]:`);
                  processResult(item, `${prefix}  `);
                } else {
                  terminalOutput.value.addLines(`${prefix}[${index}]: ${item}`);
                }
              });
            } else if (typeof data === 'object' && data !== null) {
              Object.entries(data).forEach(([key, value]) => {
                if (typeof value === 'object' && value !== null) {
                  terminalOutput.value.addLines(`${prefix}${key}:`);
                  processResult(value, `${prefix}  `);
                } else {
                  // 如果是output字段，在值前添加一个换行符
                  if (key === 'output') {
                    terminalOutput.value.addLines(`${prefix}${key}:`);
                    terminalOutput.value.addLines(`${prefix}${value}`);
                  } else {
                    terminalOutput.value.addLines(`${prefix}${key}: ${value}`);
                  }
                }
              });
            }
          };
          
          processResult(result);
        } else {
          // 如果是字符串或其他类型，直接显示
          terminalOutput.value.addLines(String(result));
        }
      }
    } else {
      // 解析失败，处理错误
      console.error('解析失败:', analyzeResponse.data.message);
      toast.error(analyzeResponse.data.message || t('nard.analyze.error'));
      
      // 在终端输出中显示错误信息
      if (terminalOutput.value) {
        terminalOutput.value.addLines(`${t('nard.terminal.error')}: ${analyzeResponse.data.message || t('nard.analyze.error')}`);
      }
    }
    
    // 恢复按钮状态
    if (analyzeButton.value) {
      analyzeButton.value.setAnalyzing(false);
    }
    
    // 触发解析完成事件
    handleAnalyzingComplete();
  } catch (err) {
    console.error('解析过程中发生错误:', err);
    toast.error(t('nard.analyze.error'));
    
    // 恢复按钮状态
    if (analyzeButton.value) {
      analyzeButton.value.setAnalyzing(false);
    }
  }
};

// 处理解析完成
const handleAnalyzingComplete = () => {
  console.log('解析完成');
  
  // 这里可以添加解析完成后的处理逻辑
};

// 组件挂载时加载模板
onMounted(async () => {
  // 先加载模板
  await loadTemplates();
  
  // 初始化终端输出
  if (terminalOutput.value) {
    terminalOutput.value.addLines(t('nard.terminal.welcome'));
    terminalOutput.value.addLines(t('nard.terminal.instruction'));
    terminalOutput.value.addLines('------------------------');
    terminalOutput.value.addLines(t('nard.terminal.ready'));
  }
  
  // 监听窗口大小变化，更新滚动条
  const handleResize = () => {
    if (templateGrid.value) {
      templateGrid.value.updateScrollbar();
    }
  };
  
  window.addEventListener('resize', handleResize);
  
  // 组件卸载时移除事件监听
  onUnmounted(() => {
    window.removeEventListener('resize', handleResize);
  });
});

// 监听模板数据变化
watch(templates, () => {
  if (templates.value.length > 0) {
    // 模板数据变化且不为空，延迟更新滚动条，但只检查一次
    setTimeout(() => {
      if (templateGrid.value) {
        templateGrid.value.forceCheckScrollbar();
      }
    }, 300);
  }
}, { deep: true });

// 监听isLoading状态变化
watch(isLoading, (newVal, oldVal) => {
  if (oldVal && !newVal && templates.value.length > 0) {
    // 加载状态结束，且有模板数据，检查滚动条，但只检查一次
    setTimeout(() => {
      if (templateGrid.value) {
        templateGrid.value.forceCheckScrollbar();
      }
    }, 300);
  }
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