<script setup lang="ts">
import { ref, computed } from 'vue';
import { useFloatBall } from '../composables/useFloatBall';

const props = withDefaults(
  defineProps<{
    opacity?: number;
  }>(),
  {
    opacity: 0.85,
  },
);

const emit = defineEmits<{
  openChat: [];
  openSettings: [];
  openRoleSelector: [];
}>();

const { position, isDragging, startDrag, onDrag, endDrag } = useFloatBall();

// 右键菜单
const contextMenuVisible = ref(false);
const contextMenuPos = ref({ x: 0, y: 0 });

function handleMouseDown(e: MouseEvent): void {
  if (e.button !== 0) return; // 仅左键拖拽
  startDrag(e);
}

function handleMouseMove(e: MouseEvent): void {
  onDrag(e);
}

function handleMouseUp(e: MouseEvent): void {
  const wasDragging = isDragging.value;
  endDrag();

  // 如果几乎没有移动，视为点击而非拖拽
  if (!wasDragging) return;
  const dx = Math.abs(e.clientX - position.value.x);
  const dy = Math.abs(e.clientY - position.value.y);
  if (dx < 5 && dy < 5) {
    emit('openChat');
  }
}

function handleClick(): void {
  // 仅在非拖拽状态下触发
  if (!isDragging.value) {
    emit('openChat');
  }
}

function handleContextMenu(e: MouseEvent): void {
  e.preventDefault();
  contextMenuPos.value = { x: e.clientX, y: e.clientY };
  contextMenuVisible.value = true;
}

function closeContextMenu(): void {
  contextMenuVisible.value = false;
}

function onContextOption(action: string): void {
  contextMenuVisible.value = false;
  if (action === 'settings') emit('openSettings');
  if (action === 'role') emit('openRoleSelector');
  if (action === 'chat') emit('openChat');
}

const ballStyle = computed(() => ({
  left: `${position.value.x}px`,
  top: `${position.value.y}px`,
  opacity: props.opacity,
}));
</script>

<template>
  <!-- 悬浮球 -->
  <div
    class="float-ball"
    :class="{ dragging: isDragging }"
    :style="ballStyle"
    @mousedown="handleMouseDown"
    @mousemove="handleMouseMove"
    @mouseup="handleMouseUp"
    @mouseleave="endDrag"
    @click="handleClick"
    @contextmenu="handleContextMenu"
  >
    <span>🤖</span>
  </div>

  <!-- 右键菜单 -->
  <div
    v-if="contextMenuVisible"
    class="context-menu"
    :style="{ left: `${contextMenuPos.x}px`, top: `${contextMenuPos.y}px` }"
  >
    <div class="context-menu-item" @click="onContextOption('chat')">打开对话</div>
    <div class="context-menu-item" @click="onContextOption('role')">切换角色</div>
    <div class="context-menu-divider" />
    <div class="context-menu-item" @click="onContextOption('settings')">设置</div>
  </div>

  <!-- 点击右键菜单外部关闭 -->
  <div
    v-if="contextMenuVisible"
    class="context-menu-overlay"
    style="position: fixed; inset: 0; z-index: 10000"
    @click="closeContextMenu"
    @contextmenu.prevent="closeContextMenu"
  />
</template>
