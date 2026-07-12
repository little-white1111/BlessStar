let counter = 0;

/** 生成唯一 ID */
export function generateId(prefix = 'comp'): string {
  counter++;
  return `${prefix}_${Date.now()}_${counter}`;
}

/** 重置计数器（用于测试） */
export function resetCounter(): void {
  counter = 0;
}
