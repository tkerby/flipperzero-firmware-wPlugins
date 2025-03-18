<template>
  <div class="template-grid-container">
    <el-scrollbar ref="scrollbarRef" class="template-scrollbar">
      <div class="template-grid">
        <slot></slot>
      </div>
    </el-scrollbar>
  </div>
</template>

<script setup>
import { ref, nextTick } from 'vue';
import { ElScrollbar } from 'element-plus';

const scrollbarRef = ref(null);

// 更新滚动条方法
const updateScrollbar = () => {
  nextTick(() => {
    if (scrollbarRef.value) {
      scrollbarRef.value.update();
      console.log('滚动条已更新');
    }
  });
};

// 暴露给父组件的方法
defineExpose({
  updateScrollbar,
  forceCheckScrollbar: () => {
    console.log('强制检查滚动条状态');
    // 强制更新滚动条
    nextTick(() => {
      if (scrollbarRef.value) {
        scrollbarRef.value.update();
        console.log('滚动条已强制更新');
      } else {
        console.warn('滚动条引用不存在，无法更新');
      }
    });
  }
});
</script>

<style scoped>
.template-grid-container {
  width: 100%;
  height: 100%;
  overflow: hidden;
}

.template-scrollbar {
  height: 100%;
}

.template-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
  gap: 16px;
  padding: 16px;
}

/* 自定义滚动条样式 */
:deep(.el-scrollbar__bar) {
  opacity: 0.3;
  transition: opacity 0.3s;
}

:deep(.el-scrollbar__bar:hover) {
  opacity: 0.8;
}
</style> 