/**
 * registry.ts — BusinessAdapterRegistry 全局注册表
 *
 * D38-5-INV-03: Registry 启动注入，运行时只读
 * 管理所有已注册业务适配器，供 AI 管线查询业务数据。
 */

import type { IBusinessAdapter, BusinessAIData, FieldDef, ConfigNormalizer } from './types'

// ── 导出类型（供其他文件引用） ─────────────────────────────────────

export interface DomainShardDef {
  domainName: string
  keywords: string[]
  domainDescription: string
  fieldSemantics?: string
  constraintKnowledge?: string
}

export interface SkillRouteDef {
  prefix: string
  description: string
  toolChain: string[]
  priority: number
  approvalRequired?: boolean
}

// ── Registry 实现 ───────────────────────────────────────────────────

class BusinessAdapterRegistryImpl {
  private adapters: Map<string, IBusinessAdapter> = new Map()
  private _initialized = false

  /** 注册一个业务适配器 */
  register(adapter: IBusinessAdapter): void {
    if (this.adapters.has(adapter.id)) {
      console.warn(`[BusinessAdapterRegistry] 适配器 "${adapter.id}" 已注册，跳过`)
      return
    }
    this.adapters.set(adapter.id, adapter)
    this._initialized = true
  }

  /** 获取指定适配器 */
  get(id: string): IBusinessAdapter | undefined {
    return this.adapters.get(id)
  }

  /** 获取所有已注册适配器 */
  getAll(): IBusinessAdapter[] {
    return Array.from(this.adapters.values())
  }

  /** 是否已初始化（至少注册了一个适配器） */
  get initialized(): boolean {
    return this._initialized
  }

  /** 获取主适配器（第一个注册的），没有则返回 null */
  getPrimary(): IBusinessAdapter | null {
    if (this.adapters.size === 0) return null
    return this.adapters.values().next().value ?? null
  }

  // ── 便捷查询方法 ─────────────────────────────────────────────────

  getAllFieldDeclarations(): FieldDef[] {
    const result: FieldDef[] = []
    for (const adapter of this.adapters.values()) {
      const fields = adapter.getFieldDeclarations()
      if (fields) result.push(...fields)
    }
    return result
  }

  getPrimaryNormalizer(): ConfigNormalizer | null {
    const primary = this.getPrimary()
    return primary ? primary.getNormalizer() : null
  }

  /** 合并所有适配器的 AI 数据 */
  getMergedAIData(): BusinessAIData {
    const merged: BusinessAIData = {}

    for (const adapter of this.adapters.values()) {
      const data = adapter.getAIData()
      if (!data) continue
      merged.configLabels = { ...merged.configLabels, ...data.configLabels }
      merged.configDescriptions = { ...merged.configDescriptions, ...data.configDescriptions }
      merged.operationPermissions = { ...merged.operationPermissions, ...data.operationPermissions }
      merged.invertedIndex = { ...merged.invertedIndex, ...data.invertedIndex }
      merged.configSemanticTypes = { ...merged.configSemanticTypes, ...data.configSemanticTypes }
      merged.baselineKW = { ...merged.baselineKW, ...data.baselineKW }
      merged.executorPatterns = { ...merged.executorPatterns, ...data.executorPatterns }
      if (data.domainShards) {
        merged.domainShards = [...(merged.domainShards || []), ...data.domainShards]
      }
      if (data.skillRoutes) {
        merged.skillRoutes = [...(merged.skillRoutes || []), ...data.skillRoutes]
      }
      if (data.trieDict) {
        merged.trieDict = {
          domainKW: { ...(merged.trieDict?.domainKW || {}), ...data.trieDict.domainKW },
          opKW: { ...(merged.trieDict?.opKW || {}), ...data.trieDict.opKW },
        }
      }
      if (data.paradigm) {
        merged.paradigm = { ...merged.paradigm, ...data.paradigm }
      }
    }

    return merged
  }

  /** 获取所有适配器的系统名（用逗号分隔） */
  getDisplayNames(): string {
    return Array.from(this.adapters.values()).map(a => a.displayName).join(', ')
  }

  /** 获取主适配器的系统 Prompt 身份 */
  getSystemPromptIdentity(): string {
    const primary = this.getPrimary()
    return primary ? primary.getSystemPromptIdentity() : '配置系统'
  }

  /** 获取主适配器的咨询知识 */
  getConsultationKnowledge(): string {
    const primary = this.getPrimary()
    return primary ? primary.getConsultationKnowledge() : ''
  }
}

/** 全局单例 */
export const BusinessAdapterRegistry = new BusinessAdapterRegistryImpl()
