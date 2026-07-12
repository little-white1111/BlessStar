/**
 * 三层人格类型定义（助手人格成长弧光系统）
 *
 * 架构不变量:
 *   C1 — 核心人格不可在运行时修改（L1 字段 immutable）
 *   C6 — 情境人格不得持久化（L3 输出仅在内存）
 *   C7 — 情境人格受 L2 自适应人格约束（输出值必须在 ±0.2 范围内）
 */

// ==================== L1: 核心人格（immutable） ====================

/** L1 核心人格特质 — 从 config-schema.yaml 加载，运行时不可修改 */
export interface CorePersona {
  /** 温暖度 [0.5, 1.0], default 0.8 */
  warmth: number;
  /** 理性度 [0.5, 1.0], default 0.9 */
  rationality: number;
  /** 好奇心 [0.3, 1.0], default 0.6 */
  curiosity: number;
}

/** L1 核心人格默认值（与 config-schema.yaml 一致） */
export const DEFAULT_CORE_PERSONA: CorePersona = {
  warmth: 0.8,
  rationality: 0.9,
  curiosity: 0.6,
};

/** L1 核心人格范围约束 */
export const CORE_PERSONA_RANGES: Record<keyof CorePersona, { min: number; max: number }> = {
  warmth: { min: 0.5, max: 1.0 },
  rationality: { min: 0.5, max: 1.0 },
  curiosity: { min: 0.3, max: 1.0 },
};

// ==================== L2: 自适应人格（可演进） ====================

/** L2 自适应人格特质 — 通过演进引擎缓慢调整 */
export interface AdaptivePersona {
  /** 鼓励倾向 [0.2, 0.9], default 0.5 */
  encouragement: number;
  /** 耐心度 [0.3, 0.95], default 0.6 */
  patience: number;
  /** 主动探询倾向 [0.1, 0.7], default 0.4 */
  proactive_curiosity: number;
}

/** L2 自适应人格默认值 */
export const DEFAULT_ADAPTIVE_PERSONA: AdaptivePersona = {
  encouragement: 0.5,
  patience: 0.6,
  proactive_curiosity: 0.4,
};

/** L2 自适应人格范围约束 */
export const ADAPTIVE_PERSONA_RANGES: Record<keyof AdaptivePersona, { min: number; max: number }> = {
  encouragement: { min: 0.2, max: 0.9 },
  patience: { min: 0.3, max: 0.95 },
  proactive_curiosity: { min: 0.1, max: 0.7 },
};

/** L2 特质 → L1 核心特质映射（用于一致性检查） */
export const ADAPTIVE_TO_CORE_MAP: Record<keyof AdaptivePersona, keyof CorePersona> = {
  encouragement: 'warmth',
  patience: 'rationality',
  proactive_curiosity: 'curiosity',
};

// ==================== 演进配置 ====================

/** 人格演进配置 */
export interface EvolutionConfig {
  /** 正面偏向因子 [0.0, 0.3], default 0.1 — 架构不变量 C4 */
  positive_bias: number;
  /** 每 1000 次对话最大变化量 [0.01, 0.2], default 0.05 */
  change_rate_per_1k: number;
}

/** 默认演进配置 */
export const DEFAULT_EVOLUTION_CONFIG: EvolutionConfig = {
  positive_bias: 0.1,
  change_rate_per_1k: 0.05,
};

// ==================== L3: 情境人格（实时，不持久化） ====================

/** L3 情境人格调制维度 — 每次对话实时计算，不持久化（架构不变量 C6） */
export interface SituationalModulation {
  /** 语气风格 */
  tone: 'warm' | 'neutral' | 'professional';
  /** 表达力度 0.0~1.0（受 L2 encouragement 约束 ±0.2） */
  expressiveness: number;
  /** 正式度 0.0~1.0（受 L2 patience 约束 ±0.2） */
  formality: number;
}

// ==================== 完整人格状态 ====================

/** 完整人格状态（L1 + L2） */
export interface PersonaState {
  core: CorePersona;
  adaptive: AdaptivePersona;
}

// ==================== 演进日志 ====================

/** 演进日志条目 — 写入 evolution_log 表（架构不变量 C5） */
export interface EvolutionLogEntry {
  /** 特质名称 */
  trait_name: string;
  /** 旧值 */
  old_value: number;
  /** 新值 */
  new_value: number;
  /** 变更原因 */
  reason: string;
  /** 一致性检查状态 */
  consistency_check: 'passed' | 'failed' | 'skipped';
  /** 时间戳 */
  timestamp: number;
}
