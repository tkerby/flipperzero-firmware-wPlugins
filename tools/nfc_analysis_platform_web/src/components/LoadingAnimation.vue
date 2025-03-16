<template>
  <div class="loading-animation-container" :class="{ 'fade-out': isComplete }">
    <div class="loading-content">
      <div class="logo-animation">
        <div class="nfc-icon">
          <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" class="nfc-svg">
            <path d="M20,2H4A2,2 0 0,0 2,4V20A2,2 0 0,0 4,22H20A2,2 0 0,0 22,20V4A2,2 0 0,0 20,2M10,17.5V15H8V17.5H6.5V9H8V13.5H10V9H11.5V17.5H10M14.5,17.5H13V9H14.5V17.5M18,17.5H16.5V9H18V17.5Z" />
          </svg>
        </div>
        <div class="signal-waves">
          <div class="wave wave-1"></div>
          <div class="wave wave-2"></div>
          <div class="wave wave-3"></div>
        </div>
      </div>
      
      <div class="title-animation">
        <h1 class="cyber-title">
          <span v-for="(char, index) in titleChars" :key="index" 
                :style="{ animationDelay: `${index * 0.1}s` }"
                class="char">
            {{ char }}
          </span>
        </h1>
      </div>
      
      <div class="loading-bar-container">
        <div class="loading-bar" :style="{ width: `${loadingProgress}%` }"></div>
        <div class="loading-percentage">{{ Math.floor(loadingProgress) }}%</div>
      </div>
      
      <div class="loading-text">
        <span class="terminal-prefix">&gt;</span>
        <span class="typewriter-text">{{ currentText }}</span>
        <span class="cursor" :class="{ 'blink': cursorBlink }">_</span>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, onBeforeUnmount } from 'vue';

const props = defineProps({
  duration: {
    type: Number,
    default: 3000
  },
  autoComplete: {
    type: Boolean,
    default: true
  }
});

const emit = defineEmits(['complete']);

const loadingProgress = ref(0);
const isComplete = ref(false);
const currentTextIndex = ref(0);
const currentCharIndex = ref(0);
const cursorBlink = ref(false);

const loadingTexts = [
  '初始化系统...',
  '加载NFC分析模块...',
  '连接Flipper设备...',
  '准备TLV解析器...',
  '系统就绪'
];

const titleText = 'NFC Analysis Platform';
const titleChars = computed(() => titleText.split(''));

const currentText = ref('');
const typingInterval = ref(null);
const progressInterval = ref(null);
const cursorInterval = ref(null);

// 打字机效果
const typeNextChar = () => {
  if (currentTextIndex.value >= loadingTexts.length) {
    return;
  }
  
  const text = loadingTexts[currentTextIndex.value];
  
  if (currentCharIndex.value < text.length) {
    currentText.value = text.substring(0, currentCharIndex.value + 1);
    currentCharIndex.value++;
  } else {
    clearInterval(typingInterval.value);
    setTimeout(() => {
      currentText.value = '';
      currentCharIndex.value = 0;
      currentTextIndex.value++;
      
      if (currentTextIndex.value < loadingTexts.length) {
        typingInterval.value = setInterval(typeNextChar, 50);
      }
    }, 1000);
  }
};

// 进度条动画
const updateProgress = () => {
  if (loadingProgress.value < 100) {
    // 非线性进度，开始快，结束慢
    const increment = (100 - loadingProgress.value) / 50;
    loadingProgress.value = Math.min(100, loadingProgress.value + increment);
  } else {
    clearInterval(progressInterval.value);
    
    if (props.autoComplete) {
      setTimeout(completeAnimation, 500);
    }
  }
};

// 完成动画
const completeAnimation = () => {
  isComplete.value = true;
  setTimeout(() => {
    emit('complete');
  }, 1000);
};

