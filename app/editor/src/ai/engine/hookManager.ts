/**
 * HookManager — application-level hook registration and dispatch system.
 *
 * ADR-全链路接通 — provides a generic hook mechanism for pipeline lifecycle
 * events that is independent of DomainPlugin hooks.
 *
 * Usage:
 *   HookManager.on('pipeline:beforeStage', async (ctx) => { ... })
 *   HookManager.dispatch('pipeline:beforeStage', ctx)
 */

type HookHandler = (...args: unknown[]) => Promise<void> | void

class HookManagerImpl {
  private hooks = new Map<string, HookHandler[]>()

  /**
   * Register a handler for the given event.
   * @returns An unsubscribe function.
   */
  on(event: string, handler: HookHandler): () => void {
    if (!this.hooks.has(event)) {
      this.hooks.set(event, [])
    }
    this.hooks.get(event)!.push(handler)
    return () => this.off(event, handler)
  }

  /**
   * Remove a specific handler for the given event.
   */
  off(event: string, handler: HookHandler): void {
    const handlers = this.hooks.get(event)
    if (!handlers) return
    const idx = handlers.indexOf(handler)
    if (idx >= 0) handlers.splice(idx, 1)
    if (handlers.length === 0) this.hooks.delete(event)
  }

  /**
   * Dispatch an event to all registered handlers.
   */
  async dispatch(event: string, ...args: unknown[]): Promise<void> {
    const handlers = this.hooks.get(event)
    if (!handlers) return
    for (const handler of handlers) {
      await handler(...args)
    }
  }

  /**
   * Check if any handler is registered for the given event.
   */
  has(event: string): boolean {
    const handlers = this.hooks.get(event)
    return handlers !== undefined && handlers.length > 0
  }

  /**
   * Remove all handlers for all events.
   */
  clear(): void {
    this.hooks.clear()
  }
}

export const HookManager = new HookManagerImpl()
