/**
 * L3 反思调度器
 *
 * 架构不变量：
 *   B3 — 反思输出必须标注 confidence，低于 0.4 的不写入画像
 *   B6 — 反思每轮输出必须携带 version 时间戳，支持多轮回溯
 *
 * 该调度器每周运行一次，读取 L1 原始观察记录，调用 LLM 生成
 * 心理分析假设，然后存储到 L3 反思假设表。
 *
 * 注意：LLM 调用由外部注入（reflectCallback），以保持模块的可测试性。
 * 实际 LLM 调用在 NarrativeSystem 层完成。
 */

import type { INarrativeStore } from '../storage/sqlite-store';
import type { NarrativeConfig, ReflectiveHypothesis } from '../types';

/** LLM 反思回调 — 接收观察文本，返回 JSON 解析后的假设列表 */
export type ReflectCallback = (
  observationsText: string,
  config: NarrativeConfig,
  previousHypothesesText: string,
) => Promise<ReflectiveHypothesis[]>;

export class ReflectScheduler {
  private timer: ReturnType<typeof setInterval> | null = null;
  private intervalMs: number;
  private store: INarrativeStore;
  private config: NarrativeConfig;
  private reflectCallback: ReflectCallback | null = null;

  /**
   * @param store      叙事存储层
   * @param config     叙事配置
   * @param intervalMs 执行间隔（默认 7 天）
   */
  constructor(
    store: INarrativeStore,
    config: NarrativeConfig,
    intervalMs: number = 7 * 24 * 60 * 60 * 1000,
  ) {
    this.store = store;
    this.config = config;
    this.intervalMs = intervalMs;
  }

  /** 设置 LLM 反思回调 */
  setReflectCallback(callback: ReflectCallback): void {
    this.reflectCallback = callback;
  }

  /** 启动定时反思 */
  start(userId: string = 'default'): void {
    if (this.timer) return;
    // 启动后等待下一个周期再执行，避免启动时立即消耗 LLM 配额
    this.timer = setInterval(() => {
      this.run(userId).catch(() => {
        // B7: 叙事子系统故障不影响主管线 — 静默吞掉错误
      });
    }, this.intervalMs);
  }

  /** 停止定时器 */
  stop(): void {
    if (this.timer) {
      clearInterval(this.timer);
      this.timer = null;
    }
  }

  /**
   * 手动触发一次反思流程
   * @returns 生成的假设列表，如果观察数不足则返回空数组
   */
  async run(userId: string = 'default'): Promise<ReflectiveHypothesis[]> {
    const totalObservations = this.store.countObservations(userId);
    if (totalObservations < this.config.minObservations) {
      return []; // 数据不足，跳过本轮分析
    }

    // 获取最近的观察记录
    const recentObservations = this.store.getRecentObservations(userId, this.config.minObservations);
    const observationsText = recentObservations
      .map((obs, i) => `[${i}] ${obs.content}`)
      .join('\n');

    // 获取之前的假设作为上下文
    const previousHypotheses = this.store.getHypothesesByConfidence(userId, 0);
    const previousHypothesesText = previousHypotheses.length > 0
      ? previousHypotheses.map((h) => `- [${h.domain}] ${h.hypothesis} (confidence: ${h.confidence})`).join('\n')
      : '';

    // 如果没有注册回调，返回空（测试模式下可用）
    if (!this.reflectCallback) {
      return [];
    }

    // B6: 生成版本时间戳
    const version = new Date().toISOString().replace(/[:.]/g, '-');

    // 调用 LLM 反思
    const hypotheses = await this.reflectCallback(
      observationsText,
      this.config,
      previousHypothesesText,
    );

    // 写入存储，附带版本号
    const savedIds: ReflectiveHypothesis[] = [];
    for (const h of hypotheses) {
      const id = this.store.insertHypothesis({
        userId,
        version,
        domain: h.domain,
        hypothesis: h.hypothesis,
        evidence: h.evidence,
        confidence: h.confidence,
        observationRefs: h.observationRefs,
        createdAt: Date.now(),
      });
      savedIds.push({ ...h, id, userId, version, createdAt: Date.now() });
    }

    return savedIds;
  }
}