onMounted(() => {
  // 启动打字机效果
  typingInterval.value = setInterval(typeNextChar, 50);
  
  // 启动进度条动画
  progressInterval.value = setInterval(updateProgress, 50);
  
  // 启动光标闪烁
  cursorInterval.value = setInterval(() => {
    cursorBlink.value = !cursorBlink.value;
  }, 500);
  
  // 如果设置了自动完成，则在指定时间后完成
  if (props.autoComplete) {
    setTimeout(() => {
      loadingProgress.value = 100;
    }, props.duration * 0.8);
  }
});

onBeforeUnmount(() => {
  clearInterval(typingInterval.value);
  clearInterval(progressInterval.value);
  clearInterval(cursorInterval.value);
});
</script>

<style scoped>
.loading-animation-container {
  @apply fixed inset-0 flex items-center justify-center bg-hacker-dark z-50;
  transition: opacity 1s ease-out;
}

.loading-animation-container.fade-out {
  opacity: 0;
}

.loading-content {
  @apply flex flex-col items-center justify-center space-y-8 w-full max-w-2xl px-4;
}

.logo-animation {
  @apply relative flex items-center justify-center mb-8;
}

.nfc-icon {
  @apply relative z-10 w-24 h-24 flex items-center justify-center;
  animation: pulse 2s infinite;
}

.nfc-svg {
  @apply w-full h-full;
  fill: theme('colors.hacker.accent');
  filter: drop-shadow(0 0 8px theme('colors.hacker.accent'));
}

.signal-waves {
  @apply absolute inset-0 flex items-center justify-center;
}

.wave {
  @apply absolute rounded-full border border-hacker-accent opacity-0;
  animation: wave-animation 3s infinite;
}

.wave-1 {
  @apply w-16 h-16;
  animation-delay: 0s;
}

.wave-2 {
  @apply w-20 h-20;
  animation-delay: 0.5s;
}

.wave-3 {
  @apply w-24 h-24;
  animation-delay: 1s;
}

.cyber-title {
  @apply text-4xl md:text-5xl font-display text-center mb-8;
}

.char {
  @apply inline-block text-hacker-accent;
  animation: glow-text 2s ease-in-out infinite alternate;
  opacity: 0;
  animation: char-appear 0.5s forwards;
}

.loading-bar-container {
  @apply relative w-full h-2 bg-hacker-primary rounded-full overflow-hidden mt-8;
}

.loading-bar {
  @apply h-full bg-hacker-accent;
  box-shadow: 0 0 10px theme('colors.hacker.accent');
  transition: width 0.2s ease-out;
}

.loading-percentage {
  @apply absolute top-3 right-0 text-xs text-hacker-accent font-mono;
}

.loading-text {
  @apply mt-6 font-mono text-hacker-light flex items-center justify-center h-6;
}

.terminal-prefix {
  @apply text-hacker-accent mr-2;
}

.typewriter-text {
  @apply text-hacker-light;
}

.cursor {
  @apply text-hacker-accent ml-1;
}

.cursor.blink {
  opacity: 0;
}

@keyframes pulse {
  0% {
    transform: scale(0.95);
    opacity: 0.8;
  }
  50% {
    transform: scale(1.05);
    opacity: 1;
  }
  100% {
    transform: scale(0.95);
    opacity: 0.8;
  }
}

@keyframes wave-animation {
  0% {
    transform: scale(0.5);
    opacity: 0.8;
  }
  100% {
    transform: scale(2);
    opacity: 0;
  }
}

@keyframes char-appear {
  0% {
    opacity: 0;
    transform: translateY(10px);
  }
  100% {
    opacity: 1;
    transform: translateY(0);
  }
}

@keyframes glow-text {
  0% {
    text-shadow: 0 0 5px theme('colors.hacker.accent');
  }
  100% {
    text-shadow: 0 0 15px theme('colors.hacker.accent'), 0 0 20px theme('colors.hacker.accent');
  }
}
</style> 