import { ref } from 'vue';

/** 默认悬浮球位置（右下角偏移） */
const DEFAULT_X = 20;
const DEFAULT_Y = 20;

export function useFloatBall() {
  const position = ref({ x: DEFAULT_X, y: DEFAULT_Y });
  const isDragging = ref(false);
  const dragStart = ref({ x: 0, y: 0 });
  const dragOffset = ref({ x: 0, y: 0 });

  /** 开始拖拽，记录起始偏移量 */
  function startDrag(e: MouseEvent): void {
    isDragging.value = true;
    dragStart.value = { x: e.clientX, y: e.clientY };
    dragOffset.value = {
      x: e.clientX - position.value.x,
      y: e.clientY - position.value.y,
    };
  }

  /** 拖拽中，更新位置 */
  function onDrag(e: MouseEvent): void {
    if (!isDragging.value) return;

    const newX = e.clientX - dragOffset.value.x;
    const newY = e.clientY - dragOffset.value.y;

    // 限制在视口范围内
    const maxX = window.innerWidth - 56;
    const maxY = window.innerHeight - 56;

    position.value = {
      x: Math.max(0, Math.min(newX, maxX)),
      y: Math.max(0, Math.min(newY, maxY)),
    };
  }

  /** 结束拖拽，持久化位置（架构不变量 #10） */
  function endDrag(): void {
    if (isDragging.value) {
      isDragging.value = false;
      window.electronAPI.savePosition(position.value.x, position.value.y);
    }
  }

  return {
    position,
    isDragging,
    startDrag,
    onDrag,
    endDrag,
  };
}
