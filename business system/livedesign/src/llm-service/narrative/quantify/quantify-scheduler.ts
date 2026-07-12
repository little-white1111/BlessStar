/**
 * L2 量化指标计算器
 *
 * 架构不变量 B2: L2 量化指标由程序自动计算，不得引入 LLM
 *
 * 职责：
 *   1. 情绪轨迹滑动平均 — 计算 VAD 各维度的滑动平均值
 *   2. 主题密度统计 — 统计观察文本中的关键词密度
 *   3. 互动模式分析 — 分析用户交互的频率和节奏模式
 *
 * 由 setInterval 定时触发，默认每 6 小时运行一次
 */

import type { INarrativeStore } from '../storage/sqlite-store';
import type { NarrativeConfig, QuantitativeMetric, MetricType } from '../types';

/** VAD 滑动窗口结果 */
export interface TrajectoryMA {
  valenceAvg: number;
  arousalAvg: number;
  dominanceAvg: number;
  windowSize: number;
}

/** 主题密度 */
export interface ThemeDensity {
  themes: Record<string, number>;
  total: number;
}

/** 互动模式 */
export interface InteractionPattern {
  messagesPerDay: number;
  avgInterval: number; // 平均间隔（分钟）
  peakHour: number; // 活跃高峰小时
}

export class QuantifyScheduler {
  private timer: ReturnType<typeof setInterval> | null = null;
  private intervalMs: number;
  private store: INarrativeStore;
  private config: NarrativeConfig;

  /**
   * @param store     叙事存储层
   * @param config    叙事配置
   * @param intervalMs 执行间隔（默认 6 小时）
   */
  constructor(
    store: INarrativeStore,
    config: NarrativeConfig,
    intervalMs: number = 6 * 60 * 60 * 1000,
  ) {
    this.store = store;
    this.config = config;
    this.intervalMs = intervalMs;
  }

  /** 启动定时计算 */
  start(userId: string = 'default'): void {
    if (this.timer) return;
    // 先立即执行一次
    this.run(userId);
    this.timer = setInterval(() => {
      this.run(userId);
    }, this.intervalMs);
  }

  /** 停止定时器 */
  stop(): void {
    if (this.timer) {
      clearInterval(this.timer);
      this.timer = null;
    }
  }

  /** 手动触发一次计算 */
  run(userId: string = 'default'): void {
    const now = Date.now();
    const periodStart = now - this.intervalMs;

    // B2: 以下计算全部为程序自动计算，不涉及 LLM

    // 1. 计算情绪轨迹滑动平均
    const ma = this.calculateTrajectoryMA(userId);
    this.store.insertMetric({
      userId,
      metricType: 'emotional_trajectory_ma',
      metricName: 'valence_avg',
      value: ma.valenceAvg,
      windowSize: ma.windowSize,
      periodStart,
      periodEnd: now,
      createdAt: now,
    });
    this.store.insertMetric({
      userId,
      metricType: 'emotional_trajectory_ma',
      metricName: 'arousal_avg',
      value: ma.arousalAvg,
      windowSize: ma.windowSize,
      periodStart,
      periodEnd: now,
      createdAt: now,
    });
    this.store.insertMetric({
      userId,
      metricType: 'emotional_trajectory_ma',
      metricName: 'dominance_avg',
      value: ma.dominanceAvg,
      windowSize: ma.windowSize,
      periodStart,
      periodEnd: now,
      createdAt: now,
    });

    // 2. 计算主题密度
    const density = this.calculateThemeDensity(userId);
    for (const [theme, count] of Object.entries(density.themes)) {
      this.store.insertMetric({
        userId,
        metricType: 'theme_density',
        metricName: theme,
        value: count / Math.max(density.total, 1),
        windowSize: density.total,
        periodStart,
        periodEnd: now,
        createdAt: now,
      });
    }

    // 3. 计算互动模式
    const pattern = this.calculateInteractionPattern(userId);
    this.store.insertMetric({
      userId,
      metricType: 'interaction_pattern',
      metricName: 'messages_per_day',
      value: pattern.messagesPerDay,
      windowSize: 0,
      periodStart,
      periodEnd: now,
      createdAt: now,
    });
    this.store.insertMetric({
      userId,
      metricType: 'interaction_pattern',
      metricName: 'avg_interval_min',
      value: pattern.avgInterval,
      windowSize: 0,
      periodStart,
      periodEnd: now,
      createdAt: now,
    });
    this.store.insertMetric({
      userId,
      metricType: 'interaction_pattern',
      metricName: 'peak_hour',
      value: pattern.peakHour,
      windowSize: 0,
      periodStart,
      periodEnd: now,
      createdAt: now,
    });
  }

