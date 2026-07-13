/**
 * 概率匹配引擎类型定义
 *
 * VAD 概率场模型 — 架构不变量 A9: 情感标签映射必须输出 Top-3 概率
 */

import type { AffectiveState } from '../personality/affective';

/** 情感高斯分布定义（用于概率密度匹配） */
export interface EmotionGaussian {
  /** 情绪名称 */
  emotion: string;
  /** 高斯分布的中心（均值）VAD */
  mean: AffectiveState;
  /** 高斯分布的标准差（各维度可不同） */
  std: AffectiveState;
}

/** 匹配结果：情绪 + 概率 */
export interface MatchResult {
  emotion: string;
  /** 概率值 0.0~1.0 */
  probability: number;
}

/** 概率密度匹配器配置 */
export interface ProbabilityMatcherConfig {
  /** 高斯标准差缩放因子（默认 1.0，越大匹配越宽松） */
  stdScale: number;
  /** 最低概率阈值（低于此值不返回，默认 0.01） */
  minProbability: number;
  /** 最大返回结果数（默认 3） */
  maxResults: number;
}

/** 默认配置 */
export const DEFAULT_MATCHER_CONFIG: ProbabilityMatcherConfig = {
  stdScale: 1.0,
  minProbability: 0.01,
  maxResults: 3,
};
