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
    
    <!-- NFC 全息动画 -->
    <div class="ark-panel p-0 overflow-hidden">
      <HolographicNfcAnimation />
    </div>
  </div>
</template>

<script setup>
import { DocumentTextIcon, ChipIcon, CogIcon } from '@heroicons/vue/solid';
import { useI18n } from 'vue-i18n';
import HolographicNfcAnimation from '@/components/HolographicNfcAnimation.vue';

const { t } = useI18n();
</script> 