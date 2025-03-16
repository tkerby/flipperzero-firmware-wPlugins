<template>
  <div class="loading-container">
    <!-- 主标题动画 -->
    <div class="title-container">
      <h1 class="main-title">
        <!-- NFC -->
        <span v-for="(char, index) in 'NFC'" 
              :key="'nfc-'+index"
              class="char"
              :style="{ 
                animationDelay: `${index * 0.1}s`,
                '--glow-color': '#38BDF8'
              }">
          {{ char }}
        </span>
        
        <!-- 间隔 -->
        <span class="word-space"></span>
        
        <!-- ANALYSIS -->
        <span v-for="(char, index) in 'ANALYSIS'" 
              :key="'analysis-'+index"
              class="char"
              :style="{ 
                animationDelay: `${(index + 4) * 0.1}s`,
                '--glow-color': '#38BDF8'
              }">
          {{ char }}
        </span>
        
        <!-- 间隔 -->
        <span class="word-space"></span>
        
        <!-- PLATFORM -->
        <span v-for="(char, index) in 'PLATFORM'" 
              :key="'platform-'+index"
              class="char"
              :style="{ 
                animationDelay: `${(index + 12) * 0.1}s`,
                '--glow-color': '#38BDF8'
              }">
          {{ char }}
        </span>
      </h1>
      
      <!-- 扫描线效果 -->
      <div class="scan-line"></div>
    </div>

    <!-- 中央动画区域 -->
    <div class="central-animation">
      <!-- 外圈旋转效果 -->
      <div class="rotating-circle"></div>
      
      <!-- NFC 信号波动 -->
      <div class="signal-waves">
        <div v-for="i in 3" :key="i" 
             class="wave"
             :style="{ animationDelay: `${(i-1) * 0.3}s` }">
        </div>
      </div>
      
      <!-- 数据流效果 -->
      <div class="data-stream">
        <div v-for="i in 12" :key="i" 
             class="data-particle"
             :style="{ 
               '--angle': `${(i * 30)}deg`,
               '--delay': `${i * 0.2}s`
             }">
          {{ Math.random() > 0.5 ? '1' : '0' }}
        </div>
      </div>
    </div>

    <!-- 加载状态文本 -->
    <div class="loading-status">
      <div class="status-text">
        <span class="text">LOADING</span>
        <span class="dots">
          <span v-for="i in 3" :key="i" 
                class="dot"
                :style="{ animationDelay: `${(i-1) * 0.2}s` }">.</span>
        </span>
      </div>
      <div class="progress-bar">
        <div class="progress" :style="{ width: `${progress}%` }"></div>
      </div>
      <div class="status-message">{{ currentMessage }}</div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue';

const emit = defineEmits(['complete']);
const progress = ref(0);
const currentMessage = ref('Initializing system');

const messages = [
  'Initializing system',
  'Loading NFC modules',
  'Calibrating signal processor',
  'Starting analysis engine',
  'System ready'
];

let messageIndex = 0;

onMounted(() => {
  // 进度条动画
  const progressInterval = setInterval(() => {
    if (progress.value < 100) {
      progress.value += 1;
      
      // 根据进度更新消息
      if (progress.value % 20 === 0) {
        messageIndex = Math.min(messageIndex + 1, messages.length - 1);
        currentMessage.value = messages[messageIndex];
      }
    } else {
      clearInterval(progressInterval);
      // 当进度达到 100% 时，等待一小段时间后触发完成事件
      setTimeout(() => {
        emit('complete');
      }, 1000);
    }
  }, 50);
});
</script>

<style scoped>
.loading-container {
  @apply fixed inset-0 flex flex-col items-center justify-center bg-ark-bg z-50;
  @apply bg-opacity-95 backdrop-blur-sm;
  perspective: 1000px;
}

/* 主标题样式 */
.title-container {
  @apply relative mb-12;
}

.main-title {
  @apply text-4xl md:text-5xl font-mono tracking-wider text-ark-text;
  word-spacing: 50em;  /* 增加单词间距 */
}

