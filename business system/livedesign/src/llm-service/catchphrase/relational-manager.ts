/**
 * RelationalCatchphraseManager（关系型口头禅管理器）
 *
 * 架构不变量 D5: Relational 口头禅使用 better-sqlite3 持久化。
 * 架构不变量 D2: 晋升必须经用户确认 — IPC 弹窗确认后才写入活跃表。
 */

import { v4 as uuidv4 } from 'uuid';
import type { RelationalCatchphrase, CatchphraseIntensity } from './types';

/** 内存中的 Relational 口头禅存储（无 better-sqlite3 时的回退） */
interface InMemoryStore {
  catchphrases: RelationalCatchphrase[];
}

export class RelationalCatchphraseManager {
  /** 晋升所需的最小使用次数 */
  private minUsage: number;
  /** 晋升所需的最低平均反馈评分 */
  private minFeedback: number;

  /** SQLite 数据库引用（运行时注入） */
  private db: { run: (sql: string, ...params: unknown[]) => void; all: <T>(sql: string, ...params: unknown[]) => T[]; get: <T>(sql: string, ...params: unknown[]) => T | undefined } | null = null;

  /** 内存存储回退 */
  private memoryStore: InMemoryStore = { catchphrases: [] };

  constructor(minUsage = 5, minFeedback = 0.7) {
    this.minUsage = minUsage;
    this.minFeedback = minFeedback;
  }

  /**
   * 注入 SQLite 数据库实例
   * 如果未注入，使用内存存储回退（仅用于测试/降级场景）。
   */
  setDatabase(db: { run: (sql: string, ...params: unknown[]) => void; all: <T>(sql: string, ...params: unknown[]) => T[]; get: <T>(sql: string, ...params: unknown[]) => T | undefined }): void {
    this.db = db;
    this.ensureTable();
  }

  /** 确保表存在 */
  private ensureTable(): void {
    if (!this.db) return;
    this.db.run(
      `CREATE TABLE IF NOT EXISTS catchphrase_relational (
        id TEXT PRIMARY KEY,
        text TEXT NOT NULL,
        usage_count INTEGER DEFAULT 0,
        total_feedback_score REAL DEFAULT 0,
        feedback_count INTEGER DEFAULT 0,
        average_score REAL DEFAULT 0,
        status TEXT DEFAULT 'candidate',
        scenario_tag TEXT,
        created_at INTEGER NOT NULL,
        last_used_at INTEGER
      )`
    );
  }

  /** 获取最小使用次数配置 */
  getMinUsage(): number {
    return this.minUsage;
  }

  /** 获取最低反馈评分配置 */
  getMinFeedback(): number {
    return this.minFeedback;
  }

  /**
   * 更新配置
   */
  updateConfig(minUsage: number, minFeedback: number): void {
    this.minUsage = minUsage;
    this.minFeedback = minFeedback;
  }

  /**
   * 记录口头禅使用（新增或递增计数）
   * @param text 口头禅文本
   * @param scenarioTag 可选场景标签
   * @returns 更新后的口头禅记录
   */
  recordUsage(text: string, scenarioTag?: string): RelationalCatchphrase {
    // 查找是否已存在
    const existing = this.findByText(text);

    if (existing) {
      return this.incrementUsage(existing.id);
    }

    // 新增
    const now = Date.now();
    const newCatchphrase: RelationalCatchphrase = {
      id: uuidv4(),
      text,
      usageCount: 1,
      totalFeedbackScore: 0,
      feedbackCount: 0,
      averageScore: 0,
      status: 'candidate',
      scenarioTag,
      createdAt: now,
      lastUsedAt: now,
    };

    this.persist(newCatchphrase);
    return { ...newCatchphrase };
  }

  /**
   * 提交用户反馈评分
   * @param id 口头禅 ID
   * @param score 评分 (0.0~1.0)
   * @returns 更新后的口头禅记录，未找到返回 undefined
   */
  submitFeedback(id: string, score: number): RelationalCatchphrase | undefined {
    const existing = this.findById(id);
    if (!existing) return undefined;

    const clampedScore = Math.max(0, Math.min(1, score));
    existing.feedbackCount++;
    existing.totalFeedbackScore += clampedScore;
    existing.averageScore = existing.totalFeedbackScore / existing.feedbackCount;

    this.persist(existing);
    return { ...existing };
  }

  /**
   * 检查是否满足晋升条件
   * 架构不变量 D2: Relational 口头禅晋升必须经用户确认。
   *
   * @param id 口头禅 ID
   * @returns 是否满足晋升条件（达到使用次数和反馈评分门槛）
   */
  checkPromotionEligibility(id: string): boolean {
    const existing = this.findById(id);
    if (!existing) return false;
    if (existing.status === 'active') return false;

    return existing.usageCount >= this.minUsage && existing.averageScore >= this.minFeedback;
  }

