/**
 * 助手人格成长弧光系统 — 模块入口
 *
 * 整合 L1 核心人格、L2 自适应人格演进引擎、L3 情境人格选择器。
 *
 * 架构不变量:
 *   C1-C7 — 全部覆盖
 */

import { loadCorePersona } from './core';
import { EvolutionEngine } from './adaptive/evolution-engine';
import { SituationalSelector } from './situational/situational-selector';
import { EvolutionLog } from './evolution-log';
import type { EvolutionLogStore } from './evolution-log';
import type { CorePersona, AdaptivePersona, EvolutionConfig, SituationalModulation } from './types';
import { DEFAULT_ADAPTIVE_PERSONA, DEFAULT_EVOLUTION_CONFIG } from './types';
import type { EvolutionContext, EvolutionResult } from './adaptive/evolution-engine';
import type { SituationalContext } from './situational/situational-selector';

export type { CorePersona, AdaptivePersona, EvolutionConfig, SituationalModulation } from './types';
export type { EvolutionLogStore } from './evolution-log';
export { EvolutionEngine, SituationalSelector, EvolutionLog, loadCorePersona };
export type { EvolutionContext, EvolutionResult, SituationalContext };

/** PersonaSystem 配置 */
export interface PersonaSystemConfig {
  evolutionConfig?: Partial<EvolutionConfig>;
  evolutionDbPath?: string;
  /** 自定义演进日志存储（用于测试），不传则默认使用 SQLite EvolutionLog */
  evolutionLogStore?: EvolutionLogStore;
}

/**
 * PersonaSystem — 助手人格系统的统一入口。
 *
 * 使用方式:
 * ```ts
 * const system = new PersonaSystem();
 * const modulation = system.selectSituational({ userMessage: 'hello', turnNumber: 1 });
 * ```
 */
export class PersonaSystem {
  /** L1 核心人格（immutable，架构不变量 C1） */
  readonly core: CorePersona;

  /** L2 自适应人格（可演进） */
  adaptive: AdaptivePersona;

  /** L3 情境选择器 */
  readonly situationalSelector: SituationalSelector;

  /** 演进引擎 */
  readonly evolutionEngine: EvolutionEngine;

  /** 演进日志 */
  readonly evolutionLog: EvolutionLogStore;

  /** 演进配置 */
  private evolutionConfig: EvolutionConfig;

  /** 上次演进时的对话数 */
  private lastEvolutionConversations: number = 0;

  /** 总对话数 */
  totalConversations: number = 0;

  /** 累积反馈分 */
  private accumulatedFeedback: number[] = [];

  constructor(config?: PersonaSystemConfig) {
    // L1: 核心人格（从环境变量加载，架构不变量 C1）
    this.core = loadCorePersona();

    // L2: 自适应人格初始值
    this.adaptive = { ...DEFAULT_ADAPTIVE_PERSONA };

    // 演进日志（支持自定义实现，如测试用的 InMemoryEvolutionLog）
    this.evolutionLog = config?.evolutionLogStore ?? new EvolutionLog(config?.evolutionDbPath);

    // 演进引擎
    this.evolutionConfig = { ...DEFAULT_EVOLUTION_CONFIG, ...config?.evolutionConfig };
    this.evolutionEngine = new EvolutionEngine(this.evolutionConfig, this.evolutionLog);

    // L3: 情境选择器
    this.situationalSelector = new SituationalSelector();
  }

  /**
   * 记录一次对话（更新对话计数）
   */
  recordConversation(): void {
    this.totalConversations++;
  }

  /**
   * 记录用户反馈（用于人格演进）
   * @param score 反馈分 -1（消极）~ 1（积极）
   */
  recordFeedback(score: number): void {
    this.accumulatedFeedback.push(Math.max(-1, Math.min(1, score)));
  }

  /**
   * 获取当前平均反馈分
   */
  getAverageFeedback(): number {
    if (this.accumulatedFeedback.length === 0) return 0;
    const sum = this.accumulatedFeedback.reduce((a, b) => a + b, 0);
    return sum / this.accumulatedFeedback.length;
  }

  /**
   * 执行自适应演进（架构不变量 C2/C3/C4/C5）。
   * 由 index.ts 注册的定时器定期调用。
   *
   * @returns 演进结果
   */
  evolve(): EvolutionResult {
    const ctx: EvolutionContext = {
      core: this.core,
      adaptive: this.adaptive,
      totalConversations: this.totalConversations,
      lastEvolutionConversations: this.lastEvolutionConversations,
      feedbackScore: this.getAverageFeedback(),
    };

    const result = this.evolutionEngine.evolve(ctx);

    if (!result.rolledBack) {
      // 应用演进后的自适应人格
      this.adaptive = { ...result.after };
      this.lastEvolutionConversations = this.totalConversations;
      // 清空已使用的反馈
      this.accumulatedFeedback = [];
    }

    return result;
  }

  /**
   * L3 情境人格选择（实时计算，输出仅在内存 — 架构不变量 C6）。
   *
   * @param context 情境上下文
   * @returns 情境调制参数
   */
  selectSituational(context: SituationalContext): SituationalModulation {
    // 架构不变量 C7: 输出受 L2 自适应人格约束
    return this.situationalSelector.select(this.core, this.adaptive, context);
  }

  /**
   * 更新演进配置
   */
  updateEvolutionConfig(config: Partial<EvolutionConfig>): void {
    this.evolutionConfig = { ...this.evolutionConfig, ...config };
    this.evolutionEngine.updateConfig(this.evolutionConfig);
  }

  /** 清理资源 */
  destroy(): void {
    this.evolutionLog.close();
  }
}