.char {
  @apply inline-block relative;
  color: transparent;
  text-shadow: 0 0 10px var(--glow-color);
  animation: char-appear 0.5s forwards, char-glow 2s infinite;
}

.scan-line {
  @apply absolute w-full h-1 bg-ark-accent;
  @apply opacity-50;
  top: 50%;
  transform: translateY(-50%);
  animation: scan-line 2s linear infinite;
  box-shadow: 0 0 15px theme('colors.ark.accent');
}

/* 中央动画区域 */
.central-animation {
  @apply relative w-48 h-48 mb-12;
}

.rotating-circle {
  @apply absolute inset-0 border-2 border-ark-accent rounded-full;
  animation: rotate 4s linear infinite;
}

.rotating-circle::before {
  content: '';
  @apply absolute w-4 h-4 bg-ark-accent rounded-full;
  top: -8px;
  left: calc(50% - 8px);
  box-shadow: 0 0 15px theme('colors.ark.accent');
}

.signal-waves {
  @apply absolute inset-0 flex items-center justify-center;
}

.wave {
  @apply absolute w-full h-full rounded-full border-2 border-ark-accent opacity-0;
  animation: wave-expand 2s cubic-bezier(0.215, 0.61, 0.355, 1) infinite;
}

.data-stream {
  @apply absolute inset-0;
}

.data-particle {
  @apply absolute font-mono text-xs text-ark-accent;
  top: 50%;
  left: 50%;
  animation: particle-move 2s linear infinite;
  animation-delay: var(--delay);
  transform-origin: 0 0;
}

/* 加载状态区域 */
.loading-status {
  @apply flex flex-col items-center;
}

.status-text {
  @apply flex items-center font-mono text-ark-text mb-4;
}

.text {
  @apply mr-1;
}

.dot {
  animation: dot-blink 1.4s infinite;
  opacity: 0;
}

.progress-bar {
  @apply w-64 h-1 bg-ark-panel rounded-full overflow-hidden mb-2;
}

.progress {
  @apply h-full bg-ark-accent;
  box-shadow: 0 0 10px theme('colors.ark.accent');
  transition: width 0.3s ease-out;
}

.status-message {
  @apply text-sm text-ark-text opacity-70 font-mono;
}

/* 动画关键帧 */
@keyframes char-appear {
  from {
    opacity: 0;
    transform: translateY(-20px) rotateX(90deg);
  }
  to {
    opacity: 1;
    transform: translateY(0) rotateX(0);
  }
}

@keyframes char-glow {
  0%, 100% {
    text-shadow: 0 0 10px var(--glow-color);
  }
  50% {
    text-shadow: 0 0 20px var(--glow-color), 0 0 30px var(--glow-color);
  }
}

@keyframes scan-line {
  0% {
    transform: translateY(-50%) scaleX(0);
    opacity: 0;
  }
  50% {
    transform: translateY(-50%) scaleX(1);
    opacity: 1;
  }
  100% {
    transform: translateY(-50%) scaleX(0);
    opacity: 0;
  }
}

@keyframes rotate {
  from {
    transform: rotate(0deg);
  }
  to {
    transform: rotate(360deg);
  }
}

@keyframes wave-expand {
  0% {
    transform: scale(0.3);
    opacity: 0.8;
  }
  100% {
    transform: scale(1);
    opacity: 0;
  }
}

@keyframes particle-move {
  0% {
    transform: rotate(var(--angle)) translateX(0) scale(1);
    opacity: 0;
  }
  20% {
    opacity: 1;
  }
  100% {
    transform: rotate(var(--angle)) translateX(100px) scale(0);
    opacity: 0;
  }
}

@keyframes dot-blink {
  0%, 20% {
    opacity: 0;
  }
  40% {
    opacity: 1;
  }
  60% {
    opacity: 1;
  }
  80%, 100% {
    opacity: 0;
  }
}

.word-space {
  display: inline-block;
  width: 0.5em;  /* 可以调整这个值来控制单词间的间距 */
}
</style> 