<template>
  <div class="space-y-6">
    <div class="ark-panel p-6">
      <h1 class="text-2xl font-medium mb-4">{{ t('home.welcome') }}</h1>
      <p class="text-ark-text-secondary">{{ t('home.description') }}</p>
    </div>

    <div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
      <!-- TLV 解析卡片 -->
      <router-link to="/tlv" class="ark-card group">
        <div class="flex items-start space-x-4">
          <div class="p-2 bg-ark-blue/10 rounded-ark">
            <DocumentTextIcon class="w-6 h-6 text-ark-blue-light" />
          </div>
          <div>
            <h3 class="text-lg font-medium group-hover:text-ark-blue-light transition-colors duration-200">
              {{ t('home.features.tlv.title') }}
            </h3>
            <p class="mt-1 text-sm text-ark-text-secondary">{{ t('home.features.tlv.description') }}</p>
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
            <h3 class="text-lg font-medium group-hover:text-ark-green-light transition-colors duration-200">
              {{ t('home.features.nard.title') }}
            </h3>
            <p class="mt-1 text-sm text-ark-text-secondary">{{ t('home.features.nard.description') }}</p>
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
            <h3 class="text-lg font-medium group-hover:text-ark-yellow-light transition-colors duration-200">
              {{ t('home.features.system.title') }}
            </h3>
            <p class="mt-1 text-sm text-ark-text-secondary">{{ t('home.features.system.description') }}</p>
          </div>
        </div>
      </router-link>
    </div>

    <!-- 最近活动 -->
    <div class="ark-panel p-6">
      <h2 class="text-xl font-medium mb-4">{{ t('home.recentActivities.title') }}</h2>
      <table class="ark-table">
        <thead>
          <tr>
            <th>{{ t('home.recentActivities.time') }}</th>
            <th>{{ t('home.recentActivities.type') }}</th>
            <th>{{ t('home.recentActivities.description') }}</th>
            <th>{{ t('home.recentActivities.status') }}</th>
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
            <td>{{ t(activity.descriptionKey) }}</td>
            <td>
              <span :class="['ark-badge', activity.statusClass]">
                {{ t(`common.status.${activity.status}`) }}
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
import { useI18n } from 'vue-i18n';

const { t } = useI18n();

const recentActivities = ref([
  {
    time: '2024-03-16 18:30',
    type: 'TLV',
    typeClass: 'ark-badge-blue',
    descriptionKey: 'home.recentActivities.items.tlvPaymentCard',
    status: 'success',
    statusClass: 'ark-badge-green'
  },
  {
    time: '2024-03-16 18:15',
    type: 'NARD',
    typeClass: 'ark-badge-green',
    descriptionKey: 'home.recentActivities.items.nardFlipperData',
    status: 'success',
    statusClass: 'ark-badge-green'
  },
  {
    time: '2024-03-16 17:45',
    type: 'TLV',
    typeClass: 'ark-badge-blue',
    descriptionKey: 'home.recentActivities.items.tlvAccessCard',
    status: 'failed',
    statusClass: 'ark-badge-red'
  }
]);
</script> 