<template>
  <div 
    class="nfc-card" 
    :class="{ 'selected': selected }" 
    :style="cardStyle"
    @click="handleClick"
  >
    <div class="card-content">
      <div class="card-header">
        <div class="card-title">{{ formatTitle(title) }}</div>
        <div class="card-chip"></div>
      </div>
      
      <div class="card-body">
        <div class="signal-waves">
          <div class="wave"></div>
          <div class="wave"></div>
          <div class="wave"></div>
        </div>
        
        <div class="card-preview">
          {{ formatContent(content) }}
        </div>
        
        <!-- 添加查看按钮 -->
        <div class="card-footer">
          <button class="view-button">
            {{ viewText }}
          </button>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue';

const props = defineProps({
  title: {
    type: String,
    default: ''
  },
  id: {
    type: String,
    default: ''
  },
  content: {
    type: String,
    default: ''
  },
  selected: {
    type: Boolean,
    default: false
  },
  clickable: {
    type: Boolean,
    default: true
  },
  theme: {
    type: String,
    default: 'blue',
    validator: (value) => ['blue', 'green', 'red', 'purple', 'yellow', 'cyan', 'orange', 'pink', 'indigo', 'teal'].includes(value)
  },
  viewText: {
    type: String,
    default: '查看'
  }
});

const emit = defineEmits(['click']);

// 格式化标题：将文件名转换为可读标题
const formatTitle = (title) => {
  if (!title) return '';
  
  // 移除文件扩展名
  const nameWithoutExt = title.replace(/\.\w+$/, '');
  
  // 将下划线替换为空格并首字母大写
  return nameWithoutExt
    .replace(/_/g, ' ')
    .replace(/\b\w/g, l => l.toUpperCase());
};

// 格式化内容：截取前几行作为预览
const formatContent = (content) => {
  if (!content) return '';
  
  // 只显示第一行，并且不显示原始内容中的标题行
  const lines = content.split('\n');
  if (lines.length <= 1) return '';
  
  // 返回第二行内容，通常是卡片的关键信息
  return lines[1] || '';
};

// 处理卡片点击事件
const handleClick = () => {
  if (props.clickable) {
    emit('click', { title: props.title, content: props.content });
  }
};

// 获取主题颜色（用于动态绑定）
const getThemeColor = (opacity = 1) => {
  const themeColors = {
    blue: `rgba(59, 130, 246, ${opacity})`,
    green: `rgba(34, 197, 94, ${opacity})`,
    red: `rgba(239, 68, 68, ${opacity})`,
    purple: `rgba(168, 85, 247, ${opacity})`,
    yellow: `rgba(234, 179, 8, ${opacity})`,
    cyan: `rgba(6, 182, 212, ${opacity})`,
    orange: `rgba(249, 115, 22, ${opacity})`,
    pink: `rgba(236, 72, 153, ${opacity})`,
    indigo: `rgba(99, 102, 241, ${opacity})`,
    teal: `rgba(20, 184, 166, ${opacity})`
  };
  
  return themeColors[props.theme] || themeColors.blue;
};

// 计算主题颜色的RGB值（用于CSS变量）
const themeColorRgb = computed(() => {
  const rgbValues = {
    blue: '59, 130, 246',
    green: '34, 197, 94',
    red: '239, 68, 68',
    purple: '168, 85, 247',
    yellow: '234, 179, 8',
    cyan: '6, 182, 212',
    orange: '249, 115, 22',
    pink: '236, 72, 153',
    indigo: '99, 102, 241',
    teal: '20, 184, 166'
  };
  
  return rgbValues[props.theme] || rgbValues.blue;
});

// 计算卡片样式
const cardStyle = computed(() => {
  return {
    '--theme-color-rgb': themeColorRgb.value
  };
});
</script>

<style scoped>
.nfc-card {
  position: relative;
  width: 100%;
  aspect-ratio: 1.6 / 1;
  border-radius: 12px;
  overflow: visible; /* 修改为 visible 以防止边框被裁剪 */
  transition: all 0.3s cubic-bezier(0.165, 0.84, 0.44, 1);
  cursor: pointer;
  background: linear-gradient(135deg, rgba(15, 23, 42, 0.9) 0%, rgba(30, 41, 59, 0.8) 100%);
  box-shadow: 
    0 4px 20px rgba(0, 0, 0, 0.3),
    0 0 0 1px rgba(255, 255, 255, 0.1) inset;
  transform-style: preserve-3d;
  perspective: 1000px;
}

/* 添加选中状态的发光效果 */
.nfc-card.selected {
  box-shadow: 
    0 0 0 2px v-bind('getThemeColor(0.8)') inset,
    0 0 15px v-bind('getThemeColor(0.5)'),
    0 4px 20px rgba(0, 0, 0, 0.3);
  transform: translateY(-2px);
}

/* 添加悬停时的发光效果 */
.nfc-card:hover {
  box-shadow: 
    0 0 0 1px v-bind('getThemeColor(0.3)') inset,
    0 0 10px v-bind('getThemeColor(0.3)'),
    0 8px 24px rgba(0, 0, 0, 0.4);
  transform: translateY(-2px) scale(1.01);
}

