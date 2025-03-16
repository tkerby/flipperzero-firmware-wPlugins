<template>
  <div class="space-y-6">
    <div class="ark-panel p-6">
      <h1 class="text-2xl font-medium mb-6">系统信息</h1>
      
      <div class="grid grid-cols-1 md:grid-cols-2 gap-6">
        <!-- 基本信息 -->
        <div class="space-y-4">
          <h2 class="text-lg font-medium text-ark-text-secondary">基本信息</h2>
          <div class="space-y-2">
            <div class="flex justify-between">
              <span class="text-ark-text-secondary">版本</span>
              <span>v1.0.0</span>
            </div>
            <div class="flex justify-between">
              <span class="text-ark-text-secondary">构建时间</span>
              <span>2024-03-16</span>
            </div>
            <div class="flex justify-between">
              <span class="text-ark-text-secondary">运行环境</span>
              <span>Node.js v18.0.0</span>
            </div>
          </div>
        </div>

        <!-- 状态信息 -->
        <div class="space-y-4">
          <h2 class="text-lg font-medium text-ark-text-secondary">状态信息</h2>
          <div class="space-y-2">
            <div class="flex justify-between">
              <span class="text-ark-text-secondary">系统状态</span>
              <span class="ark-badge ark-badge-green">运行中</span>
            </div>
            <div class="flex justify-between">
              <span class="text-ark-text-secondary">内存使用</span>
              <span>256MB / 1GB</span>
            </div>
            <div class="flex justify-between">
              <span class="text-ark-text-secondary">CPU 使用率</span>
              <span>15%</span>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- 系统日志 -->
    <div class="ark-panel p-6">
      <h2 class="text-lg font-medium mb-4">系统日志</h2>
      <div class="bg-ark-bg rounded-ark p-4 font-mono text-sm h-64 overflow-auto">
        <div v-for="(log, index) in systemLogs" :key="index" class="py-1">
          <span class="text-ark-text-secondary">{{ log.timestamp }}</span>
          <span :class="getLogLevelClass(log.level)" class="mx-2">{{ log.level }}</span>
          <span>{{ log.message }}</span>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref } from 'vue';

const systemLogs = ref([
  {
    timestamp: '2024-03-16 18:45:23',
    level: 'INFO',
    message: '系统启动完成'
  },
  {
    timestamp: '2024-03-16 18:45:22',
    level: 'INFO',
    message: '加载配置文件'
  },
  {
    timestamp: '2024-03-16 18:45:21',
    level: 'DEBUG',
    message: '初始化数据库连接'
  },
  {
    timestamp: '2024-03-16 18:45:20',
    level: 'WARN',
    message: '使用默认配置启动'
  }
]);

const getLogLevelClass = (level) => {
  const classes = {
    'INFO': 'text-ark-blue-light',
    'DEBUG': 'text-ark-green-light',
    'WARN': 'text-ark-yellow-light',
    'ERROR': 'text-ark-red-light'
  };
  return classes[level] || 'text-ark-text';
};
</script> 