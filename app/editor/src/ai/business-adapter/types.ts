/**
 * types.ts — IBusinessAdapter 接口定义
 *
 * BlessStar 核心定义，业务系统实现此接口以接入 AI 管线。
 * D38-5-INV-02: BlessStar 定义接口，业务侧实现
 */

import type { DomainShardDef, SkillRouteDef } from './registry'

export interface FieldDef {
  key: string
  type: string
  defaultStr: string
  description: string
  required: boolean
}

export interface ConfigNormalizer {
  /** 将业务系统原始配置归一化为 BlessStar Config v1 JSON */
  normalize(storageJson: string, sensitiveJson?: string): string | null
}

export interface BusinessAIData {
  /** 配置字段 key → 中文标签 */
  configLabels?: Record<string, string>
  /** 配置字段 key → 自然语言描述（供 LLM 模糊匹配） */
  configDescriptions?: Record<string, string>
  /** 配置字段 key → 允许的操作列表 */
  operationPermissions?: Record<string, string[]>
  /** 关键词 → configKey 列表 */
  invertedIndex?: Record<string, string[]>
  /** 三元组字典：领域词 → domain 路径；操作词 → operation */
  trieDict?: {
    domainKW: Record<string, string>
    opKW: Record<string, string>
  }
  /** 领域分片 */
  domainShards?: DomainShardDef[]
  /** /command 路由 */
  skillRoutes?: SkillRouteDef[]
  /** 翻译模板 */
  paradigm?: Record<string, unknown>
  /** 配置语义类型 */
  configSemanticTypes?: Record<string, 'config_value' | 'directory' | 'file_path' | 'url'>
  /** 出厂基线关键词表（三阶权重 AdaptiveIndex 用） */
  baselineKW?: Record<string, string[]>
  /** executor pattern 索引（消息→query.audience 等） */
  executorPatterns?: Record<string, string>
}

export interface IBusinessAdapter {
  /** 唯一标识 */
  readonly id: string
  /** 显示名称（如 "LiveDesign"） */
  readonly displayName: string
  /** 配置字段声明（传给 native registerConfigFields） */
  getFieldDeclarations(): FieldDef[]
  /** 归一化器：异构配置 → Config v1 JSON（可选） */
  getNormalizer(): ConfigNormalizer | null
  /** AI 管线数据（可选） */
  getAIData(): BusinessAIData | null
  /** 咨询知识文本 */
  getConsultationKnowledge(): string
  /** 系统 Prompt 身份标识 */
  getSystemPromptIdentity(): string
}
