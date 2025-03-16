<!--
 * @Author: SpenserCai
 * @Date: 2025-03-16 23:16:25
 * @version: 
 * @LastEditors: SpenserCai
 * @LastEditTime: 2025-03-16 23:32:54
 * @Description: file content
-->
<template>
  <span 
    @click="handleClick" 
    class="cursor-pointer px-2 py-1 rounded text-xs font-medium transition-colors duration-200 inline-flex items-center"
    :class="colorClass"
  >
    <slot></slot>
  </span>
</template>

<script setup>
import { computed } from 'vue';

const props = defineProps({
  text: {
    type: String,
    default: ''
  },
  level: {
    type: Number,
    default: 0
  },
  colorIndex: {
    type: Number,
    default: null
  }
});

const emit = defineEmits(['click']);

// 定义10种不同的颜色，全部使用 ark 风格
const colorClasses = [
  'bg-ark-blue-light/20 text-ark-blue-light hover:bg-ark-blue-light/30',
  'bg-ark-green-light/20 text-ark-green-light hover:bg-ark-green-light/30',
  'bg-ark-yellow-light/20 text-ark-yellow-light hover:bg-ark-yellow-light/30',
  'bg-ark-red-light/20 text-ark-red-light hover:bg-ark-red-light/30',
  'bg-ark-purple-light/20 text-ark-purple-light hover:bg-ark-purple-light/30',
  'bg-ark-cyan-light/20 text-ark-cyan-light hover:bg-ark-cyan-light/30',
  'bg-ark-orange-light/20 text-ark-orange-light hover:bg-ark-orange-light/30',
  'bg-ark-pink-light/20 text-ark-pink-light hover:bg-ark-pink-light/30',
  'bg-ark-indigo-light/20 text-ark-indigo-light hover:bg-ark-indigo-light/30',
  'bg-ark-teal-light/20 text-ark-teal-light hover:bg-ark-teal-light/30'
];

// 根据层级或指定的索引选择颜色
const colorClass = computed(() => {
  const index = props.colorIndex !== null ? props.colorIndex : props.level;
  return colorClasses[index % colorClasses.length];
});

// 处理点击事件
const handleClick = () => {
  emit('click', props.text || '');
};
</script> 