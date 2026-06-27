/**
 * paradigm — 业务范式注册中心
 *
 * 对应 GAP-10（用户可见翻译走 Business Paradigm 模板）
 * 和 GAP-15（Business Paradigm 可蒸馏）。
 *
 * 每个业务系统注册自己的 Tool 翻译模板，系统按业务系统名查找。
 * T1 模板层（零 AI）覆盖 80%+ 的翻译场景。
 */

import type { BusinessParadigm, ToolTranslationTemplates } from '../types'
import defaultParadigmData from './paradigm.json'

// ── 范式注册表 ────────────────────────────────────────────────────────

class ParadigmRegistry {
  private paradigms = new Map<string, BusinessParadigm>()
  private universal: BusinessParadigm | null = null

  /** 注册一个业务系统的范式 */
  register(paradigm: BusinessParadigm): void {
    this.paradigms.set(paradigm.systemName, paradigm)
  }

  /** 注册通用范式（基线模板，所有系统回退到这里） */
  setUniversal(paradigm: BusinessParadigm): void {
    this.universal = paradigm
  }

  /** 获取某业务系统的范式，未注册则返回通用范式 */
  get(systemName: string): BusinessParadigm | null {
    return this.paradigms.get(systemName) || this.universal
  }

  /** 获取某系统某 tool 的翻译模板，逐级回退：系统级 → 通用级 → null */
  getTemplate(systemName: string, toolName: string): ToolTranslationTemplates | null {
    // 先查系统级
    const paradigm = this.paradigms.get(systemName)
    if (paradigm?.toolTemplates?.[toolName]) {
      return paradigm.toolTemplates[toolName]
    }

    // 回退到通用级
    const uni = this.universal
    if (uni?.toolTemplates?.[toolName]) {
      return uni.toolTemplates[toolName]
    }

    return null
  }

  /** 获取所有已注册的范式 */
  getAll(): BusinessParadigm[] {
    const result: BusinessParadigm[] = []
    if (this.universal) result.push(this.universal)
    for (const paradigm of this.paradigms.values()) {
      result.push(paradigm)
    }
    return result
  }

  /** 计算范式蒸馏结果：从多个业务系统中提炼通用模板（GAP-15） */
  distill(): BusinessParadigm {
    // 统计所有系统中共有的 tool 模板
    const commonTemplates: Record<string, ToolTranslationTemplates> = {}

    if (this.paradigms.size < 2) {
      // 不足 2 个系统时，返回当前 universal 或空
      return this.universal || { systemName: '__universal__', toolTemplates: {} }
    }

    // 统计各 tool 在各系统中的出现频率
    const toolCounts = new Map<string, number>()
    for (const p of this.paradigms.values()) {
      for (const toolName of Object.keys(p.toolTemplates)) {
        toolCounts.set(toolName, (toolCounts.get(toolName) || 0) + 1)
      }
    }

    // 出现频率 > 50% 的 tool 模板作为通用模板候选
    const threshold = this.paradigms.size * 0.5
    for (const [toolName, count] of toolCounts) {
      if (count >= threshold) {
        // 取第一个系统的模板作为通用模板
        for (const p of this.paradigms.values()) {
          if (p.toolTemplates[toolName]) {
            commonTemplates[toolName] = p.toolTemplates[toolName]
            break
          }
        }
      }
    }

    return { systemName: '__universal__', toolTemplates: commonTemplates }
  }
}

// ── 全局实例 ──────────────────────────────────────────────────────────

export const paradigmRegistry = new ParadigmRegistry()

// ── 从 JSON 初始化 ───────────────────────────────────────────────────-

/**
 * 加载默认范式声明。
 * 在应用启动时调用。
 */
export function loadDefaultParadigms(): void {
  // 注册通用范式（基线）
  const uni = (defaultParadigmData as { universal: BusinessParadigm }).universal
  if (uni) {
    paradigmRegistry.setUniversal(uni)
  }

  // 注册各业务系统范式
  const systems = (defaultParadigmData as { systems?: BusinessParadigm[] }).systems
  if (systems) {
    for (const paradigm of systems) {
      paradigmRegistry.register(paradigm)
    }
  }
}
