/**
 * FrequencyGovernor（频率管制器）
 *
 * 架构不变量 D3: FrequencyGovernor 强制每轮对话最多 1 次口头禅。
 * 基于对话 session ID 的递增计数器，session 结束时 reset。
 *
 * 从 config-schema.yaml:
 *   catchphrase.frequency.max_per_round - 默认 1，范围 [1,3]
 */

/** 默认每轮最大次数 */
const DEFAULT_MAX_PER_ROUND = 1;

export class FrequencyGovernor {
  private maxPerRound: number;
  /** session ID → 当前轮次计数器 */
  private sessionCounters = new Map<string, number>();

  constructor(maxPerRound = DEFAULT_MAX_PER_ROUND) {
    this.maxPerRound = maxPerRound;
  }

  /**
   * 更新配置
   */
  updateConfig(maxPerRound: number): void {
    this.maxPerRound = maxPerRound;
  }

  /**
   * 检查是否允许在当前 session 中输出口头禅
   * 架构不变量 D3: 每轮对话最多 maxPerRound 次。
   *
   * @param sessionId 对话 session ID
   * @returns 是否允许输出
   */
  canOutput(sessionId: string): boolean {
    const count = this.sessionCounters.get(sessionId) ?? 0;
    return count < this.maxPerRound;
  }

  /**
   * 记录一次口头禅输出
   * 架构不变量 D3: 递增计数器。
   *
   * @param sessionId 对话 session ID
   * @returns 更新后的计数器值
   */
  recordOutput(sessionId: string): number {
    const count = (this.sessionCounters.get(sessionId) ?? 0) + 1;
    this.sessionCounters.set(sessionId, count);
    return count;
  }

  /**
   * 重置指定 session 的计数器
   * 架构不变量 D3: session 结束时 reset。
   *
   * @param sessionId 对话 session ID
   */
  resetSession(sessionId: string): void {
    this.sessionCounters.delete(sessionId);
  }

  /**
   * 获取当前 session 的输出次数
   */
  getOutputCount(sessionId: string): number {
    return this.sessionCounters.get(sessionId) ?? 0;
  }

  /**
   * 获取当前最大每轮次数配置
   */
  getMaxPerRound(): number {
    return this.maxPerRound;
  }

  /** 清理所有 session 数据（用于测试/重置） */
  clearAll(): void {
    this.sessionCounters.clear();
  }
}
