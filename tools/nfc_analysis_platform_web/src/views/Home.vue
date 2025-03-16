<template>
  <div class="space-y-6">
    <div class="ark-panel p-6">
      <h1 class="text-2xl font-medium mb-4">欢迎使用 NFC Analysis Platform</h1>
      <p class="text-ark-text-secondary">这是一个专业的 NFC 数据分析平台，提供 TLV 和 NARD 格式的数据解析功能。</p>
    </div>

    <div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
      <!-- TLV 解析卡片 -->
      <router-link to="/tlv" class="ark-card group">
        <div class="flex items-start space-x-4">
          <div class="p-2 bg-ark-blue/10 rounded-ark">
            <DocumentTextIcon class="w-6 h-6 text-ark-blue-light" />
          </div>
          <div>
            <h3 class="text-lg font-medium group-hover:text-ark-blue-light transition-colors duration-200">TLV 解析</h3>
            <p class="mt-1 text-sm text-ark-text-secondary">解析 TLV (Tag-Length-Value) 格式的 NFC 数据。</p>
          </div>
        </div>
      </router-link>

      <!-- NARD 解析卡片 -->
      <router-link to="/nard" class="ark-card group">
        <div class="flex items-start space-x-4">
          <div class="p-2 bg-ark-green/10 rounded-ark">
            <ChipIcon class="w-6 h-6 text-ark-green-light" />
          </div>
          <div>
            <h3 class="text-lg font-medium group-hover:text-ark-green-light transition-colors duration-200">NARD 解析</h3>
            <p class="mt-1 text-sm text-ark-text-secondary">解析 NARD 格式的 NFC 通信数据。</p>
          </div>
        </div>
      </router-link>

      <!-- 系统信息卡片 -->
      <router-link to="/system" class="ark-card group">
        <div class="flex items-start space-x-4">
          <div class="p-2 bg-ark-yellow/10 rounded-ark">
            <CogIcon class="w-6 h-6 text-ark-yellow-light" />
          </div>
          <div>
            <h3 class="text-lg font-medium group-hover:text-ark-yellow-light transition-colors duration-200">系统信息</h3>
            <p class="mt-1 text-sm text-ark-text-secondary">查看系统状态和配置信息。</p>
          </div>
        </div>
      </router-link>
    </div>

    <!-- 最近活动 -->
    <div class="ark-panel p-6">
      <h2 class="text-xl font-medium mb-4">最近活动</h2>
      <table class="ark-table">
        <thead>
          <tr>
            <th>时间</th>
            <th>类型</th>
            <th>描述</th>
            <th>状态</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="(activity, index) in recentActivities" :key="index">
            <td class="text-ark-text-secondary">{{ activity.time }}</td>
            <td>
              <span :class="['ark-badge', activity.typeClass]">
                {{ activity.type }}
              </span>
            </td>
            <td>{{ activity.description }}</td>
            <td>
              <span :class="['ark-badge', activity.statusClass]">
                {{ activity.status }}
              </span>
            </td>
          </tr>
        </tbody>
      </table>
    </div>
  </div>
</template>

<script setup>
import { ref } from 'vue';
import { DocumentTextIcon, ChipIcon, CogIcon } from '@heroicons/vue/solid';

const recentActivities = ref([
  {
    time: '2024-03-16 18:30',
    type: 'TLV',
    typeClass: 'ark-badge-blue',
    description: '解析支付卡 TLV 数据',
    status: '成功',
    statusClass: 'ark-badge-green'
  },
  {
    time: '2024-03-16 18:15',
    type: 'NARD',
    typeClass: 'ark-badge-green',
    description: '解析 Flipper Zero 捕获数据',
    status: '成功',
    statusClass: 'ark-badge-green'
  },
  {
    time: '2024-03-16 17:45',
    type: 'TLV',
    typeClass: 'ark-badge-blue',
    description: '解析门禁卡数据',
    status: '失败',
    statusClass: 'ark-badge-red'
  }
]);
</script> 