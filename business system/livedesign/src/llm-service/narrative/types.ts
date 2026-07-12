/**
 * 用户叙事与动态画像系统 — 类型定义
 *
 * 架构不变量：
 *   B2 — L2 量化指标由程序自动计算，不得引入 LLM
 *   B3 — L3 反思输出必须标注 confidence，低于 0.4 的不写入画像
 *   B4 — L4 动态画像不可直接用于在线对话决策
 *   B5 — 用户叙事档案必须支持完整删除/重置（遗忘权）
 *   B6 — 反思每轮输出必须携带 version 时间戳，支持多轮回溯
 */

// ==================== L1: 原始观察 ====================

/** L1 原始观察记录 — 所有 Agent 输出必须先写入此层（B1） */
export interface RawObservation {
  /** 自增 ID */
  id?: number;
  /** 会话 ID */
  sessionId: string;
  /** 用户标识 */
  userId: string;
  /** 观察文本内容 */
  content: string;
  /** 关联的情绪标签 */
  emotion: string;
  /** 情感强度 [0, 1] */
  intensity: number;
  /** VAD 愉悦度 [0, 1] */
  valence: number;
  /** VAD 激活度 [0, 1] */
  arousal: number;
  /** VAD 支配度 [0, 1] */
  dominance: number;
  /** 是否为重要节点（由情感强度阈值决定） */
  significant: boolean;
  /** 创建时间戳（毫秒） */
  createdAt: number;
}

/** 创建 L1 观察记录的参数 */
export interface CreateObservationParams {
  sessionId: string;
  content: string;
  emotion: string;
  intensity: number;
  valence: number;
  arousal: number;
  dominance: number;
}

// ==================== L2: 量化指标 ====================

/** L2 量化指标类型 */
export type MetricType = 'emotional_trajectory_ma' | 'theme_density' | 'interaction_pattern';

/** L2 量化指标记录 */
export interface QuantitativeMetric {
  id?: number;
  userId: string;
  /** 指标大类 */
  metricType: MetricType;
  /** 具体指标名 */
  metricName: string;
  /** 指标值 */
  value: number;
  /** 滑动窗口大小（观察数） */
  windowSize: number;
  /** 统计周期开始时间戳 */
  periodStart: number;
  /** 统计周期结束时间戳 */
  periodEnd: number;
  /** 创建时间戳 */
  createdAt: number;
}

// ==================== L3: 反思假设 ====================

/** L3 反思假设 — 由 LLM 分析生成（B3: 必须标注 confidence） */
export interface ReflectiveHypothesis {
  id?: number;
  /** 用户标识 */
  userId: string;
  /** 版本时间戳 — 支持多轮回溯（B6） */
  version: string;
  /** 假设所属领域 */
  domain: string;
  /** 假设内容 */
  hypothesis: string;
  /** 支持证据 */
  evidence: string;
  /** 置信度 [0, 1] — 低于 0.4 不写入画像（B3） */
  confidence: number;
  /** 引用的观察记录 ID 列表 */
  observationRefs: number[];
  /** 创建时间戳 */
  createdAt: number;
}

// ==================== L4: 动态画像 ====================

/** 轨迹属性趋势 */
export type TrendDirection = 'rising' | 'falling' | 'stable';

/** L4 动态画像 — 不可直接用于在线对话决策（B4） */
export interface DynamicProfile {
  /** 版本时间戳 */
  version: string;
  /** 用户标识 */
  userId: string;
  /** 当前活跃的假设列表（仅 confidence >= 0.4） */
  hypotheses: ReflectiveHypothesis[];
  /** 成长轨迹属性值序列 */
  trajectoryAttributes: Record<string, number[]>;
  /** 合成时间戳 */
  mergedAt: number;
  /** 各属性的趋势方向 */
  attributeTrends: Record<string, TrendDirection>;
}

// ==================== 配置 ====================

/** 叙事系统配置 */
export interface NarrativeConfig {
  /** L2 量化计算 cron 表达式 */
  quantifySchedule: string;
  /** L3 反思 cron 表达式 */
  reflectSchedule: string;
  /** 触发反思所需的最小观察数 */
  minObservations: number;
  /** 标记重要节点的情感强度阈值 */
  significantThreshold: number;
  /** 可追踪的成长轨迹属性列表 */
  trajectoryAttributes: string[];
  /** 原始观察保留天数 */
  retentionDays: number;
  /** 模式识别灵敏度 [0.05, 0.8] */
  patternSensitivity: number;
}

/** 默认叙事配置 */
export const DEFAULT_NARRATIVE_CONFIG: NarrativeConfig = {
  quantifySchedule: '0 */6 * * *',
  reflectSchedule: '0 2 * * 0',
  minObservations: 20,
  significantThreshold: 0.7,
  trajectoryAttributes: ['self_efficacy', 'emotional_expression', 'self_acceptance'],
  retentionDays: 365,
  patternSensitivity: 0.3,
};
