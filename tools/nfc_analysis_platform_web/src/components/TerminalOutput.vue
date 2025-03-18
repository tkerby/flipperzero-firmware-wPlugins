<template>
  <div class="terminal-container" ref="terminalContainer">
    <div class="terminal-header">
      <div class="terminal-title">{{ title }}</div>
      <div class="terminal-controls">
        <button class="control-button" @click="clear" title="清空终端">
          <svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
            <rect x="3" y="3" width="18" height="18" rx="2" ry="2"></rect>
            <line x1="9" y1="9" x2="15" y2="15"></line>
            <line x1="15" y1="9" x2="9" y2="15"></line>
          </svg>
        </button>
      </div>
    </div>
    <div class="terminal-content" ref="terminalContent">
      <div class="hologram-effect"></div>
      <div class="scan-line"></div>
      <div class="terminal-lines">
        <div v-for="(line, index) in lines" :key="index" class="terminal-line" :class="{ 'fade-in': line.animate }">
          <span class="prompt">{{ prompt }}</span>
          <span class="line-content" v-html="formatLine(line.text)"></span>
        </div>
        <div class="terminal-line cursor-line">
          <span class="prompt">{{ prompt }}</span>
          <span class="cursor" v-if="showCursor"></span>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, nextTick, watch } from 'vue';

const props = defineProps({
  title: {
    type: String,
    default: '终端输出'
  },
  prompt: {
    type: String,
    default: '>>'
  },
  maxLines: {
    type: Number,
    default: 1000
  }
});

const terminalContainer = ref(null);
const terminalContent = ref(null);
const lines = ref([]);
const showCursor = ref(true);

// 格式化行内容，处理特殊标记
const formatLine = (text) => {
  if (!text) return '';
  
  // 处理换行符，将其转换为HTML的换行
  // 注意：需要处理字符串中的实际换行符和转义的\n
  let formattedText = String(text)
    .replace(/\\n/g, '<br>') // 处理转义的\n
    .replace(/\n/g, '<br>'); // 处理实际的换行符
  
  // 保留连续空格
  formattedText = formattedText.replace(/ {2,}/g, match => {
    return '&nbsp;'.repeat(match.length);
  });
  
  // 高亮关键词，但避免将error高亮为errors
  formattedText = formattedText
    .replace(/\b(error|错误)\b/gi, '<span class="highlight-error">$&</span>')
    .replace(/\b(success|成功)\b/gi, '<span class="highlight-success">$&</span>')
    .replace(/\b(warning|警告)\b/gi, '<span class="highlight-warning">$&</span>')
    .replace(/(0x[0-9a-f]+)/gi, '<span class="highlight-hex">$&</span>')
    .replace(/(\d{2} ){5,}/g, '<span class="highlight-data">$&</span>');
  
  return formattedText;
};

// 添加一行或多行内容
const addLines = (newLines) => {
  if (!newLines) return;
  
  const linesToAdd = Array.isArray(newLines) ? newLines : [newLines];
  
  // 为每一行添加动画标记
  const newLinesWithAnimation = linesToAdd.map(text => ({
    text,
    animate: true
  }));
  
  lines.value = [...lines.value, ...newLinesWithAnimation];
  
  // 如果超过最大行数，移除最早的行
  if (lines.value.length > props.maxLines) {
    lines.value = lines.value.slice(lines.value.length - props.maxLines);
  }
  
  // 300ms后移除动画标记
  setTimeout(() => {
    newLinesWithAnimation.forEach(line => {
      line.animate = false;
    });
  }, 300);
  
  // 滚动到底部
  nextTick(() => {
    scrollToBottom();
  });
};

// 清空终端内容
const clear = () => {
  lines.value = [];
};

// 滚动到底部
const scrollToBottom = () => {
  if (terminalContent.value) {
    terminalContent.value.scrollTop = terminalContent.value.scrollHeight;
  }
};

// 光标闪烁效果
onMounted(() => {
  setInterval(() => {
    showCursor.value = !showCursor.value;
  }, 500);
});

// 暴露方法给父组件
defineExpose({
  addLines,
  clear
});
</script>

<style scoped>
.terminal-container {
  width: 100%;
  height: 100%;
  background-color: rgba(22, 27, 34, 0.8);
  border-radius: 8px;
  border: 1px solid rgba(56, 189, 248, 0.3);
  box-shadow: 0 0 20px rgba(0, 0, 0, 0.3), inset 0 0 10px rgba(0, 0, 0, 0.2);
  display: flex;
  flex-direction: column;
  overflow: hidden;
  position: relative;
}

