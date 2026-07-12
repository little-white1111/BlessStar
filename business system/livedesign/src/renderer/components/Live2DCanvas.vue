<script setup lang="ts">
import { watch, onMounted } from 'vue';
import { useLive2D } from '../composables/useLive2D';
import { useEmotion } from '../composables/useEmotion';
import type { VADState } from '../composables/useEmotion';

const props = defineProps<{
  width?: number;
  height?: number;
}>();

const { canvasRef, state, applyEmotion, applyVAD } = useLive2D();

/**
 * 情绪驱动动画（架构不变量 #8/#A6）
 * useEmotion 自动监听 window 上的 emotion-update 事件
 * 当收到情绪更新时，触发 applyEmotion 驱动 Live2D 模型，
 * 并将 VAD 值同步到表情参数权重
 */
useEmotion((emotion, action, intensity, vad) => {
  applyEmotion(emotion, action, intensity);
  // 架构不变量 A6: VAD 同步到表情参数
  if (vad) {
    applyVAD(vad);
  }
});

onMounted(() => {
  if (canvasRef.value) {
    // 初始化 Canvas 上下文
    const gl = canvasRef.value.getContext('webgl2') || canvasRef.value.getContext('webgl');
    if (gl) {
      gl.clearColor(0, 0, 0, 0); // 透明背景
      gl.clear(gl.COLOR_BUFFER_BIT);
    }
  }
});
</script>

<template>
  <canvas
    ref="canvasRef"
    class="live2d-canvas"
    :width="width ?? 300"
    :height="height ?? 400"
  />
</template>

<style scoped>
/*
 * Live2D 渲染容器
 * 架构不变量 #3：Live2D 渲染目标 60fps
 *
 * 当前 Canvas 用于 WebGL 渲染。
 * 实际渲染由 Cubism SDK 接管，集成方式：
 *   1. 将 canvasRef 传给 Live2DModel.fromModelJSON()
 *   2. 使用 requestAnimationFrame 循环驱动渲染，目标 60fps
 *   3. 通过 applyEmotion / applyAction 控制模型参数和动作
 */
canvas.live2d-canvas {
  pointer-events: none;
  position: fixed;
  bottom: 0;
  right: 0;
  z-index: 9998;
}
</style>
