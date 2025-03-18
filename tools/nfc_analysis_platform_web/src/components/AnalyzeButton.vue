<template>
  <button 
    class="analyze-button" 
    :class="{ 
      'disabled': disabled, 
      'analyzing': analyzing,
      'enabled': !disabled && !analyzing
    }"
    @click="handleClick"
    :disabled="disabled || analyzing"
  >
    <div class="button-content">
      <div class="chip-icon">
        <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
          <rect x="4" y="4" width="16" height="16" rx="2" ry="2"></rect>
          <rect x="9" y="9" width="6" height="6"></rect>
          <line x1="9" y1="2" x2="9" y2="4"></line>
          <line x1="15" y1="2" x2="15" y2="4"></line>
          <line x1="9" y1="20" x2="9" y2="22"></line>
          <line x1="15" y1="20" x2="15" y2="22"></line>
          <line x1="20" y1="9" x2="22" y2="9"></line>
          <line x1="20" y1="14" x2="22" y2="14"></line>
          <line x1="2" y1="9" x2="4" y2="9"></line>
          <line x1="2" y1="14" x2="4" y2="14"></line>
        </svg>
      </div>
      <span class="button-text">{{ buttonText }}</span>
      <div class="analyze-indicator" v-if="analyzing">
        <div class="indicator-dot"></div>
        <div class="indicator-dot"></div>
        <div class="indicator-dot"></div>
      </div>
    </div>
    <div class="button-glow"></div>
    <div class="button-circuit-lines">
      <div class="circuit-line line-1"></div>
      <div class="circuit-line line-2"></div>
      <div class="circuit-line line-3"></div>
      <div class="circuit-line line-4"></div>
    </div>
  </button>
</template>

<script setup>
import { ref, computed, watch } from 'vue';
import { useI18n } from 'vue-i18n';

const { t } = useI18n();

const props = defineProps({
  disabled: {
    type: Boolean,
    default: true
  }
});

const emit = defineEmits(['analyze', 'analyzing-complete']);

const analyzing = ref(false);
const analyzeTimeout = ref(null);

// 按钮文本根据状态变化
const buttonText = computed(() => {
  if (analyzing.value) {
    return t('nard.buttons.analyzing');
  } else if (props.disabled) {
    return t('nard.buttons.analyze');
  } else {
    return t('nard.buttons.analyze');
  }
});

// 处理按钮点击
const handleClick = () => {
  if (props.disabled || analyzing.value) return;
  
  analyzing.value = true;
  emit('analyze');
};

// 设置解析状态
const setAnalyzing = (status) => {
  analyzing.value = status;
  
  // 如果设置为false，清除定时器
  if (!status && analyzeTimeout.value) {
    clearTimeout(analyzeTimeout.value);
    analyzeTimeout.value = null;
  }
};

// 当组件销毁时清除定时器
watch(() => props.disabled, (newVal) => {
  if (newVal && analyzing.value) {
    clearTimeout(analyzeTimeout.value);
    analyzing.value = false;
  }
});

// 暴露方法给父组件
defineExpose({
  setAnalyzing
});
</script>

<style scoped>
.analyze-button {
  position: relative;
  width: 100%;
  height: 56px;
  border: none;
  border-radius: 8px;
  background-color: rgba(28, 33, 40, 0.7);
  color: rgba(201, 209, 217, 0.7);
  font-size: 16px;
  font-weight: 500;
  cursor: pointer;
  overflow: hidden;
  transition: all 0.3s ease;
  padding: 0;
  outline: none;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.2);
}

.button-content {
  position: relative;
  z-index: 2;
  display: flex;
  align-items: center;
  width: 100%;
  height: 100%;
  padding: 0 16px;
}

.chip-icon {
  position: absolute;
  left: 16px;
  display: flex;
  align-items: center;
  justify-content: center;
  width: 32px;
  height: 32px;
  border-radius: 6px;
  background-color: rgba(22, 27, 34, 0.7);
  transition: all 0.3s ease;
}

