/**
 * 引擎事件总线
 * 遵循架构不变量 I7：禁止组件间直接耦合通信，必须通过 EventBus 或配置变更间接通信
 */

/** 事件监听器类型 */
type EventListener<T = unknown> = (payload: T) => void;

/** 取消订阅函数 */
type Unsubscribe = () => void;

/**
 * 轻量事件总线
 * 支持命名空间事件、一次性监听、异步派发
 */
export class EventBus {
  private listeners = new Map<string, Set<EventListener>>();
  private onceListeners = new Map<string, Set<EventListener>>();

  /**
   * 订阅事件
   * @param event 事件名称
   * @param listener 监听器
   * @returns 取消订阅函数
   */
  on<T = unknown>(event: string, listener: EventListener<T>): Unsubscribe {
    if (!this.listeners.has(event)) {
      this.listeners.set(event, new Set());
    }
    this.listeners.get(event)!.add(listener as EventListener);

    return () => {
      this.listeners.get(event)?.delete(listener as EventListener);
    };
  }

  /**
   * 一次性订阅事件
   * @param event 事件名称
   * @param listener 监听器
   * @returns 取消订阅函数
   */
  once<T = unknown>(event: string, listener: EventListener<T>): Unsubscribe {
    if (!this.onceListeners.has(event)) {
      this.onceListeners.set(event, new Set());
    }
    this.onceListeners.get(event)!.add(listener as EventListener);

    return () => {
      this.onceListeners.get(event)?.delete(listener as EventListener);
    };
  }

  /**
   * 派发事件
   * @param event 事件名称
   * @param payload 事件载荷
   */
  emit<T = unknown>(event: string, payload: T): void {
    // 触发普通监听器
    this.listeners.get(event)?.forEach((listener) => {
      try {
        listener(payload);
      } catch (err) {
        console.error(`[EventBus] Error in listener for "${event}":`, err);
      }
    });

    // 触发一次性监听器并自动清理
    this.onceListeners.get(event)?.forEach((listener) => {
      try {
        listener(payload);
      } catch (err) {
        console.error(`[EventBus] Error in once-listener for "${event}":`, err);
      }
    });
    this.onceListeners.delete(event);
  }

  /**
   * 异步派发事件（所有监听器在下一个 microtask 执行）
   * @param event 事件名称
   * @param payload 事件载荷
   */
  async emitAsync<T = unknown>(event: string, payload: T): Promise<void> {
    await Promise.resolve();
    this.emit(event, payload);
  }

  /**
   * 取消所有事件订阅
   * @param event 可选，指定事件名称
   */
  clear(event?: string): void {
    if (event) {
      this.listeners.delete(event);
      this.onceListeners.delete(event);
    } else {
      this.listeners.clear();
      this.onceListeners.clear();
    }
  }

  /**
   * 获取指定事件的监听器数量
   */
  listenerCount(event: string): number {
    const normal = this.listeners.get(event)?.size ?? 0;
    const once = this.onceListeners.get(event)?.size ?? 0;
    return normal + once;
  }
}
