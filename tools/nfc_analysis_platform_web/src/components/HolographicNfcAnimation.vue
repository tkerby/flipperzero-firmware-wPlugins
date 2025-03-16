<template>
  <div class="holographic-container">
    <!-- 背景网格 -->
    <div class="grid-background"></div>
    
    <!-- 中央 NFC 标志 -->
    <div class="nfc-hologram">
      <div class="nfc-logo">
        <div class="nfc-rings">
          <div class="ring ring-1"></div>
          <div class="ring ring-2"></div>
          <div class="ring ring-3"></div>
        </div>
        <div class="nfc-symbol">NFC</div>
      </div>
    </div>
    
    <!-- 数据流 -->
    <div class="data-streams">
      <div v-for="n in 8" :key="n" class="data-stream" :class="`stream-${n}`">
        <div v-for="i in 5" :key="i" class="data-packet" :class="`packet-${i}`"></div>
      </div>
    </div>
    
    <!-- 浮动数据标签 -->
    <div class="floating-labels">
      <div class="label label-1">TLV: 6F84A5</div>
      <div class="label label-2">UID: 04A5B9C2</div>
      <div class="label label-3">APDU: 00A4040007</div>
      <div class="label label-4">SW: 9000</div>
    </div>
    
    <!-- 扫描线效果 -->
    <div class="scan-line"></div>
  </div>
</template>

<script setup>
// 组件逻辑
</script>

<style scoped>
.holographic-container {
  position: relative;
  width: 100%;
  height: 300px;
  overflow: hidden;
  background-color: rgba(13, 17, 23, 0.7);
  border-radius: 8px;
  box-shadow: 0 0 30px rgba(0, 0, 0, 0.5) inset;
}

/* 背景网格 */
.grid-background {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background-image: 
    linear-gradient(rgba(56, 189, 248, 0.1) 1px, transparent 1px),
    linear-gradient(90deg, rgba(56, 189, 248, 0.1) 1px, transparent 1px);
  background-size: 20px 20px;
  animation: gridMove 20s linear infinite;
  opacity: 0.5;
}

@keyframes gridMove {
  0% {
    background-position: 0 0;
  }
  100% {
    background-position: 20px 20px;
  }
}

/* 中央 NFC 标志 */
.nfc-hologram {
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  width: 120px;
  height: 120px;
  perspective: 1000px;
}

.nfc-logo {
  position: relative;
  width: 100%;
  height: 100%;
  transform-style: preserve-3d;
  animation: rotate3d 15s linear infinite;
}

@keyframes rotate3d {
  0% {
    transform: rotateY(0deg) rotateX(10deg);
  }
  100% {
    transform: rotateY(360deg) rotateX(10deg);
  }
}

.nfc-rings {
  position: absolute;
  width: 100%;
  height: 100%;
  top: 0;
  left: 0;
}

.ring {
  position: absolute;
  border-radius: 50%;
  border: 2px solid transparent;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
}

.ring-1 {
  width: 80%;
  height: 80%;
  border-color: rgba(56, 189, 248, 0.8);
  animation: pulse 2s ease-in-out infinite alternate;
}

.ring-2 {
  width: 100%;
  height: 100%;
  border-color: rgba(126, 231, 135, 0.6);
  animation: pulse 3s ease-in-out infinite alternate;
}

.ring-3 {
  width: 120%;
  height: 120%;
  border-color: rgba(137, 87, 229, 0.4);
  animation: pulse 4s ease-in-out infinite alternate;
}

@keyframes pulse {
  0% {
    transform: translate(-50%, -50%) scale(0.95);
    opacity: 0.7;
  }
  100% {
    transform: translate(-50%, -50%) scale(1.05);
    opacity: 1;
  }
}

.nfc-symbol {
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  font-size: 24px;
  font-weight: bold;
  color: rgba(255, 255, 255, 0.9);
  text-shadow: 0 0 10px rgba(56, 189, 248, 0.8);
  animation: glow 2s ease-in-out infinite alternate;
}

@keyframes glow {
  0% {
    text-shadow: 0 0 5px rgba(56, 189, 248, 0.8);
  }
  100% {
    text-shadow: 0 0 20px rgba(56, 189, 248, 1);
  }
}

/* 数据流 */
.data-streams {
  position: absolute;
  width: 100%;
  height: 100%;
  top: 0;
  left: 0;
}

.data-stream {
  position: absolute;
  top: 50%;
  left: 50%;
  width: 150px;
  height: 2px;
  transform-origin: left center;
}

.stream-1 { transform: rotate(0deg); }
.stream-2 { transform: rotate(45deg); }
.stream-3 { transform: rotate(90deg); }
.stream-4 { transform: rotate(135deg); }
.stream-5 { transform: rotate(180deg); }
.stream-6 { transform: rotate(225deg); }
.stream-7 { transform: rotate(270deg); }
.stream-8 { transform: rotate(315deg); }

.data-packet {
  position: absolute;
  width: 6px;
  height: 6px;
  border-radius: 50%;
  background-color: rgba(56, 189, 248, 0.8);
  transform: translateY(-50%);
  animation-name: movePacket;
  animation-duration: 4s;
  animation-timing-function: linear;
  animation-iteration-count: infinite;
}

.packet-1 { animation-delay: 0s; }
.packet-2 { animation-delay: 0.8s; }
.packet-3 { animation-delay: 1.6s; }
.packet-4 { animation-delay: 2.4s; }
.packet-5 { animation-delay: 3.2s; }

@keyframes movePacket {
  0% {
    left: 0;
    opacity: 0;
    transform: translateY(-50%) scale(0.5);
  }
  10% {
    opacity: 1;
    transform: translateY(-50%) scale(1);
  }
  90% {
    opacity: 1;
    transform: translateY(-50%) scale(1);
  }
  100% {
    left: 100%;
    opacity: 0;
    transform: translateY(-50%) scale(0.5);
  }
}

/* 浮动数据标签 */
.floating-labels {
  position: absolute;
  width: 100%;
  height: 100%;
  top: 0;
  left: 0;
}

.label {
  position: absolute;
  padding: 4px 8px;
  background-color: rgba(22, 27, 34, 0.7);
  border: 1px solid rgba(56, 189, 248, 0.4);
  border-radius: 4px;
  color: rgba(255, 255, 255, 0.9);
  font-family: monospace;
  font-size: 12px;
  box-shadow: 0 0 10px rgba(56, 189, 248, 0.3);
  animation-name: float;
  animation-duration: 10s;
  animation-timing-function: ease-in-out;
  animation-iteration-count: infinite;
  animation-direction: alternate;
}

.label-1 {
  top: 30%;
  left: 20%;
  animation-delay: 0s;
}

.label-2 {
  top: 60%;
  left: 25%;
  animation-delay: 2s;
}

.label-3 {
  top: 40%;
  left: 70%;
  animation-delay: 1s;
}

.label-4 {
  top: 70%;
  left: 65%;
  animation-delay: 3s;
}

@keyframes float {
  0% {
    transform: translate(0, 0);
  }
  50% {
    transform: translate(10px, -10px);
  }
  100% {
    transform: translate(-10px, 10px);
  }
}

/* 扫描线效果 */
.scan-line {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 2px;
  background: linear-gradient(to right, 
    rgba(56, 189, 248, 0), 
    rgba(56, 189, 248, 0.8), 
    rgba(56, 189, 248, 0));
  animation: scanMove 3s linear infinite;
  box-shadow: 0 0 10px rgba(56, 189, 248, 0.5);
}

@keyframes scanMove {
  0% {
    top: 0;
  }
  100% {
    top: 100%;
  }
}
</style> 