  /**
   * 计算情绪轨迹滑动平均（B2）
   * 使用最近 N 条观察记录的 VAD 均值
   */
  calculateTrajectoryMA(userId: string, windowSize: number = 20): TrajectoryMA {
    const observations = this.store.getRecentObservations(userId, windowSize);
    if (observations.length === 0) {
      return { valenceAvg: 0.5, arousalAvg: 0.5, dominanceAvg: 0.5, windowSize: 0 };
    }

    const sum = observations.reduce(
      (acc, obs) => ({
        valence: acc.valence + obs.valence,
        arousal: acc.arousal + obs.arousal,
        dominance: acc.dominance + obs.dominance,
      }),
      { valence: 0, arousal: 0, dominance: 0 },
    );

    const count = observations.length;
    return {
      valenceAvg: sum.valence / count,
      arousalAvg: sum.arousal / count,
      dominanceAvg: sum.dominance / count,
      windowSize: count,
    };
  }

  /**
   * 计算主题密度（B2）
   * 简单实现：统计情感标签/关键字的出现频率
   */
  calculateThemeDensity(userId: string, limit: number = 100): ThemeDensity {
    const observations = this.store.getRecentObservations(userId, limit);
    const themes: Record<string, number> = {};

    for (const obs of observations) {
      const emotion = obs.emotion || 'neutral';
      themes[emotion] = (themes[emotion] || 0) + 1;
    }

    return { themes, total: observations.length };
  }

  /**
   * 计算互动模式（B2）
   * 分析消息频率、间隔和活跃时段
   */
  calculateInteractionPattern(userId: string, limit: number = 200): InteractionPattern {
    const observations = this.store.getRecentObservations(userId, limit);

    if (observations.length < 2) {
      return { messagesPerDay: 0, avgInterval: 0, peakHour: 0 };
    }

    // 计算平均每天消息数
    const firstTime = observations[observations.length - 1].createdAt;
    const lastTime = observations[0].createdAt;
    const daysDiff = Math.max((lastTime - firstTime) / (24 * 60 * 60 * 1000), 1);
    const messagesPerDay = observations.length / daysDiff;

    // 计算平均间隔（分钟）
    let totalInterval = 0;
    let intervalCount = 0;
    for (let i = 0; i < observations.length - 1; i++) {
      const interval = observations[i].createdAt - observations[i + 1].createdAt;
      if (interval > 0 && interval < 24 * 60 * 60 * 1000) {
        totalInterval += interval;
        intervalCount++;
      }
    }
    const avgInterval =
      intervalCount > 0
        ? totalInterval / intervalCount / (60 * 1000)
        : 0;

    // 计算活跃高峰小时
    const hourCounts: Record<number, number> = {};
    for (const obs of observations) {
      const hour = new Date(obs.createdAt).getHours();
      hourCounts[hour] = (hourCounts[hour] || 0) + 1;
    }
    let peakHour = 0;
    let maxCount = 0;
    for (const [hour, count] of Object.entries(hourCounts)) {
      if (count > maxCount) {
        maxCount = count;
        peakHour = parseInt(hour, 10);
      }
    }

    return { messagesPerDay, avgInterval, peakHour };
  }
}