.chip-icon svg {
  width: 20px;
  height: 20px;
  color: rgba(56, 189, 248, 0.8);
  transition: all 0.3s ease;
}

.button-text {
  width: 100%;
  text-align: center;
  transition: all 0.3s ease;
}

/* 禁用状态 */
.analyze-button.disabled {
  background-color: rgba(28, 33, 40, 0.5);
  color: rgba(201, 209, 217, 0.4);
  cursor: not-allowed;
  box-shadow: none;
}

.analyze-button.disabled .chip-icon {
  background-color: rgba(22, 27, 34, 0.5);
}

.analyze-button.disabled .chip-icon svg {
  color: rgba(201, 209, 217, 0.4);
}

/* 可用状态 */
.analyze-button.enabled {
  background-color: rgba(28, 33, 40, 0.8);
  color: rgba(201, 209, 217, 1);
  border: 1px solid rgba(56, 189, 248, 0.3);
  box-shadow: 0 4px 16px rgba(56, 189, 248, 0.1);
}

.analyze-button.enabled:hover {
  background-color: rgba(33, 38, 45, 0.9);
  border-color: rgba(56, 189, 248, 0.5);
  box-shadow: 0 4px 20px rgba(56, 189, 248, 0.2);
}

.analyze-button.enabled .chip-icon {
  background-color: rgba(56, 189, 248, 0.1);
}

.analyze-button.enabled .chip-icon svg {
  color: rgba(56, 189, 248, 1);
}

.analyze-button.enabled .button-glow {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background: radial-gradient(circle at center, rgba(56, 189, 248, 0.1) 0%, transparent 70%);
  opacity: 0;
  transition: opacity 0.5s ease;
  z-index: 1;
}

.analyze-button.enabled:hover .button-glow {
  opacity: 1;
}

/* 解析中状态 */
.analyze-button.analyzing {
  background-color: rgba(28, 33, 40, 0.8);
  color: rgba(201, 209, 217, 1);
  border: 1px solid rgba(52, 211, 153, 0.5);
  box-shadow: 0 4px 16px rgba(52, 211, 153, 0.2);
}

.analyze-button.analyzing .chip-icon {
  background-color: rgba(52, 211, 153, 0.1);
}

.analyze-button.analyzing .chip-icon svg {
  color: rgba(52, 211, 153, 1);
  animation: pulse 1.5s infinite;
}

.analyze-button.analyzing .button-glow {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background: radial-gradient(circle at center, rgba(52, 211, 153, 0.15) 0%, transparent 70%);
  opacity: 1;
  z-index: 1;
}

/* 电路线动画 */
.button-circuit-lines {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  z-index: 1;
  opacity: 0;
  transition: opacity 0.3s ease;
  overflow: hidden;
  border-radius: 8px;
}

.analyze-button.enabled:hover .button-circuit-lines,
.analyze-button.analyzing .button-circuit-lines {
  opacity: 1;
}

.circuit-line {
  position: absolute;
}

/* 主要扫描线 */
.line-1 {
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background: repeating-linear-gradient(
    0deg,
    transparent,
    transparent 4px,
    rgba(56, 189, 248, 0.03) 4px,
    rgba(56, 189, 248, 0.03) 5px
  );
  animation: hologram-scan 8s infinite linear;
}

/* 水平扫描光束 */
.line-2 {
  top: 0;
  left: -100%;
  width: 100%;
  height: 100%;
  background: linear-gradient(90deg, 
    transparent 0%, 
    rgba(56, 189, 248, 0.01) 20%, 
    rgba(56, 189, 248, 0.1) 50%,
    rgba(56, 189, 248, 0.01) 80%,
    transparent 100%
  );
  animation: hologram-sweep 3s infinite ease-in-out;
}

/* 脉冲效果 */
.line-3 {
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background: radial-gradient(circle at center, rgba(56, 189, 248, 0.08) 0%, transparent 70%);
  animation: hologram-pulse 4s infinite ease-in-out;
}

