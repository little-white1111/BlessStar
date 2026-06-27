/**
 * feedbackCollector — T2 反馈层（Rejection Sampling）
 *
 * 对应「三层翻译确定性增强管线」中的 T2 层。
 * 当用户修正 AI 的输出时，收集修正记录用于离线范式迭代。
 *
 * 工作流程：
 * 1. 用户对 AI 的输出不满意 → 提供修正
 * 2. feedbackCollector 记录原始输出 + 修正内容
 * 3. 离线阶段：分析高频修正模式 → 更新 Business Paradigm 模板
 * 4. 新范式上线后同类修正需求减少
 *
 * 存储格式：JSONL（每行一条修正记录），存储在本地文件系统。
 * MVP 阶段：仅收集到内存 + localStorage，不涉及云端同步。
 */

// ── 类型 ──────────────────────────────────────────────────────────────

export interface FeedbackRecord {
  /** 唯一 ID */
  id: string
  /** 触发时间戳 */
  timestamp: number
  /** 用户意图 / Skill 名称 */
  intent: string
  /** 原始 AI 输出 */
  originalOutput: string
  /** 用户修正内容 */
  userCorrection: string
  /** 涉及的工具 */
  toolName?: string
  /** 反馈类型 */
  type: FeedbackType
  /** 用户评分（可选） */
  rating?: 1 | 2 | 3 | 4 | 5
}

export type FeedbackType = 
  | 'translation_incorrect'   // 翻译错误：AI 对工具结果描述不准确
  | 'format_wrong'            // 格式错误：范式模板不匹配
  | 'missing_info'            // 信息缺失：应包含更多细节
  | 'gate_missed'             // Gate 遗漏：未检查条件
  | 'tool_wrong'              // 工具选错：应使用不同工具
  | 'other'                   // 其他

export interface FeedbackStats {
  total: number
  byType: Record<FeedbackType, number>
  topIntents: Array<{ intent: string; count: number }>
  lastUpdated: number
}

// ── 收集器 ───────────────────────────────────────────────────────────

const STORAGE_KEY = 'blessstar_feedback_records'
const MAX_RECORDS = 1000

export class FeedbackCollector {
  private records: FeedbackRecord[] = []
  private loaded = false

  /** 加载已有记录 */
  private ensureLoaded(): void {
    if (this.loaded) return
    try {
      const raw = localStorage.getItem(STORAGE_KEY)
      if (raw) {
        this.records = JSON.parse(raw)
      }
    } catch {
      this.records = []
    }
    this.loaded = true
  }

  /** 保存到本地存储 */
  private persist(): void {
    try {
      // 仅保留最近 MAX_RECORDS 条
      if (this.records.length > MAX_RECORDS) {
        this.records = this.records.slice(-MAX_RECORDS)
      }
      localStorage.setItem(STORAGE_KEY, JSON.stringify(this.records))
    } catch {
      console.warn('[FeedbackCollector] 持久化失败，存储可能已满')
    }
  }

  /**
   * 记录一条用户反馈
   */
  record(feedback: Omit<FeedbackRecord, 'id' | 'timestamp'>): FeedbackRecord {
    this.ensureLoaded()

    const record: FeedbackRecord = {
      ...feedback,
      id: `fb_${Date.now()}_${Math.random().toString(36).slice(2, 6)}`,
      timestamp: Date.now(),
    }

    this.records.push(record)
    this.persist()
    return record
  }

  /**
   * 获取所有记录
   */
  getAll(): FeedbackRecord[] {
    this.ensureLoaded()
    return [...this.records]
  }

  /**
   * 获取反馈统计
   */
  getStats(): FeedbackStats {
    this.ensureLoaded()

    const byType: Record<string, number> = {
      translation_incorrect: 0,
      format_wrong: 0,
      missing_info: 0,
      gate_missed: 0,
      tool_wrong: 0,
      other: 0,
    }

    const intentCount: Record<string, number> = {}

    for (const r of this.records) {
      byType[r.type] = (byType[r.type] || 0) + 1
      intentCount[r.intent] = (intentCount[r.intent] || 0) + 1
    }

    const topIntents = Object.entries(intentCount)
      .sort(([, a], [, b]) => b - a)
      .slice(0, 10)
      .map(([intent, count]) => ({ intent, count }))

    return {
      total: this.records.length,
      byType: byType as Record<FeedbackType, number>,
      topIntents,
      lastUpdated: this.records.length > 0 ? this.records[this.records.length - 1].timestamp : Date.now(),
    }
  }

  /**
   * 清除所有记录
   */
  clear(): void {
    this.records = []
    this.persist()
  }

  /**
   * 导出为 JSONL 格式（用于离线训练/分析）
   */
  exportJSONL(): string {
    this.ensureLoaded()
    return this.records.map((r) => JSON.stringify(r)).join('\n')
  }

  /**
   * 从 JSONL 导入
   */
  importJSONL(jsonl: string): number {
    const lines = jsonl.trim().split('\n')
    let count = 0
    for (const line of lines) {
      try {
        const record = JSON.parse(line) as FeedbackRecord
        if (record.id && record.intent !== undefined) {
          this.records.push(record)
          count++
        }
      } catch { /* skip malformed lines */ }
    }
    this.persist()
    return count
  }
}

// ── 全局单例 ──────────────────────────────────────────────────────────

export const feedbackCollector = new FeedbackCollector()