.terminal-container::before {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: 
    linear-gradient(90deg, rgba(56, 189, 248, 0.03) 1px, transparent 1px),
    linear-gradient(rgba(56, 189, 248, 0.03) 1px, transparent 1px);
  background-size: 20px 20px;
  pointer-events: none;
  z-index: 1;
}

.terminal-header {
  height: 36px;
  background-color: rgba(33, 38, 45, 0.8);
  border-bottom: 1px solid rgba(56, 189, 248, 0.2);
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 0 12px;
  z-index: 3;
}

.terminal-title {
  color: rgba(201, 209, 217, 0.9);
  font-size: 14px;
  font-weight: 500;
}

.terminal-controls {
  display: flex;
  gap: 8px;
}

.control-button {
  width: 24px;
  height: 24px;
  border-radius: 4px;
  background-color: rgba(33, 38, 45, 0.6);
  border: 1px solid rgba(56, 189, 248, 0.2);
  color: rgba(201, 209, 217, 0.7);
  display: flex;
  align-items: center;
  justify-content: center;
  cursor: pointer;
  transition: all 0.2s ease;
}

.control-button:hover {
  background-color: rgba(56, 189, 248, 0.1);
  color: rgba(56, 189, 248, 0.9);
  border-color: rgba(56, 189, 248, 0.4);
}

.terminal-content {
  flex: 1;
  overflow-y: auto;
  padding: 12px;
  position: relative;
  z-index: 2;
  scrollbar-width: thin;
  scrollbar-color: rgba(56, 189, 248, 0.3) rgba(22, 27, 34, 0.2);
}

.terminal-content::-webkit-scrollbar {
  width: 6px;
}

.terminal-content::-webkit-scrollbar-track {
  background: rgba(22, 27, 34, 0.2);
}

.terminal-content::-webkit-scrollbar-thumb {
  background-color: rgba(56, 189, 248, 0.3);
  border-radius: 3px;
}

.hologram-effect {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: radial-gradient(circle at center, rgba(56, 189, 248, 0.05) 0%, transparent 70%);
  pointer-events: none;
  z-index: 1;
  animation: hologram-pulse 4s infinite ease-in-out;
}

.scan-line {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  height: 2px;
  background: linear-gradient(90deg, 
    transparent 0%, 
    rgba(56, 189, 248, 0.05) 20%, 
    rgba(56, 189, 248, 0.2) 50%,
    rgba(56, 189, 248, 0.05) 80%,
    transparent 100%
  );
  z-index: 2;
  pointer-events: none;
  animation: scan-line 3s infinite ease-in-out;
}

.terminal-lines {
  position: relative;
  z-index: 3;
  font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
  font-size: 14px;
  line-height: 1.5;
  color: rgba(201, 209, 217, 0.9);
}

.terminal-line {
  margin-bottom: 4px;
  display: flex;
  align-items: flex-start;
  transition: opacity 0.2s ease;
}

.fade-in {
  animation: fade-in 0.3s ease-in-out;
}

.prompt {
  color: rgba(56, 189, 248, 0.9);
  margin-right: 8px;
  font-weight: 600;
  user-select: none;
}

.line-content {
  flex: 1;
  word-break: break-word;
}

.cursor {
  display: inline-block;
  width: 8px;
  height: 16px;
  background-color: rgba(56, 189, 248, 0.7);
  animation: cursor-blink 1s infinite;
  vertical-align: middle;
}

.cursor-line {
  height: 20px;
}

/* 高亮样式 */
:deep(.highlight-error) {
  color: #f87171;
  font-weight: 600;
}

:deep(.highlight-success) {
  color: #34d399;
  font-weight: 600;
}

:deep(.highlight-warning) {
  color: #fbbf24;
  font-weight: 600;
}

:deep(.highlight-hex) {
  color: #a78bfa;
}

:deep(.highlight-data) {
  color: #60a5fa;
  letter-spacing: 0.5px;
}

@keyframes cursor-blink {
  0%, 49% {
    opacity: 1;
  }
  50%, 100% {
    opacity: 0;
  }
}

@keyframes scan-line {
  0% {
    top: 0;
  }
  100% {
    top: 100%;
  }
}

@keyframes hologram-pulse {
  0% {
    opacity: 0.3;
    transform: scale(0.98);
  }
  50% {
    opacity: 0.7;
    transform: scale(1.02);
  }
  100% {
    opacity: 0.3;
    transform: scale(0.98);
  }
}

@keyframes fade-in {
  from {
    opacity: 0;
    transform: translateY(5px);
  }
  to {
    opacity: 1;
    transform: translateY(0);
  }
}
</style> 