/* 网格背景 */
.line-4 {
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background-image: 
    linear-gradient(0deg, transparent 49.5%, rgba(56, 189, 248, 0.05) 49.5%, rgba(56, 189, 248, 0.05) 50.5%, transparent 50.5%),
    linear-gradient(90deg, transparent 49.5%, rgba(56, 189, 248, 0.05) 49.5%, rgba(56, 189, 248, 0.05) 50.5%, transparent 50.5%);
  background-size: 15px 15px;
  animation: hologram-grid 15s infinite linear;
}

/* 添加更多全息元素 */
.analyze-button.enabled:hover .button-circuit-lines::before,
.analyze-button.analyzing .button-circuit-lines::before {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background-image: 
    /* 数据流线 */
    linear-gradient(90deg, transparent 30%, rgba(56, 189, 248, 0.1) 50%, transparent 70%),
    linear-gradient(90deg, transparent 40%, rgba(56, 189, 248, 0.07) 50%, transparent 60%),
    linear-gradient(90deg, transparent 45%, rgba(56, 189, 248, 0.05) 50%, transparent 55%),
    /* 垂直数据流线 */
    linear-gradient(0deg, transparent 30%, rgba(56, 189, 248, 0.1) 50%, transparent 70%),
    linear-gradient(0deg, transparent 40%, rgba(56, 189, 248, 0.07) 50%, transparent 60%),
    /* 对角数据流线 */
    linear-gradient(45deg, transparent 49.8%, rgba(56, 189, 248, 0.1) 49.9%, rgba(56, 189, 248, 0.1) 50.1%, transparent 50.2%),
    linear-gradient(-45deg, transparent 49.8%, rgba(56, 189, 248, 0.1) 49.9%, rgba(56, 189, 248, 0.1) 50.1%, transparent 50.2%);
  
  background-size: 
    100% 100%,
    100% 100%,
    100% 100%,
    100% 100%,
    100% 100%,
    100px 100px,
    100px 100px;
  
  opacity: 0.7;
  animation: hologram-data-flow 10s infinite linear;
}

/* 添加数据点和处理节点 */
.analyze-button.enabled:hover .button-circuit-lines::after,
.analyze-button.analyzing .button-circuit-lines::after {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background-image: 
    /* 数据处理节点 */
    radial-gradient(circle at 20% 30%, rgba(56, 189, 248, 0.2) 0, rgba(56, 189, 248, 0.2) 1px, transparent 3px),
    radial-gradient(circle at 40% 40%, rgba(56, 189, 248, 0.2) 0, rgba(56, 189, 248, 0.2) 1px, transparent 3px),
    radial-gradient(circle at 60% 60%, rgba(56, 189, 248, 0.2) 0, rgba(56, 189, 248, 0.2) 1px, transparent 3px),
    radial-gradient(circle at 80% 20%, rgba(56, 189, 248, 0.2) 0, rgba(56, 189, 248, 0.2) 1px, transparent 3px),
    radial-gradient(circle at 15% 70%, rgba(56, 189, 248, 0.2) 0, rgba(56, 189, 248, 0.2) 1px, transparent 3px),
    radial-gradient(circle at 75% 75%, rgba(56, 189, 248, 0.2) 0, rgba(56, 189, 248, 0.2) 1px, transparent 3px),
    /* 主要处理节点 */
    radial-gradient(circle at 30% 50%, rgba(56, 189, 248, 0) 0, rgba(56, 189, 248, 0.3) 5px, transparent 10px),
    radial-gradient(circle at 70% 50%, rgba(56, 189, 248, 0) 0, rgba(56, 189, 248, 0.3) 5px, transparent 10px);
  
  background-repeat: no-repeat;
  animation: hologram-nodes-pulse 4s infinite alternate;
}

