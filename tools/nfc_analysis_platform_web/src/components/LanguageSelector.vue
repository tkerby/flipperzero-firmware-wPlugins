<template>
  <div class="language-selector">
    <button
      @click="toggleDropdown"
      class="flex items-center px-3 py-2 text-ark-text hover:bg-ark-hover rounded-md"
    >
      <span class="mr-2">{{ languages[currentLanguage] }}</span>
      <svg
        class="w-4 h-4"
        :class="{ 'transform rotate-180': isOpen }"
        fill="none"
        stroke="currentColor"
        viewBox="0 0 24 24"
      >
        <path
          stroke-linecap="round"
          stroke-linejoin="round"
          stroke-width="2"
          d="M19 9l-7 7-7-7"
        />
      </svg>
    </button>

    <div
      v-if="isOpen"
      class="absolute right-0 mt-2 py-2 w-48 bg-ark-panel rounded-md shadow-ark z-50"
      style="min-width: 120px;"
    >
      <button
        v-for="(name, lang) in languages"
        :key="lang"
        @click="changeLanguage(lang)"
        class="block w-full px-4 py-2 text-left hover:bg-ark-hover text-ark-text"
        :class="{ 'text-ark-accent': currentLanguage === lang }"
      >
        {{ name }}
      </button>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { useI18n } from 'vue-i18n'
import { setLanguage } from '../i18n'

const { locale } = useI18n()
const isOpen = ref(false)
const currentLanguage = computed(() => locale.value)

const languages = {
  'en': 'English',
  'zh': '中文'
}

const toggleDropdown = () => {
  isOpen.value = !isOpen.value
}

const changeLanguage = (lang) => {
  setLanguage(lang)
  isOpen.value = false
}

// 点击外部关闭下拉菜单
const closeDropdown = (e) => {
  if (!e.target.closest('.language-selector')) {
    isOpen.value = false
  }
}

// 添加和移除事件监听器
onMounted(() => {
  document.addEventListener('click', closeDropdown)
})

onUnmounted(() => {
  document.removeEventListener('click', closeDropdown)
})
</script>

<style scoped>
.language-selector {
  @apply relative inline-block;
}

.language-selector button {
  @apply whitespace-nowrap;
}
</style> 