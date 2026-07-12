/**
 * L2 自适应人格演进引擎
 *
 * 架构不变量:
 *   C1 — 演进引擎跳过 L1 核心人格字段
 *   C2 — 演进后调用 ConsistencyChecker.Check()，失败则回滚+警告
 *   C3 — 自适应人格值被 min/max 硬性截断（clamp）
 *   C4 — 演进方向包含正面偏向（delta = feedbackDelta + positive_bias）
 *   C5 — 每次演进记录 evolution_log
 *
 * 触发方式: 月/季度定时触发（由 index.ts 注册计时器）
 */

import type { CorePersona, AdaptivePersona, EvolutionConfig, PersonaState } from '../types';
import { ADAPTIVE_PERSONA_RANGES, DEFAULT_ADAPTIVE_PERSONA, DEFAULT_EVOLUTION_CONFIG } from '../types';
import { isCoreField } from '../core';
import { checkConsistency } from './consistency-checker';
import type { EvolutionLogStore } from '../evolution-log';

/** 演进上下文 */
export interface EvolutionContext {
  /** 核心人格（只读） */
  core: CorePersona;
  /** 当前自适应人格 */
  adaptive: AdaptivePersona;
  /** 对话总数（用于计算变化幅度） */
  totalConversations: number;
  /** 上次演进时的对话数 */
  lastEvolutionConversations: number;
  /** 用户反馈汇总（最近周期内的平均反馈分，-1~1，负值表示消极） */
  feedbackScore: number;
}

/** 演进结果 */
export interface EvolutionResult {
  /** 演进前的自适应人格 */
  before: AdaptivePersona;
  /** 演进后的自适应人格 */
  after: AdaptivePersona;
  /** 一致性检查是否通过 */
  consistencyPassed: boolean;
  /** 如果一致性检查失败，是否已回滚 */
  rolledBack: boolean;
  /** 变更记录 */
  changes: Array<{
    trait: string;
    oldValue: number;
    newValue: number;
    delta: number;
  }>;
}

/** L2 自适应人格演进引擎 */
export class EvolutionEngine {
  private config: EvolutionConfig;
  private log: EvolutionLogStore;

  constructor(config: Partial<EvolutionConfig> = {}, log: EvolutionLogStore) {
    this.config = { ...DEFAULT_EVOLUTION_CONFIG, ...config };
    this.log = log;
  }

  /**
   * 更新演进配置
   */
  updateConfig(config: Partial<EvolutionConfig>): void {
    this.config = { ...this.config, ...config };
  }

  /**
   * 执行一次自适应演进（架构不变量 C2/C3/C4/C5）。
   *
   * @param ctx 演进上下文
   * @returns 演进结果
   */
  evolve(ctx: EvolutionContext): EvolutionResult {
    const before: AdaptivePersona = { ...ctx.adaptive };
    const changes: EvolutionResult['changes'] = [];

    // 计算对话增量
    const conversationDelta = ctx.totalConversations - ctx.lastEvolutionConversations;
    if (conversationDelta <= 0) {
      // 没有新对话，无变化
      return {
        before,
        after: { ...before },
        consistencyPassed: true,
        rolledBack: false,
        changes: [],
      };
    }

    // 计算变化率因子（基于对话增量 / 1000 * change_rate_per_1k）
    const changeFactor = (conversationDelta / 1000) * this.config.change_rate_per_1k;

    // 遍历所有 L2 自适应特质（跳过 L1 核心特质，架构不变量 C1）
    const adaptiveKeys: Array<keyof AdaptivePersona> = ['encouragement', 'patience', 'proactive_curiosity'];
    const newAdaptive: AdaptivePersona = { ...before };

    for (const key of adaptiveKeys) {
      if (isCoreField(key)) {
        // 架构不变量 C1: 跳过 L1 核心字段
        continue;
      }

      const range = ADAPTIVE_PERSONA_RANGES[key];
      const currentValue = before[key];

      // 计算反馈增量（架构不变量 C4: delta = feedbackDelta + positive_bias）
      const feedbackDelta = ctx.feedbackScore * changeFactor;
      const delta = feedbackDelta + this.config.positive_bias * changeFactor;

      if (Math.abs(delta) < 0.0001) {
        continue; // 无显著变化
      }

      // 架构不变量 C3: 硬性 clamp
      const newValue = clamp(currentValue + delta, range.min, range.max);

      if (Math.abs(newValue - currentValue) > 0.0001) {
        newAdaptive[key] = newValue;
        changes.push({
          trait: key,
          oldValue: currentValue,
          newValue,
          delta,
        });
      }
    }

    // 架构不变量 C2: 一致性检查
    const consistencyResult = checkConsistency(ctx.core, newAdaptive);

    // 架构不变量 C5: 记录日志
    for (const change of changes) {
      this.log.write({
        trait_name: change.trait,
        old_value: change.oldValue,
        new_value: change.newValue,
        reason: `feedbackScore=${ctx.feedbackScore.toFixed(3)}`,
        consistency_check: consistencyResult.passed ? 'passed' : 'failed',
      });
    }

    // 架构不变量 C2: 失败则回滚
    if (!consistencyResult.passed) {
      return {
        before,
        after: { ...before }, // 回滚到演进前
        consistencyPassed: false,
        rolledBack: true,
        changes: [], // 回滚后无有效变更
      };
    }

    return {
      before,
      after: newAdaptive,
      consistencyPassed: true,
      rolledBack: false,
      changes,
    };
  }
}

function clamp(value: number, min: number, max: number): number {
  return Math.max(min, Math.min(max, value));
}