  /**
   * 确认晋升（用户确认后调用）
   * 架构不变量 D2: 用户确认后才写入活跃表。
   *
   * @param id 口头禅 ID
   * @returns 晋升后的口头禅记录，未找到返回 undefined
   */
  confirmPromotion(id: string): RelationalCatchphrase | undefined {
    const existing = this.findById(id);
    if (!existing) return undefined;

    existing.status = 'active';
    this.persist(existing);
    return { ...existing };
  }

  /**
   * 拒绝晋升（用户拒绝后调用）
   * 会将使用计数和反馈重置，防止反复提示同一口头禅。
   *
   * @param id 口头禅 ID
   */
  rejectPromotion(id: string): void {
    const existing = this.findById(id);
    if (!existing) return;

    // 重置计数
    existing.usageCount = 0;
    existing.totalFeedbackScore = 0;
    existing.feedbackCount = 0;
    existing.averageScore = 0;
    this.persist(existing);
  }

  /**
   * 获取所有活跃的 Relational 口头禅
   */
  getActive(): RelationalCatchphrase[] {
    const all = this.findAll();
    return all.filter((c) => c.status === 'active');
  }

  /**
   * 获取所有候选的 Relational 口头禅
   */
  getCandidates(): RelationalCatchphrase[] {
    const all = this.findAll();
    return all.filter((c) => c.status === 'candidate');
  }

  /**
   * 根据场景标签查找活跃口头禅
   */
  findByScenarioTag(tag: string): RelationalCatchphrase[] {
    const all = this.findAll();
    return all.filter((c) => c.status === 'active' && c.scenarioTag === tag);
  }

  /**
   * 根据文本查找
   */
  findByText(text: string): RelationalCatchphrase | undefined {
    const all = this.findAll();
    return all.find((c) => c.text === text);
  }

  /**
   * 根据 ID 查找
   */
  findById(id: string): RelationalCatchphrase | undefined {
    const all = this.findAll();
    return all.find((c) => c.id === id);
  }

  /** 获取所有记录 */
  findAll(): RelationalCatchphrase[] {
    if (this.db) {
      try {
        const rows = this.db.all<RelationalCatchphraseRow>('SELECT * FROM catchphrase_relational');
        return rows.map(this.rowToCatchphrase);
      } catch {
        return [];
      }
    }
    return [...this.memoryStore.catchphrases];
  }

  // ==================== 持久化 ====================

  private persist(catchphrase: RelationalCatchphrase): void {
    if (this.db) {
      try {
        this.db.run(
          `INSERT OR REPLACE INTO catchphrase_relational (id, text, usage_count, total_feedback_score, feedback_count, average_score, status, scenario_tag, created_at, last_used_at)
           VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
          catchphrase.id,
          catchphrase.text,
          catchphrase.usageCount,
          catchphrase.totalFeedbackScore,
          catchphrase.feedbackCount,
          catchphrase.averageScore,
          catchphrase.status,
          catchphrase.scenarioTag ?? null,
          catchphrase.createdAt,
          catchphrase.lastUsedAt ?? null,
        );
      } catch {
        // 数据库写入失败，fallback 到内存
        this.persistToMemory(catchphrase);
      }
    } else {
      this.persistToMemory(catchphrase);
    }
  }

  private persistToMemory(catchphrase: RelationalCatchphrase): void {
    const idx = this.memoryStore.catchphrases.findIndex((c) => c.id === catchphrase.id);
    if (idx >= 0) {
      this.memoryStore.catchphrases[idx] = { ...catchphrase };
    } else {
      this.memoryStore.catchphrases.push({ ...catchphrase });
    }
  }

  private incrementUsage(id: string): RelationalCatchphrase {
    const existing = this.findById(id)!;
    existing.usageCount++;
    existing.lastUsedAt = Date.now();
    this.persist(existing);
    return { ...existing };
  }

  private rowToCatchphrase(row: RelationalCatchphraseRow): RelationalCatchphrase {
    return {
      id: row.id,
      text: row.text,
      usageCount: row.usage_count,
      totalFeedbackScore: row.total_feedback_score,
      feedbackCount: row.feedback_count,
      averageScore: row.average_score,
      status: row.status as 'candidate' | 'active',
      scenarioTag: row.scenario_tag ?? undefined,
      createdAt: row.created_at,
      lastUsedAt: row.last_used_at ?? undefined,
    };
  }
}

interface RelationalCatchphraseRow {
  id: string;
  text: string;
  usage_count: number;
  total_feedback_score: number;
  feedback_count: number;
  average_score: number;
  status: string;
  scenario_tag: string | null;
  created_at: number;
  last_used_at: number | null;
}
