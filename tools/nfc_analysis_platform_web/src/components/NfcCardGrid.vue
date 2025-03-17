<template>
  <div 
    class="nfc-card-grid"
    :class="{ 'grid-loading': loading }"
  >
    <div v-if="loading" class="grid-loading-overlay">
      <div class="loading-spinner">
        <div class="spinner-ring"></div>
        <div class="spinner-core"></div>
      </div>
      <div class="loading-text">{{ loadingText || t('common.loading') }}</div>
    </div>
    
    <div v-else-if="error" class="grid-error">
      <div class="error-icon">
        <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
          <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z" />
        </svg>
      </div>
      <div class="error-message">{{ error }}</div>
      <button v-if="onRetry" @click="onRetry" class="retry-button">
        {{ t('common.retry') }}
      </button>
    </div>
    
    <div v-else-if="isEmpty" class="grid-empty">
      <div class="empty-icon">
        <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
          <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M20 13V6a2 2 0 00-2-2H6a2 2 0 00-2 2v7m16 0v5a2 2 0 01-2 2H6a2 2 0 01-2-2v-5m16 0h-2.586a1 1 0 00-.707.293l-2.414 2.414a1 1 0 01-.707.293h-3.172a1 1 0 01-.707-.293l-2.414-2.414A1 1 0 006.586 13H4" />
        </svg>
      </div>
      <div class="empty-message">{{ emptyText || t('common.noData') }}</div>
    </div>
    
    <div v-else class="grid-content" :style="gridStyle">
      <slot></slot>
    </div>
    
    <div class="grid-scrollbar" v-if="showScrollbar && !loading && !error && !isEmpty">
      <div 
        class="scrollbar-track"
        @mousedown="startDrag"
      >
        <div 
          class="scrollbar-thumb"
          :style="{ top: `${thumbPosition}%`, height: `${thumbSize}%` }"
          ref="scrollThumb"
        ></div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted, watch } from 'vue';
import { useI18n } from 'vue-i18n';

const { t } = useI18n();

const props = defineProps({
  loading: {
    type: Boolean,
    default: false
  },
  error: {
    type: String,
    default: ''
  },
  isEmpty: {
    type: Boolean,
    default: false
  },
  loadingText: {
    type: String,
    default: ''
  },
  emptyText: {
    type: String,
    default: ''
  },
  onRetry: {
    type: Function,
    default: null
  },
  columns: {
    type: [Number, Object],
    default: 3
  },
  gap: {
    type: [Number, String],
    default: 16
  },
  maxHeight: {
    type: [Number, String],
    default: null
  }
});

// 计算网格样式
const gridStyle = computed(() => {
  const style = {};
  
  // 处理列数
  if (typeof props.columns === 'number') {
    style.gridTemplateColumns = `repeat(${props.columns}, 1fr)`;
  } else if (typeof props.columns === 'object') {
    const { sm, md, lg, xl } = props.columns;
    style.gridTemplateColumns = `repeat(${sm || 1}, 1fr)`;
    style['@media (min-width: 640px)'] = { gridTemplateColumns: `repeat(${md || 2}, 1fr)` };
    style['@media (min-width: 1024px)'] = { gridTemplateColumns: `repeat(${lg || 3}, 1fr)` };
    style['@media (min-width: 1280px)'] = { gridTemplateColumns: `repeat(${xl || 4}, 1fr)` };
  }
  
  // 处理间距
  style.gap = typeof props.gap === 'number' ? `${props.gap}px` : props.gap;
  
  // 处理最大高度
  if (props.maxHeight) {
    style.maxHeight = typeof props.maxHeight === 'number' ? `${props.maxHeight}px` : props.maxHeight;
  }
  
  return style;
});

// 滚动条相关
const gridContent = ref(null);
const scrollThumb = ref(null);
const thumbPosition = ref(0);
const thumbSize = ref(20);
const isDragging = ref(false);
const startY = ref(0);
const startThumbTop = ref(0);

// 是否显示滚动条
const showScrollbar = computed(() => {
  return props.maxHeight !== null;
});

// 更新滚动条位置和大小
const updateScrollbar = () => {
  if (!gridContent.value) return;
  
  const { scrollHeight, clientHeight, scrollTop } = gridContent.value;
  
  // 计算滑块大小
  thumbSize.value = Math.max(10, (clientHeight / scrollHeight) * 100);
  
  // 计算滑块位置
  thumbPosition.value = (scrollTop / (scrollHeight - clientHeight)) * (100 - thumbSize.value);
};

// 开始拖动
const startDrag = (e) => {
  if (!scrollThumb.value || !gridContent.value) return;
  
  // 如果点击的是滑道而不是滑块，直接跳转到点击位置
  if (e.target !== scrollThumb.value) {
    const trackRect = e.currentTarget.getBoundingClientRect();
    const clickPosition = (e.clientY - trackRect.top) / trackRect.height;
    const { scrollHeight, clientHeight } = gridContent.value;
    
    gridContent.value.scrollTop = clickPosition * (scrollHeight - clientHeight);
    return;
  }
  
  isDragging.value = true;
  startY.value = e.clientY;
  startThumbTop.value = thumbPosition.value;
  
  document.addEventListener('mousemove', onDrag);
  document.addEventListener('mouseup', stopDrag);
};