/* 添加卡片表面的网格纹理 */
.nfc-card::before {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background-image: 
    linear-gradient(rgba(255, 255, 255, 0.03) 1px, transparent 1px),
    linear-gradient(90deg, rgba(255, 255, 255, 0.03) 1px, transparent 1px);
  background-size: 20px 20px;
  border-radius: 12px;
  z-index: 0;
}

/* 添加卡片表面的光泽效果 */
.nfc-card::after {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: linear-gradient(
    135deg,
    rgba(255, 255, 255, 0.1) 0%,
    rgba(255, 255, 255, 0) 40%,
    rgba(255, 255, 255, 0) 60%,
    rgba(255, 255, 255, 0.05) 100%
  );
  border-radius: 12px;
  z-index: 1;
  pointer-events: none;
}

.card-content {
  position: relative;
  z-index: 2;
  width: 100%;
  height: 100%;
  display: flex;
  flex-direction: column;
  padding: 16px;
  border-radius: 12px; /* 添加圆角到内容区域 */
  overflow: hidden; /* 内容区域保持 hidden 以防止内容溢出 */
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 16px;
}

.card-title {
  font-size: 1.1rem;
  font-weight: 600;
  color: white;
  text-shadow: 0 2px 4px rgba(0, 0, 0, 0.3);
  transition: all 0.3s ease;
}

.card-chip {
  width: 30px;
  height: 24px;
  background: linear-gradient(135deg, #2a2a2a 0%, #1a1a1a 100%);
  border-radius: 4px;
  border: 1px solid v-bind('getThemeColor(0.5)');
  position: relative;
  overflow: hidden;
}

.card-chip::before {
  content: '';
  position: absolute;
  width: 80%;
  height: 60%;
  top: 20%;
  left: 10%;
  background-image: 
    linear-gradient(90deg, transparent 50%, v-bind('getThemeColor(0.3)') 50%),
    linear-gradient(transparent 50%, v-bind('getThemeColor(0.3)') 50%);
  background-size: 4px 4px;
  opacity: 0.7;
}

.card-body {
  flex: 1;
  display: flex;
  flex-direction: column;
  justify-content: space-between;
  position: relative;
  height: calc(100% - 40px); /* 减去header高度 */
}

.signal-waves {
  position: absolute;
  bottom: 6px;
  right: 16px;
  width: 40px;
  height: 40px;
}

.signal-waves .wave {
  position: absolute;
  border: 2px solid v-bind('getThemeColor(0.3)');
  border-radius: 50%;
  width: 100%;
  height: 100%;
  opacity: 0;
}

.card-preview {
  font-size: 0.9rem;
  color: rgba(255, 255, 255, 0.7);
  margin-top: 8px;
  line-height: 1.4;
  max-height: 60px;
  overflow: hidden;
  flex-grow: 1;
}

/* 增强信号波动画效果 */
@keyframes signal-wave {
  0% {
    opacity: 0.3;
    transform: scale(0.8);
  }
  50% {
    opacity: 0.7;
  }
  100% {
    opacity: 0;
    transform: scale(1.2);
  }
}

.signal-waves .wave {
  animation: signal-wave 3s infinite cubic-bezier(0.4, 0, 0.2, 1);
}

.signal-waves .wave:nth-child(2) {
  animation-delay: 0.5s;
}

.signal-waves .wave:nth-child(3) {
  animation-delay: 1s;
}

/* 添加卡片激活时的动画效果 */
@keyframes card-activate {
  0% {
    box-shadow: 
      0 0 0 1px v-bind('getThemeColor(0.3)') inset,
      0 0 5px v-bind('getThemeColor(0.2)');
  }
  50% {
    box-shadow: 
      0 0 0 2px v-bind('getThemeColor(0.8)') inset,
      0 0 20px v-bind('getThemeColor(0.6)');
  }
  100% {
    box-shadow: 
      0 0 0 2px v-bind('getThemeColor(0.8)') inset,
      0 0 15px v-bind('getThemeColor(0.5)');
  }
}

.nfc-card.selected {
  animation: card-activate 1s forwards;
}

/* 添加芯片闪烁效果 */
@keyframes chip-glow {
  0% {
    box-shadow: 0 0 3px v-bind('getThemeColor(0.3)');
  }
  50% {
    box-shadow: 0 0 8px v-bind('getThemeColor(0.6)');
  }
  100% {
    box-shadow: 0 0 3px v-bind('getThemeColor(0.3)');
  }
}

.card-chip {
  animation: chip-glow 4s infinite;
}

/* 添加标题悬停效果 */
.card-title:hover {
  text-shadow: 0 0 8px v-bind('getThemeColor(0.6)');
}

.card-footer {
  margin-top: auto;
  text-align: left;
  padding-top: 8px;
}

.view-button {
  background: rgba(0, 0, 0, 0.3);
  border: 1px solid v-bind('getThemeColor(0.5)');
  border-radius: 4px;
  padding: 6px 12px;
  font-size: 0.8rem;
  color: white;
  cursor: pointer;
  transition: all 0.3s ease;
  box-shadow: 0 0 5px rgba(0, 0, 0, 0.2);
}

.view-button:hover {
  background: rgba(0, 0, 0, 0.5);
  border-color: v-bind('getThemeColor(0.8)');
  box-shadow: 0 0 8px v-bind('getThemeColor(0.4)');
}
</style> 