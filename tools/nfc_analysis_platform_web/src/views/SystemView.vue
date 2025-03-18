<template>
  <div class="space-y-6">
    <!-- 系统信息 -->
    <div class="ark-panel p-6">
      <h1 class="text-2xl font-medium mb-4">{{ t('system.title') }}</h1>
      <p class="text-ark-text-secondary mb-6">{{ t('system.description') }}</p>

      <div class="grid grid-cols-1 md:grid-cols-2 gap-6">
        <div class="p-4 bg-ark-panel-light rounded-lg">
          <h3 class="text-sm font-medium mb-2">{{ t('system.info.version') }}</h3>
          <p class="text-2xl font-mono">{{ systemInfo.version || '-' }}</p>
        </div>
        <div class="p-4 bg-ark-panel-light rounded-lg">
          <h3 class="text-sm font-medium mb-2">{{ t('system.info.buildDate') }}</h3>
          <p class="text-2xl font-mono">{{ formatDate(systemInfo.build_date) || '-' }}</p>
        </div>
        <div class="p-4 bg-ark-panel-light rounded-lg">
          <h3 class="text-sm font-medium mb-2">{{ t('system.info.os') }}</h3>
          <p class="text-2xl font-mono">{{ systemInfo.os || '-' }}</p>
        </div>
        <div class="p-4 bg-ark-panel-light rounded-lg">
          <h3 class="text-sm font-medium mb-2">{{ t('system.info.arch') }}</h3>
          <p class="text-2xl font-mono">{{ systemInfo.arch || '-' }}</p>
        </div>
        <div class="p-4 bg-ark-panel-light rounded-lg">
          <h3 class="text-sm font-medium mb-2">{{ t('system.info.goVersion') }}</h3>
          <p class="text-2xl font-mono">{{ systemInfo.go_version || '-' }}</p>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue';
import { useI18n } from 'vue-i18n';
import { getSystemInfo } from '@/api/system';

const { t } = useI18n();
const systemInfo = ref({});

// 格式化日期
const formatDate = (dateString) => {
  if (!dateString) return '-';
  return new Date(dateString).toLocaleString();
};

// 获取系统信息
const fetchSystemInfo = async () => {
  try {
    const data = await getSystemInfo();
    systemInfo.value = data;
  } catch (error) {
    console.error('Failed to fetch system info:', error);
  }
};

// 只在组件挂载时获取一次系统信息
onMounted(() => {
  fetchSystemInfo();
});
</script> 