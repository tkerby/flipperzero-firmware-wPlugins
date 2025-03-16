<template>
  <div class="min-h-screen bg-ark-bg text-ark-text">
    <!-- 顶部导航栏 -->
    <nav class="bg-ark-panel border-b border-ark-border">
      <div class="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div class="flex justify-between h-16">
          <!-- 左侧 Logo 和导航链接 -->
          <div class="flex">
            <div class="flex-shrink-0 flex items-center">
              <img class="h-8 w-auto" src="@/assets/logo.svg" alt="NFC Analysis Platform">
            </div>
            
            <div class="hidden sm:ml-6 sm:flex sm:space-x-8">
              <router-link 
                to="/" 
                class="inline-flex items-center px-1 pt-1 border-b-2"
                :class="[
                  $route.name === 'home' 
                    ? 'border-ark-accent text-ark-accent' 
                    : 'border-transparent hover:border-ark-border'
                ]"
              >
                主页
              </router-link>
              
              <router-link 
                to="/tlv" 
                class="inline-flex items-center px-1 pt-1 border-b-2"
                :class="[
                  $route.name === 'tlv' 
                    ? 'border-ark-accent text-ark-accent' 
                    : 'border-transparent hover:border-ark-border'
                ]"
              >
                TLV 解析
              </router-link>
              
              <router-link 
                to="/nard" 
                class="inline-flex items-center px-1 pt-1 border-b-2"
                :class="[
                  $route.name === 'nard' 
                    ? 'border-ark-accent text-ark-accent' 
                    : 'border-transparent hover:border-ark-border'
                ]"
              >
                NARD 解析
              </router-link>
              
              <router-link 
                to="/system" 
                class="inline-flex items-center px-1 pt-1 border-b-2"
                :class="[
                  $route.name === 'system' 
                    ? 'border-ark-accent text-ark-accent' 
                    : 'border-transparent hover:border-ark-border'
                ]"
              >
                系统信息
              </router-link>
            </div>
          </div>
          
          <!-- 右侧用户菜单 -->
          <div class="hidden sm:ml-6 sm:flex sm:items-center">
            <button 
              type="button" 
              class="bg-ark-panel-light p-1 rounded-full text-ark-text hover:text-ark-accent focus:outline-none"
            >
              <span class="sr-only">查看通知</span>
              <svg class="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15 17h5l-1.405-1.405A2.032 2.032 0 0118 14.158V11a6.002 6.002 0 00-4-5.659V5a2 2 0 10-4 0v.341C7.67 6.165 6 8.388 6 11v3.159c0 .538-.214 1.055-.595 1.436L4 17h5m6 0v1a3 3 0 11-6 0v-1m6 0H9" />
              </svg>
            </button>
          </div>
          
          <!-- 移动端菜单按钮 -->
          <div class="flex items-center sm:hidden">
            <button 
              type="button"
              @click="mobileMenuOpen = !mobileMenuOpen"
              class="inline-flex items-center justify-center p-2 rounded-md text-ark-text hover:text-ark-accent focus:outline-none"
            >
              <span class="sr-only">打开主菜单</span>
              <svg 
                :class="[mobileMenuOpen ? 'hidden' : 'block', 'h-6 w-6']"
                fill="none" 
                viewBox="0 0 24 24" 
                stroke="currentColor"
              >
                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 6h16M4 12h16M4 18h16" />
              </svg>
              <svg 
                :class="[mobileMenuOpen ? 'block' : 'hidden', 'h-6 w-6']"
                fill="none" 
                viewBox="0 0 24 24" 
                stroke="currentColor"
              >
                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12" />
              </svg>
            </button>
          </div>
        </div>
      </div>
      
      <!-- 移动端菜单 -->
      <div 
        :class="[mobileMenuOpen ? 'block' : 'hidden', 'sm:hidden']"
        class="border-t border-ark-border"
      >
        <div class="pt-2 pb-3 space-y-1">
          <router-link 
            to="/" 
            class="block pl-3 pr-4 py-2 border-l-4"
            :class="[
              $route.name === 'home'
                ? 'border-ark-accent text-ark-accent bg-ark-panel-light'
                : 'border-transparent hover:border-ark-border'
            ]"
          >
            主页
          </router-link>
          
          <router-link 
            to="/tlv" 
            class="block pl-3 pr-4 py-2 border-l-4"
            :class="[
              $route.name === 'tlv'
                ? 'border-ark-accent text-ark-accent bg-ark-panel-light'
                : 'border-transparent hover:border-ark-border'
            ]"
          >
            TLV 解析
          </router-link>
          
          <router-link 
            to="/nard" 
            class="block pl-3 pr-4 py-2 border-l-4"
            :class="[
              $route.name === 'nard'
                ? 'border-ark-accent text-ark-accent bg-ark-panel-light'
                : 'border-transparent hover:border-ark-border'
            ]"
          >
            NARD 解析
          </router-link>
          
          <router-link 
            to="/system" 
            class="block pl-3 pr-4 py-2 border-l-4"
            :class="[
              $route.name === 'system'
                ? 'border-ark-accent text-ark-accent bg-ark-panel-light'
                : 'border-transparent hover:border-ark-border'
            ]"
          >
            系统信息
          </router-link>
        </div>
      </div>
    </nav>

    <!-- 主要内容区域 -->
    <main class="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
      <router-view></router-view>
    </main>
  </div>
</template>

<script setup>
import { ref } from 'vue';

const mobileMenuOpen = ref(false);
</script>

<style>
.ark-nav-link {
  @apply px-3 py-2 text-sm text-ark-text-secondary rounded-ark
         hover:text-ark-text hover:bg-ark-hover
         transition duration-200;
}

.ark-nav-link.active {
  @apply text-ark-text bg-ark-hover;
}

.ark-sidebar-link {
  @apply flex items-center px-3 py-2 text-sm text-ark-text-secondary rounded-ark
         hover:text-ark-text hover:bg-ark-hover
         transition duration-200 space-x-2;
}

.ark-sidebar-link.active {
  @apply text-ark-text bg-ark-hover;
}

.fade-enter-active,
.fade-leave-active {
  transition: opacity 0.15s ease;
}

.fade-enter-from,
.fade-leave-to {
  opacity: 0;
}
</style> 