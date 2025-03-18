<template>
  <div 
    class="nfc-card-grid"
    :class="{ 'grid-loading': loading, 'is-scrolling': isScrolling }"
    ref="gridContainer"
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
    
    <div v-else ref="gridContent" class="grid-content" :style="gridStyle">
      <slot></slot>
    </div>
    
    <!-- 滚动条 -->
    <div 
      class="grid-scrollbar" 
      v-if="showScrollbar"
      :class="{ 'visible': needsScrollbar }"
    >
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
    
    <!-- 调试信息 -->
    <div v-if="false" class="debug-info">
      <div>showScrollbar: {{ showScrollbar }}</div>
      <div>needsScrollbar: {{ needsScrollbar }}</div>
      <div>loading: {{ loading }}</div>
      <div>error: {{ error }}</div>
      <div>isEmpty: {{ isEmpty }}</div>
      <div>maxHeight: {{ maxHeight }}</div>
      <div v-if="gridContent">scrollHeight: {{ gridContent.scrollHeight }}</div>
      <div v-if="gridContent">clientHeight: {{ gridContent.clientHeight }}</div>
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
const gridContainer = ref(null);
const gridContent = ref(null);
const scrollThumb = ref(null);
const thumbPosition = ref(0);
const thumbSize = ref(20);
const isDragging = ref(false);
const startY = ref(0);
const startThumbTop = ref(0);
const isCheckingScrollbar = ref(false); // 新增：标记是否正在检查滚动条
const isScrolling = ref(false); // 新增：标记是否正在滚动
let scrollTimer = null; // 新增：滚动计时器

// 是否显示滚动条
const needsScrollbar = ref(false);
const showScrollbar = computed(() => {
  return needsScrollbar.value && props.maxHeight !== null && !props.loading && !props.error && !props.isEmpty;
});

// 更新滚动条位置和大小
const updateScrollbar = () => {
  if (!gridContent.value || isCheckingScrollbar.value) return;
  
  isCheckingScrollbar.value = true;
  
  const { scrollHeight, clientHeight, scrollTop } = gridContent.value;
  
  // 只有当内容高度超过容器高度时才显示滚动条
  const shouldShowScrollbar = scrollHeight > clientHeight;
  
  // 只有当状态需要变化时才更新，避免不必要的重渲染
  if (needsScrollbar.value !== shouldShowScrollbar) {
    needsScrollbar.value = shouldShowScrollbar;
  }
  
  // 如果需要显示滚动条，更新滑块大小和位置
  if (shouldShowScrollbar) {
    // 计算滑块大小（百分比）
    const sizeRatio = clientHeight / scrollHeight;
    thumbSize.value = Math.max(10, sizeRatio * 100);
    
    // 计算滑块位置（百分比）
    if (scrollHeight <= clientHeight) {
      thumbPosition.value = 0;
    } else {
      const scrollRatio = scrollTop / (scrollHeight - clientHeight);
      thumbPosition.value = scrollRatio * (100 - thumbSize.value);
    }
  }
  
  isCheckingScrollbar.value = false;
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
    // 设置正在滚动标志
    isScrolling.value = true;
    
    // 清除之前的计时器
    if (scrollTimer) {
      clearTimeout(scrollTimer);
    }
    
    // 设置新的计时器，滚动停止1.5秒后隐藏滚动条
    scrollTimer = setTimeout(() => {
      isScrolling.value = false;
    }, 1500);
    
    requestAnimationFrame(updateScrollbar);
  }
};

// 强制检查滚动条 - 优化版本
const forceCheckScrollbar = () => {
  if (!gridContent.value || isCheckingScrollbar.value) return;
  
  // 设置标志，防止重复检查
  isCheckingScrollbar.value = true;
  
  // 获取内容尺寸
  const { scrollHeight, clientHeight } = gridContent.value;
  
  // 如果内容高度为0，可能是内容还未渲染完成
  if (scrollHeight === 0 || clientHeight === 0) {
    isCheckingScrollbar.value = false;
    // 只设置一个延迟检查，避免多次检查
    setTimeout(forceCheckScrollbar, 300);
    return;
  }
  
  // 确定是否需要滚动条
  const shouldShowScrollbar = scrollHeight > clientHeight;
  
  // 只有当状态需要变化时才更新，避免不必要的重渲染
  if (needsScrollbar.value !== shouldShowScrollbar) {
    needsScrollbar.value = shouldShowScrollbar;
  }
  
  // 如果需要滚动条，更新滑块大小和位置
  if (shouldShowScrollbar) {
    // 计算滑块大小（百分比）
    const sizeRatio = clientHeight / scrollHeight;
    thumbSize.value = Math.max(10, sizeRatio * 100);
    
    // 计算滑块位置（百分比）
    const scrollTop = gridContent.value.scrollTop;
    if (scrollHeight <= clientHeight) {
      thumbPosition.value = 0;
    } else {
      const scrollRatio = scrollTop / (scrollHeight - clientHeight);
      thumbPosition.value = scrollRatio * (100 - thumbSize.value);
    }
  }
  
  isCheckingScrollbar.value = false;
};

