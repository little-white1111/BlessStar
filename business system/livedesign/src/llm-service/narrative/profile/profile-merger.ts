/**
 * L4 动态画像合成器
 *
 * 架构不变量：
 *   B3 — 低于 0.4 置信度的假设不写入画像
 *   B4 — L4 动态画像不可直接用于在线对话决策
 *   B6 — 画像携带 version 时间戳
 *
 * 职责：
 *   从 L3 反思假设中筛选高置信度（>= 0.4）的假设，
 *   结合 L2 量化指标中的轨迹属性数据，合并为 L4 动态画像。
 */

import type {
  INarrativeStore,
} from '../storage/sqlite-store';
import type {
  NarrativeConfig,
  DynamicProfile,
  ReflectiveHypothesis,
  TrendDirection,
} from '../types';

export class ProfileMerger {
  private store: INarrativeStore;
  private config: NarrativeConfig;

  constructor(store: INarrativeStore, config: NarrativeConfig) {
    this.store = store;
    this.config = config;
  }

  /**
   * 合成 L4 动态画像
   *
   * @param userId 用户标识
   * @returns 合成后的动态画像
   */
  merge(userId: string = 'default'): DynamicProfile {
    // B3: 仅筛选 confidence >= 0.4 的假设
    const activeHypotheses = this.store.getHypothesesByConfidence(userId, 0.4);

    // B6: 使用最新版本的时间戳
    const version = new Date().toISOString().replace(/[:.]/g, '-');

    // 从量化指标中提取轨迹属性值
    const trajectoryAttributes = this.extractTrajectoryAttributes(userId);

    // 计算各属性的趋势方向
    const attributeTrends = this.calculateTrends(trajectoryAttributes);

    const profile: DynamicProfile = {
      version,
      userId,
      hypotheses: activeHypotheses,
      trajectoryAttributes,
      mergedAt: Date.now(),
      attributeTrends,
    };

    // 保存快照
    this.store.saveProfileSnapshot(profile);

    return profile;
  }

  /**
   * 从 L2 量化指标中提取成长轨迹属性
   * 使用 emotional_trajectory_ma 指标作为轨迹数据
   */
  private extractTrajectoryAttributes(userId: string): Record<string, number[]> {
    const attributes: Record<string, number[]> = {};
    const defaultAttrs = this.config.trajectoryAttributes;

    // 为每个配置的轨迹属性初始化数组
    for (const attr of defaultAttrs) {
      attributes[attr] = [];
    }

    // 从量化指标中获取轨迹数据
    const metrics = this.store.getRecentMetrics(userId, 'emotional_trajectory_ma', 200);
    for (const metric of metrics) {
      const attrName = metric.metricName.replace('_avg', '');
      // 检查是否属于配置的轨迹属性
      if (defaultAttrs.includes(attrName)) {
        attributes[attrName].push(metric.value);
      }
    }

    // 如果没有量化数据，用默认值占位
    for (const attr of defaultAttrs) {
      if (attributes[attr].length === 0) {
        attributes[attr] = [0.5];
      }
    }

    return attributes;
  }

  /**
   * 计算轨迹趋势方向
   * 比较最近一段数据的均值与前一段的均值
   */
  private calculateTrends(
    attributes: Record<string, number[]>,
  ): Record<string, TrendDirection> {
    const trends: Record<string, TrendDirection> = {};

    for (const [attr, values] of Object.entries(attributes)) {
      if (values.length < 4) {
        trends[attr] = 'stable';
        continue;
      }

      const half = Math.floor(values.length / 2);
      const recentAvg = values.slice(0, half).reduce((a, b) => a + b, 0) / half;
      const olderAvg = values.slice(half).reduce((a, b) => a + b, 0) / (values.length - half);

      const sensitivity = this.config.patternSensitivity || 0.3;
      const diff = recentAvg - olderAvg;

      if (diff > sensitivity) {
        trends[attr] = 'rising';
      } else if (diff < -sensitivity) {
        trends[attr] = 'falling';
      } else {
        trends[attr] = 'stable';
      }
    }

    return trends;
  }
}
