/**
 * ProbabilityDensityMatcher — 基于高斯概率密度的情感匹配器
 *
 * 架构不变量 A9: 情感标签映射必须输出 Top-3 概率
 *
 * 使用高斯 PDF（概率密度函数）计算 VAD 状态与各情感点的匹配概率，
 * 替代原来的欧氏最近邻匹配，覆盖 VAD [0,1]³ 全部空间。
 */

import { EMOTION_VAD_MAP, BASELINE_STATE, type AffectiveState } from '../personality/affective';
import {
  type EmotionGaussian,
  type MatchResult,
  type ProbabilityMatcherConfig,
  DEFAULT_MATCHER_CONFIG,
} from './types';

/** 高斯标准差（各维度独立），通过 EMOTION_VAD_MAP 推断 */
function inferStdForDimension(values: number[]): number {
  if (values.length === 0) return 0.12;
  const mean = values.reduce((a, b) => a + b, 0) / values.length;
  const variance = values.reduce((sum, v) => sum + (v - mean) ** 2, 0) / values.length;
  // 取标准差和固定最小值的较大者，避免过度锐利
  return Math.max(Math.sqrt(variance || 0.01), 0.08);
}

/** 从 EMOTION_VAD_MAP 构建 22 个情感高斯分布 */
function buildEmotionGaussians(): EmotionGaussian[] {
  const entries = Object.entries(EMOTION_VAD_MAP);

  // 收集各维度值用于推断标准差
  const valences: number[] = [];
  const arousals: number[] = [];
  const dominances: number[] = [];

  for (const [, vad] of entries) {
    valences.push(vad.valence ?? BASELINE_STATE.valence);
    arousals.push(vad.arousal ?? BASELINE_STATE.arousal);
    dominances.push(vad.dominance ?? BASELINE_STATE.dominance);
  }

  const std = {
    valence: inferStdForDimension(valences),
    arousal: inferStdForDimension(arousals),
    dominance: inferStdForDimension(dominances),
  };

  return entries.map(([emotion, vad]) => ({
    emotion,
    mean: {
      valence: vad.valence ?? BASELINE_STATE.valence,
      arousal: vad.arousal ?? BASELINE_STATE.arousal,
      dominance: vad.dominance ?? BASELINE_STATE.dominance,
    },
    std: { ...std },
  }));
}

/**
 * 高斯概率密度函数（单变量）
 * f(x) = (1 / (σ * √(2π))) * exp(-(x - μ)² / (2σ²))
 */
function gaussianPDF(x: number, mean: number, std: number): number {
  if (std <= 0) return x === mean ? 1 : 0;
  const exponent = -((x - mean) ** 2) / (2 * std ** 2);
  return (1 / (std * Math.SQRT2 * Math.sqrt(Math.PI))) * Math.exp(exponent);
}

/**
 * 三变量高斯 PDF（各维度独立，乘积）
 * P(v, a, d) = P(v) * P(a) * P(d)
 */
function multivariateGaussianPDF(
  state: AffectiveState,
  mean: AffectiveState,
  std: AffectiveState
): number {
  const pValence = gaussianPDF(state.valence, mean.valence, std.valence);
  const pArousal = gaussianPDF(state.arousal, mean.arousal, std.arousal);
  const pDominance = gaussianPDF(state.dominance, mean.dominance, std.dominance);
  return pValence * pArousal * pDominance;
}

/**
 * ProbabilityDensityMatcher
 *
 * 使用高斯概率密度函数匹配 VAD 状态到情感标签，
 * 输出 Top-3 概率结果。
 */
export class ProbabilityDensityMatcher {
  private gaussians: EmotionGaussian[];
  private config: ProbabilityMatcherConfig;

  constructor(config: Partial<ProbabilityMatcherConfig> = {}) {
    this.config = { ...DEFAULT_MATCHER_CONFIG, ...config };
    this.gaussians = buildEmotionGaussians();
  }

  /**
   * 更新配置
   */
  updateConfig(config: Partial<ProbabilityMatcherConfig>): void {
    this.config = { ...this.config, ...config };
  }

  /**
   * 匹配 VAD 状态到情感标签
   *
   * @param state 当前 VAD 状态
   * @returns MatchResult[] — Top-N 概率结果，按概率降序排列
   */
  match(state: AffectiveState): MatchResult[] {
    const baseStd = this.gaussians[0]?.std ?? { valence: 0.12, arousal: 0.12, dominance: 0.12 };
    const scaledStd = {
      valence: baseStd.valence * this.config.stdScale,
      arousal: baseStd.arousal * this.config.stdScale,
      dominance: baseStd.dominance * this.config.stdScale,
    };

    // 计算每个情感的高斯 PDF 值
    const rawResults: Array<{ emotion: string; pdf: number }> = this.gaussians.map((g) => ({
      emotion: g.emotion,
      pdf: multivariateGaussianPDF(state, g.mean, scaledStd),
    }));

    // 计算概率和进行 Softmax 归一化
    const totalPDF = rawResults.reduce((sum, r) => sum + r.pdf, 0);
    if (totalPDF <= 0) {
      // 极端情况：所有 PDF 均为 0，返回中性
      return [{ emotion: 'neutral', probability: 1.0 }];
    }

    // 归一化 + 过滤低概率 + 排序
    return rawResults
      .map((r) => ({
        emotion: r.emotion,
        probability: r.pdf / totalPDF,
      }))
      .filter((r) => r.probability >= this.config.minProbability)
      .sort((a, b) => b.probability - a.probability)
      .slice(0, this.config.maxResults);
  }
}