// 组件挂载时
onMounted(() => {
  // 初始化后检查一次滚动条，不再使用多次检查
  setTimeout(() => {
    if (gridContent.value) {
      // 添加滚动事件监听
      gridContent.value.addEventListener('scroll', onScroll);
      // 检查滚动条
      forceCheckScrollbar();
      
      // 确保滚动条尺寸正确，再次检查一次
      setTimeout(() => {
        if (!isCheckingScrollbar.value) {
          forceCheckScrollbar();
        }
      }, 500);
    }
  }, 300);
  
  // 监听窗口大小变化
  const handleResize = () => {
    setTimeout(forceCheckScrollbar, 200);
  };
  
  window.addEventListener('resize', handleResize);
  
  // 组件卸载时
  onUnmounted(() => {
    if (gridContent.value) {
      gridContent.value.removeEventListener('scroll', onScroll);
    }
    
    // 断开内容观察器
    if (contentObserver.value) {
      contentObserver.value.disconnect();
    }
    
    window.removeEventListener('resize', handleResize);
    document.removeEventListener('mousemove', onDrag);
    document.removeEventListener('mouseup', stopDrag);
    
    // 清除滚动计时器
    if (scrollTimer) {
      clearTimeout(scrollTimer);
    }
  });
});

// 监听内容变化
const contentObserver = ref(null);

// 监听加载状态变化
watch(() => props.loading, (newVal, oldVal) => {
  if (oldVal && !newVal) {
    // 从加载状态变为非加载状态时，延迟检查滚动条，但只检查一次
    setTimeout(forceCheckScrollbar, 300);
    
    // 设置内容观察器，监听内容变化
    setTimeout(() => {
      if (gridContent.value && !contentObserver.value) {
        contentObserver.value = new MutationObserver(() => {
          // 内容变化时检查滚动条，但使用节流避免频繁更新
          if (!isCheckingScrollbar.value) {
            forceCheckScrollbar();
          }
        });
        
        contentObserver.value.observe(gridContent.value, {
          childList: true,
          subtree: true,
          attributes: true
        });
        
        // 确保初始状态下滚动条尺寸正确
        setTimeout(forceCheckScrollbar, 100);
      }
    }, 500);
  }
});

// 插槽内容变化方法
const slotContentChanged = () => {
  setTimeout(forceCheckScrollbar, 300);
};

// 暴露方法给父组件
defineExpose({
  updateScrollbar,
  slotContentChanged,
  forceCheckScrollbar
});

// 监听maxHeight变化
watch(() => props.maxHeight, () => {
  setTimeout(forceCheckScrollbar, 100);
  // 确保滚动条尺寸正确，再次检查一次
  setTimeout(forceCheckScrollbar, 500);
});

// 监听columns变化
watch(() => props.columns, () => {
  setTimeout(forceCheckScrollbar, 100);
  // 确保滚动条尺寸正确，再次检查一次
  setTimeout(forceCheckScrollbar, 500);
});
</script>

<style scoped>
.nfc-card-grid {
  position: relative;
  width: 100%;
  height: 100%;
  overflow: hidden; /* 确保内容不会溢出 */
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
  margin-right: 0; /* 移除右侧边距 */
  padding-right: 30px; /* 减少右侧内边距 */
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
  right: 0;
  width: 6px; /* 稍微减小宽度 */
  height: calc(100% - 8px);
  z-index: 5;
  opacity: 0;
  transition: opacity 0.3s ease;
}

.grid-scrollbar.visible {
  opacity: 0;
}

.grid-content:hover + .grid-scrollbar,
.grid-scrollbar:hover,
.is-scrolling .grid-scrollbar {
  opacity: 0.7 !important;
}

.scrollbar-track {
  position: relative;
  width: 100%;
  height: 100%;
  background-color: rgba(33, 38, 45, 0.2);
  border-radius: 3px 0 0 3px; /* 只在左侧添加圆角 */
  cursor: pointer;
}

.scrollbar-thumb {
  position: absolute;
  width: 100%;
  background-color: rgba(56, 189, 248, 0.3);
  border-radius: 4px;
  transition: background-color 0.2s ease;
  min-height: 30px; /* 确保滑块有最小高度 */
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

/* 调试信息 */
.debug-info {
  position: absolute;
  bottom: 4px;
  left: 4px;
  background-color: rgba(0, 0, 0, 0.7);
  color: white;
  padding: 8px;
  border-radius: 4px;
  font-size: 12px;
  z-index: 100;
}
</style> 