// 拖动中
const onDrag = (e) => {
  if (!isDragging.value || !gridContent.value) return;
  
  const { scrollHeight, clientHeight } = gridContent.value;
  const trackHeight = 100; // 百分比
  
  // 计算拖动距离（百分比）
  const deltaY = (e.clientY - startY.value) / (trackHeight * window.innerHeight / 100);
  const newThumbPosition = Math.max(0, Math.min(100 - thumbSize.value, startThumbTop.value + deltaY * 100));
  
  // 更新滑块位置
  thumbPosition.value = newThumbPosition;
  
  // 更新内容滚动位置
  gridContent.value.scrollTop = (newThumbPosition / (100 - thumbSize.value)) * (scrollHeight - clientHeight);
};

// 停止拖动
const stopDrag = () => {
  isDragging.value = false;
  document.removeEventListener('mousemove', onDrag);
  document.removeEventListener('mouseup', stopDrag);
};

// 监听内容滚动
const onScroll = () => {
  if (!isDragging.value) {
    updateScrollbar();
  }
};

// 组件挂载时
onMounted(() => {
  gridContent.value = document.querySelector('.grid-content');
  
  if (gridContent.value) {
    gridContent.value.addEventListener('scroll', onScroll);
    updateScrollbar();
  }
  
  // 监听窗口大小变化
  window.addEventListener('resize', updateScrollbar);
});

// 组件卸载时
onUnmounted(() => {
  if (gridContent.value) {
    gridContent.value.removeEventListener('scroll', onScroll);
  }
  
  window.removeEventListener('resize', updateScrollbar);
  document.removeEventListener('mousemove', onDrag);
  document.removeEventListener('mouseup', stopDrag);
});

// 监听 loading 状态变化
watch(() => props.loading, (newVal) => {
  if (!newVal) {
    // 加载完成后更新滚动条
    setTimeout(updateScrollbar, 100);
  }
});
</script>

<style scoped>
.nfc-card-grid {
  position: relative;
  width: 100%;
  height: 100%;
}

.grid-content {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 20px;
  width: 100%;
  padding: 4px; /* 添加内边距，确保卡片发光效果不被裁剪 */
  overflow-y: auto;
  scrollbar-width: none; /* Firefox */
  -ms-overflow-style: none; /* IE and Edge */
  padding-right: 24px; /* 增加右侧内边距，避免与滚动条重合 */
}

.grid-content::-webkit-scrollbar {
  display: none; /* Chrome, Safari, Opera */
}

.grid-loading-overlay {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  background-color: rgba(13, 17, 23, 0.7);
  backdrop-filter: blur(4px);
  z-index: 10;
  border-radius: 8px;
}

.loading-spinner {
  position: relative;
  width: 48px;
  height: 48px;
  margin-bottom: 16px;
}

.spinner-ring {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  border: 3px solid rgba(56, 189, 248, 0.1);
  border-top-color: rgba(56, 189, 248, 0.8);
  border-radius: 50%;
  animation: spin 1.2s linear infinite;
}

.spinner-core {
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  width: 30%;
  height: 30%;
  background-color: rgba(56, 189, 248, 0.2);
  border-radius: 50%;
  box-shadow: 0 0 10px rgba(56, 189, 248, 0.4);
  animation: pulse 1.2s ease-in-out infinite alternate;
}

.loading-text {
  font-size: 14px;
  color: rgba(201, 209, 217, 0.9);
}

.grid-error,
.grid-empty {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  padding: 32px;
  text-align: center;
  min-height: 200px;
}

.error-icon,
.empty-icon {
  width: 48px;
  height: 48px;
  margin-bottom: 16px;
}

.error-icon svg {
  width: 100%;
  height: 100%;
  color: rgba(248, 81, 73, 0.8);
}

.empty-icon svg {
  width: 100%;
  height: 100%;
  color: rgba(201, 209, 217, 0.4);
}

.error-message,
.empty-message {
  font-size: 16px;
  color: rgba(201, 209, 217, 0.8);
  margin-bottom: 16px;
}

.retry-button {
  padding: 8px 16px;
  background-color: rgba(33, 38, 45, 0.8);
  border: 1px solid rgba(56, 189, 248, 0.4);
  border-radius: 6px;
  color: rgba(201, 209, 217, 1);
  cursor: pointer;
  transition: all 0.2s ease;
}

.retry-button:hover {
  background-color: rgba(56, 189, 248, 0.1);
}

/* 自定义滚动条 */
.grid-scrollbar {
  position: absolute;
  top: 4px;
  right: 4px;
  width: 8px;
  height: calc(100% - 8px);
  z-index: 5;
}

.scrollbar-track {
  position: relative;
  width: 100%;
  height: 100%;
  background-color: rgba(33, 38, 45, 0.3);
  border-radius: 4px;
  cursor: pointer;
}

.scrollbar-thumb {
  position: absolute;
  width: 100%;
  background-color: rgba(56, 189, 248, 0.3);
  border-radius: 4px;
  transition: background-color 0.2s ease;
}

.scrollbar-thumb:hover,
.scrollbar-thumb:active {
  background-color: rgba(56, 189, 248, 0.5);
}

/* 动画 */
@keyframes spin {
  0% { transform: rotate(0deg); }
  100% { transform: rotate(360deg); }
}

@keyframes pulse {
  0% { 
    transform: translate(-50%, -50%) scale(0.8);
    opacity: 0.5;
  }
  100% { 
    transform: translate(-50%, -50%) scale(1.2);
    opacity: 0.8;
  }
}

/* 响应式设计 */
@media (max-width: 1024px) {
  .grid-content {
    grid-template-columns: repeat(2, 1fr);
  }
}

@media (max-width: 640px) {
  .grid-content {
    grid-template-columns: 1fr;
  }
}
</style> 