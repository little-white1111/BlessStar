/**
 * 口头禅类型定义
 *
 * 架构不变量 D1-D6 涉及的核心类型。
 * Core 口头禅：配置驱动，不可变（D5）
 * Relational 口头禅：计数/评分配持久化（D5）
 */

import type { AffectiveState } from '../personality/affective';

/** 口头禅来源 */
export type CatchphraseSource = 'core' | 'relational';

/** 口头禅强度档位 */
export type CatchphraseIntensity = 'low' | 'medium' | 'high';

/** 口头禅变体（不同强度档位） */
export interface CatchphraseVariants {
  low: string;
  medium: string;
  high: string;
}

/** 核心口头禅配置项（来自 config-schema.yaml catchphrase.core） */
export interface CoreCatchphraseItem {
  /** 关联的情绪类型（如 encouragement, calming） */
  emotion_type: string;
  /** 各强度档位的文本变体 */
  variants: CatchphraseVariants;
}

/** Relational 口头禅记录（持久化到 SQLite） */
export interface RelationalCatchphrase {
  /** 唯一标识 */
  id: string;
  /** 口头禅文本 */
  text: string;
  /** 使用次数 */
  usageCount: number;
  /** 累计反馈总分 */
  totalFeedbackScore: number;
  /** 反馈次数 */
  feedbackCount: number;
  /** 平均反馈评分 */
  averageScore: number;
  /** 状态: candidate=候选, active=已启用 */
  status: 'candidate' | 'active';
  /** 关联场景标签（可选） */
  scenarioTag?: string;
  /** 创建时间戳 */
  createdAt: number;
  /** 最近使用时间戳 */
  lastUsedAt?: number;
}

/** 口头禅匹配结果 */
export interface CatchphraseMatch {
  /** 匹配到的口头禅文本 */
  text: string;
  /** 来源类型 */
  source: CatchphraseSource;
  /** 强度档位 */
  intensity: CatchphraseIntensity;
  /** 匹配置信度 0.0~1.0 */
  confidence: number;
  /** 关联的情绪类型（仅 core） */
  emotionType?: string;
  /** 关联的 Relational 口头禅 ID（仅 relational） */
  relationalId?: string;
}

/** EmotionTrigger 匹配上下文 */
export interface EmotionMatchContext {
  /** 当前用户 VAD 状态 */
  currentVAD: AffectiveState;
  /** VAD 基线状态 */
  baselineVAD: AffectiveState;
  /** VAD 偏差幅度 */
  deviation: number;
  /** 主导情绪轴 */
  dominantAxis: 'valence' | 'arousal' | 'dominance';
  /** 偏差方向（positive = 高于基线，negative = 低于基线） */
  deviationDirection: 'positive' | 'negative';
}

/** ScenarioTrigger 匹配上下文 */
export interface ScenarioMatchContext {
  /** 场景标签列表 */
  tags: string[];
}

/** 口头禅选择配置 */
export interface CatchphraseSelectionConfig {
  /** 当前 VAD 状态 */
  currentVAD: AffectiveState;
  /** VAD 基线 */
  baselineVAD: AffectiveState;
  /** 场景标签 */
  scenarioTags: string[];
  /** 自适应人格鼓励倾向值 (0.2~0.9) */
  adaptiveEncouragement: number;
  /** 会话 session ID */
  sessionId: string;
}

/** 用户确认请求 */
export interface PromotionConfirmationRequest {
  /** 候选口头禅 ID */
  catchphraseId: string;
  /** 口头禅文本 */
  text: string;
  /** 当前使用次数 */
  usageCount: number;
  /** 当前平均反馈分 */
  averageScore: number;
}
