/**
 * 用户叙事与动态画像系统 — 模块入口
 *
 * 架构不变量：
 *   B1 — 所有 Agent 输出必须先写入 L1 原始观察层，不可跳过
 *   B2 — L2 量化指标由程序自动计算，不得引入 LLM
 *   B3 — L3 反思输出必须标注 confidence，低于 0.4 的不写入画像
 *   B4 — L4 动态画像不可直接用于在线对话决策
 *   B5 — 用户叙事档案必须支持完整删除/重置（遗忘权）
 *   B6 — 反思每轮输出必须携带 version 时间戳，支持多轮回溯
 *   B7 — 叙事子系统故障不得影响主交互管线
 *
 * 使用方式：
 *   import { narrativeSystem } from './narrative';
 *   narrativeSystem.record({ sessionId, content, emotion, ... });
 */

import { NarrativeStore, type INarrativeStore } from './storage/sqlite-store';
import { QuantifyScheduler } from './quantify/quantify-scheduler';
import { ReflectScheduler } from './reflect/reflect-scheduler';
import { ProfileMerger } from './profile/profile-merger';
import type { CreateObservationParams, NarrativeConfig, DynamicProfile, ReflectiveHypothesis } from './types';
import { DEFAULT_NARRATIVE_CONFIG } from './types';
import type { ExpressionResult } from '../agent/expresser';

/**
 * 叙事系统主类
 *
 * 整合 L1~L4 全链路：
 *   1. L1 写入 → 2. L2 定时量化 → 3. L3 定时反思 → 4. L4 画像合成
 */
export class NarrativeSystem {
  readonly store: INarrativeStore;
  readonly quantify: QuantifyScheduler;
  readonly reflect: ReflectScheduler;
  readonly profileMerger: ProfileMerger;

  private config: NarrativeConfig;
  private sessionId: string = 'default';
  private userId: string = 'default';
  private started = false;

  /**
   * @param config  叙事配置（可选）
   * @param dbPath  SQLite 数据库路径（可选，仅 NarrativeStore 使用）
   * @param store   可注入的存储实现（测试用 MemoryStore，生产用 NarrativeStore）
   */
  constructor(config?: Partial<NarrativeConfig>, dbPath?: string, store?: INarrativeStore) {
    this.config = { ...DEFAULT_NARRATIVE_CONFIG, ...config };
    this.store = store || new NarrativeStore(dbPath);
    this.quantify = new QuantifyScheduler(this.store, this.config);
    this.reflect = new ReflectScheduler(this.store, this.config);
    this.profileMerger = new ProfileMerger(this.store, this.config);
  }

  /** 更新配置 */
  updateConfig(config: Partial<NarrativeConfig>): void {
    this.config = { ...this.config, ...config };
  }

  /** 设置当前会话/用户上下文 */
  setContext(sessionId: string, userId: string = 'default'): void {
    this.sessionId = sessionId;
    this.userId = userId;
  }

  /**
   * B1: L1 原始观察写入
   *
   * 由 Agent Expresser 回调调用，将每次 Agent 输出写入原始观察层。
   * B7: 所有操作被 try-catch 包裹，失败不影响主管线。
   */
  record(params: CreateObservationParams): void {
    try {
      const significant =
        Math.abs(params.intensity - 0.5) >=
        (this.config.significantThreshold - 0.5);

      this.store.insertObservation({
        ...params,
        userId: this.userId,
        significant,
      });
    } catch (error) {
      // B7: 叙事子系统故障不影响主管线 — 静默吞掉错误
      console.error('[Narrative] record failed:', error);
    }
  }

  /**
   * 从 ExpressionResult 记录 L1 观察（由 Expresser 回调调用）
   */
  recordFromExpression(result: ExpressionResult): void {
    const { text, emotionUpdate } = result;
    this.record({
      sessionId: this.sessionId,
      content: text,
      emotion: emotionUpdate.emotion || 'neutral',
      intensity: emotionUpdate.intensity ?? 0.5,
      valence: emotionUpdate.valence ?? 0.5,
      arousal: emotionUpdate.arousal ?? 0.5,
      dominance: emotionUpdate.dominance ?? 0.5,
    });
  }

  /**
   * 启动定时器
   * - L2 量化指标计算器开始周期性运行
   * - L3 反思调度器开始周期性运行
   */
  start(userId: string = 'default'): void {
    if (this.started) return;
    this.started = true;
    this.userId = userId;

    // B2: 启动 L2 定时量化计算
    try {
      this.quantify.start(userId);
    } catch (error) {
      // B7: 异常隔离
      console.error('[Narrative] quantify start failed:', error);
    }

    // B3/B6: 启动 L3 定时反思
    try {
      this.reflect.start(userId);
    } catch (error) {
      // B7: 异常隔离
      console.error('[Narrative] reflect start failed:', error);
    }
  }

  /** 停止所有定时器 */
  stop(): void {
    try {
      this.quantify.stop();
      this.reflect.stop();
    } catch {
      // B7: 异常隔离
    }
    this.started = false;
  }

  /**
   * B5: 删除用户所有叙事数据（遗忘权）
   */
  deleteUserData(userId: string = 'default'): void {
    try {
      this.store.deleteUserData(userId);
    } catch (error) {
      // B7: 异常隔离
      console.error('[Narrative] deleteUserData failed:', error);
    }
  }

  /**
   * B4: 获取 L4 动态画像（不可直接用于在线对话决策）
   *
   * 调用方必须理解：此画像仅用于离线分析/UI 展示，
   * 不可注入到 LLM 在线对话上下文中。
   */
  getProfile(userId: string = 'default'): DynamicProfile | null {
    try {
      return this.store.getLatestProfile(userId);
    } catch (error) {
      // B7: 异常隔离
      console.error('[Narrative] getProfile failed:', error);
      return null;
    }
  }

  /**
   * 手动触发 L4 画像合成
   */
  mergeProfile(userId: string = 'default'): DynamicProfile | null {
    try {
      return this.profileMerger.merge(userId);
    } catch (error) {
      // B7: 异常隔离
      console.error('[Narrative] mergeProfile failed:', error);
      return null;
    }
  }

  /**
   * 手动触发 L2 量化计算
   */
  runQuantify(userId: string = 'default'): void {
    try {
      this.quantify.run(userId);
    } catch (error) {
      // B7: 异常隔离
      console.error('[Narrative] runQuantify failed:', error);
    }
  }

  /**
   * 手动触发 L3 反思
   */
  async runReflect(userId: string = 'default'): Promise<ReflectiveHypothesis[]> {
    try {
      return await this.reflect.run(userId);
    } catch (error) {
      // B7: 异常隔离
      console.error('[Narrative] runReflect failed:', error);
      return [];
    }
  }

  /** 获取统计信息 */
  getStats() {
    try {
      return this.store.getStats();
    } catch {
      return null;
    }
  }

  /** 清理过期数据 */
  cleanOldData(): number {
    try {
      return this.store.cleanOldObservations(this.config.retentionDays);
    } catch {
      return 0;
    }
  }

  /** 释放资源 */
  destroy(): void {
    this.stop();
    try {
      this.store.close();
    } catch {
      // B7: 异常隔离
    }
  }
}