/* 添加移动的数据点 */
.analyze-button.enabled:hover .circuit-line::after,
.analyze-button.analyzing .circuit-line::after {
  content: '';
  position: absolute;
  width: 2px;
  height: 2px;
  border-radius: 50%;
  background-color: rgba(56, 189, 248, 1);
  box-shadow: 0 0 4px rgba(56, 189, 248, 0.8);
  filter: blur(0.5px);
}

.analyze-button.enabled:hover .line-1::after,
.analyze-button.analyzing .line-1::after {
  top: 25%;
  animation: hologram-particle-horizontal 2s infinite linear;
}

.analyze-button.enabled:hover .line-2::after,
.analyze-button.analyzing .line-2::after {
  top: 75%;
  animation: hologram-particle-horizontal 3s infinite linear reverse;
}

.analyze-button.enabled:hover .line-3::after,
.analyze-button.analyzing .line-3::after {
  left: 25%;
  top: 0;
  animation: hologram-particle-vertical 2.5s infinite linear;
}

.analyze-button.enabled:hover .line-4::after,
.analyze-button.analyzing .line-4::after {
  left: 75%;
  top: 0;
  animation: hologram-particle-vertical 3.5s infinite linear reverse;
}

/* 全息动画关键帧 */
@keyframes hologram-scan {
  0% { background-position: 0 0; }
  100% { background-position: 0 100px; }
}

@keyframes hologram-sweep {
  0% { transform: translateX(0%); }
  100% { transform: translateX(200%); }
}

@keyframes hologram-pulse {
  0% { opacity: 0.3; transform: scale(0.95); }
  50% { opacity: 0.7; transform: scale(1.05); }
  100% { opacity: 0.3; transform: scale(0.95); }
}

@keyframes hologram-grid {
  0% { background-position: 0 0; }
  100% { background-position: 30px 30px; }
}

@keyframes hologram-data-flow {
  0% { background-position: 0% 0%, 0% 0%, 0% 0%, 0% 0%, 0% 0%, 0px 0px, 0px 0px; }
  50% { background-position: 100% 0%, -100% 0%, 100% 0%, 0% 100%, 0% -100%, 50px 50px, -50px 50px; }
  100% { background-position: 0% 0%, 0% 0%, 0% 0%, 0% 0%, 0% 0%, 100px 100px, -100px 100px; }
}

@keyframes hologram-nodes-pulse {
  0% { opacity: 0.5; transform: scale(0.98); }
  100% { opacity: 1; transform: scale(1.02); }
}

@keyframes hologram-particle-horizontal {
  0% { left: 0; }
  100% { left: 100%; }
}

@keyframes hologram-particle-vertical {
  0% { top: 0; }
  100% { top: 100%; }
}

/* 解析指示器 */
.analyze-indicator {
  position: absolute;
  right: 16px;
  display: flex;
  align-items: center;
}

.indicator-dot {
  width: 6px;
  height: 6px;
  border-radius: 50%;
  background-color: rgba(52, 211, 153, 1);
  margin: 0 2px;
  opacity: 0.6;
}

.indicator-dot:nth-child(1) {
  animation: dot-pulse 1.5s infinite 0s;
}

.indicator-dot:nth-child(2) {
  animation: dot-pulse 1.5s infinite 0.5s;
}

.indicator-dot:nth-child(3) {
  animation: dot-pulse 1.5s infinite 1s;
}

@keyframes pulse {
  0% {
    transform: scale(1);
    opacity: 1;
  }
  50% {
    transform: scale(1.1);
    opacity: 0.8;
  }
  100% {
    transform: scale(1);
    opacity: 1;
  }
}

@keyframes circuit-glow {
  0% {
    opacity: 0.1;
  }
  50% {
    opacity: 0.3;
  }
  100% {
    opacity: 0.1;
  }
}

@keyframes dot-pulse {
  0% {
    transform: scale(1);
    opacity: 0.6;
  }
  50% {
    transform: scale(1.2);
    opacity: 1;
  }
  100% {
    transform: scale(1);
    opacity: 0.6;
  }
}
</style> 