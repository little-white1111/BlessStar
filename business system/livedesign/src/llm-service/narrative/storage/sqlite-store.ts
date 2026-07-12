/**
 * 叙事系统 SQLite 存储层
 *
 * 架构不变量：
 *   B1 — L1 原始观察写入
 *   B5 — 用户叙事档案必须支持完整删除/重置（遗忘权）
 *   B6 — 支持 version 时间戳回溯
 *
 * 使用 better-sqlite3（已在 package.json 依赖中）
 * 与 dialog-store.ts 使用相同的数据库模式
 *
 * 提供 NarrativeStore 和 MemoryStore 两个实现：
 *   - NarrativeStore: 基于 better-sqlite3 的持久化实现（生产环境）
 *   - MemoryStore: 基于内存数组的轻量实现（单元测试）
 */

import Database from 'better-sqlite3';
import * as path from 'path';
import * as fs from 'fs';
import type {
  RawObservation,
  QuantitativeMetric,
  ReflectiveHypothesis,
  DynamicProfile,
  CreateObservationParams,
  MetricType,
} from '../types';

/** 默认数据库路径 */
const DEFAULT_DB_DIR = path.join(process.cwd(), 'data');

/**
 * 叙事存储接口 — 定义了 L1~L4 数据的 CRUD 操作
 * NarrativeStore 和 MemoryStore 都实现此接口
 */
export interface INarrativeStore {
  insertObservation(params: CreateObservationParams & { userId: string; significant: boolean }): number;
  getRecentObservations(userId: string, limit?: number, offset?: number): RawObservation[];
  getObservationsByTimeRange(userId: string, startTime: number, endTime: number): RawObservation[];
  countObservations(userId: string): number;
  insertMetric(metric: Omit<QuantitativeMetric, 'id'>): number;
  getRecentMetrics(userId: string, metricType?: MetricType, limit?: number): QuantitativeMetric[];
  insertHypothesis(hypothesis: Omit<ReflectiveHypothesis, 'id'>): number;
  getHypothesesByVersion(userId: string, version: string): ReflectiveHypothesis[];
  getAllVersions(userId: string): string[];
  getHypothesesByConfidence(userId: string, minConfidence?: number): ReflectiveHypothesis[];
  saveProfileSnapshot(profile: DynamicProfile): number;
  getLatestProfile(userId: string): DynamicProfile | null;
  deleteUserData(userId: string): void;
  cleanOldObservations(retentionDays: number): number;
  getStats(): { totalObservations: number; totalMetrics: number; totalHypotheses: number; totalSnapshots: number };
  close(): void;
}

// ==================== SQLite 实现（生产环境） ====================

export class NarrativeStore implements INarrativeStore {
  private db: Database.Database;

  /**
   * @param dbPath SQLite 数据库文件路径（默认: ./data/narrative.db）
   * @param db     可选的 Database 实例（用于 DI/测试注入）
   */
  constructor(dbPath?: string, db?: Database.Database) {
    if (db) {
      this.db = db;
    } else {
      const resolvedPath = dbPath || process.env.NARRATIVE_DB_PATH || path.join(DEFAULT_DB_DIR, 'narrative.db');
      const dir = path.dirname(resolvedPath);
      if (!fs.existsSync(dir)) {
        fs.mkdirSync(dir, { recursive: true });
      }
      this.db = new Database(resolvedPath);
      this.db.pragma('journal_mode = WAL');
      this.db.pragma('foreign_keys = ON');
    }
    this.initializeSchema();
  }

  /** 初始化表结构 */
  private initializeSchema(): void {
    this.db.exec(`
      CREATE TABLE IF NOT EXISTS observations (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        session_id TEXT NOT NULL,
        user_id TEXT NOT NULL DEFAULT 'default',
        content TEXT NOT NULL,
        emotion TEXT NOT NULL DEFAULT 'neutral',
        intensity REAL NOT NULL DEFAULT 0.5,
        valence REAL NOT NULL DEFAULT 0.5,
        arousal REAL NOT NULL DEFAULT 0.5,
        dominance REAL NOT NULL DEFAULT 0.5,
        significant_mark INTEGER NOT NULL DEFAULT 0,
        created_at INTEGER NOT NULL
      );
      CREATE TABLE IF NOT EXISTS quantitative_metrics (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id TEXT NOT NULL DEFAULT 'default',
        metric_type TEXT NOT NULL,
        metric_name TEXT NOT NULL,
        value REAL NOT NULL,
        window_size INTEGER NOT NULL DEFAULT 0,
        period_start INTEGER NOT NULL,
        period_end INTEGER NOT NULL,
        created_at INTEGER NOT NULL
      );
      CREATE TABLE IF NOT EXISTS reflective_hypotheses (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id TEXT NOT NULL DEFAULT 'default',
        version TEXT NOT NULL,
        domain TEXT NOT NULL,
        hypothesis TEXT NOT NULL,
        evidence TEXT DEFAULT '',
        confidence REAL NOT NULL,
        observation_refs TEXT NOT NULL DEFAULT '[]',
        created_at INTEGER NOT NULL
      );
      CREATE TABLE IF NOT EXISTS profile_snapshots (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id TEXT NOT NULL DEFAULT 'default',
        version TEXT NOT NULL,
        profile_json TEXT NOT NULL,
        created_at INTEGER NOT NULL
      );
      CREATE INDEX IF NOT EXISTS idx_observations_user_id ON observations(user_id);
      CREATE INDEX IF NOT EXISTS idx_observations_created_at ON observations(created_at);
      CREATE INDEX IF NOT EXISTS idx_observations_significant ON observations(significant_mark);
      CREATE INDEX IF NOT EXISTS idx_quantitative_user_id ON quantitative_metrics(user_id);
      CREATE INDEX IF NOT EXISTS idx_quantitative_type ON quantitative_metrics(metric_type);
      CREATE INDEX IF NOT EXISTS idx_hypotheses_user_id ON reflective_hypotheses(user_id);
      CREATE INDEX IF NOT EXISTS idx_hypotheses_version ON reflective_hypotheses(version);
      CREATE INDEX IF NOT EXISTS idx_profile_user_id ON profile_snapshots(user_id);
    `);
  }

  // ==================== L1: 原始观察 CRUD ====================

  insertObservation(
    params: CreateObservationParams & { userId: string; significant: boolean },
  ): number {
    const stmt = this.db.prepare(`
      INSERT INTO observations (session_id, user_id, content, emotion, intensity, valence, arousal, dominance, significant_mark, created_at)
      VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    `);
    const result = stmt.run(
      params.sessionId, params.userId, params.content, params.emotion,
      params.intensity, params.valence, params.arousal, params.dominance,
      params.significant ? 1 : 0, Date.now(),
    );
    return result.lastInsertRowid as number;
  }

  getRecentObservations(userId: string, limit: number = 100, offset: number = 0): RawObservation[] {
    const rows = this.db
      .prepare(`SELECT id, session_id AS sessionId, user_id AS userId, content, emotion, intensity, valence, arousal, dominance, significant_mark AS significant, created_at AS createdAt FROM observations WHERE user_id = ? ORDER BY created_at DESC LIMIT ? OFFSET ?`)
      .all(userId, limit, offset) as Array<Record<string, unknown>>;
    return rows.map((r) => ({ ...r, significant: (r as unknown as { significant: number }).significant === 1 })) as unknown as RawObservation[];
  }

  getObservationsByTimeRange(userId: string, startTime: number, endTime: number): RawObservation[] {
    const rows = this.db
      .prepare(`SELECT id, session_id AS sessionId, user_id AS userId, content, emotion, intensity, valence, arousal, dominance, significant_mark AS significant, created_at AS createdAt FROM observations WHERE user_id = ? AND created_at >= ? AND created_at <= ? ORDER BY created_at ASC`)
      .all(userId, startTime, endTime) as Array<Record<string, unknown>>;
    return rows.map((r) => ({ ...r, significant: (r as unknown as { significant: number }).significant === 1 })) as unknown as RawObservation[];
  }

  countObservations(userId: string): number {
    const row = this.db.prepare('SELECT COUNT(*) AS count FROM observations WHERE user_id = ?').get(userId) as { count: number };
    return row.count;
  }

  // ==================== L2: 量化指标 CRUD ====================

  insertMetric(metric: Omit<QuantitativeMetric, 'id'>): number {
    const stmt = this.db.prepare(`INSERT INTO quantitative_metrics (user_id, metric_type, metric_name, value, window_size, period_start, period_end, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?)`);
    const result = stmt.run(metric.userId, metric.metricType, metric.metricName, metric.value, metric.windowSize, metric.periodStart, metric.periodEnd, metric.createdAt);
    return result.lastInsertRowid as number;
  }

  getRecentMetrics(userId: string, metricType?: MetricType, limit: number = 50): QuantitativeMetric[] {
    let query = `SELECT id, user_id AS userId, metric_type AS metricType, metric_name AS metricName, value, window_size AS windowSize, period_start AS periodStart, period_end AS periodEnd, created_at AS createdAt FROM quantitative_metrics WHERE user_id = ?`;
    const params: unknown[] = [userId];
    if (metricType) { query += ' AND metric_type = ?'; params.push(metricType); }
    query += ' ORDER BY created_at DESC LIMIT ?';
    params.push(limit);
    return this.db.prepare(query).all(...params) as QuantitativeMetric[];
  }

  // ==================== L3: 反思假设 CRUD ====================

  insertHypothesis(hypothesis: Omit<ReflectiveHypothesis, 'id'>): number {
    const stmt = this.db.prepare(`INSERT INTO reflective_hypotheses (user_id, version, domain, hypothesis, evidence, confidence, observation_refs, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?)`);
    const result = stmt.run(hypothesis.userId, hypothesis.version, hypothesis.domain, hypothesis.hypothesis, hypothesis.evidence, hypothesis.confidence, JSON.stringify(hypothesis.observationRefs), hypothesis.createdAt);
    return result.lastInsertRowid as number;
  }

  getHypothesesByVersion(userId: string, version: string): ReflectiveHypothesis[] {
    const rows = this.db
      .prepare(`SELECT id, user_id AS userId, version, domain, hypothesis, evidence, confidence, observation_refs AS observationRefsStr, created_at AS createdAt FROM reflective_hypotheses WHERE user_id = ? AND version = ? ORDER BY created_at ASC`)
      .all(userId, version) as Array<Record<string, unknown>>;
    return rows.map((r) => ({ ...r, observationRefs: JSON.parse((r as unknown as { observationRefsStr: string }).observationRefsStr) })) as unknown as ReflectiveHypothesis[];
  }

  getAllVersions(userId: string): string[] {
    const rows = this.db.prepare('SELECT DISTINCT version FROM reflective_hypotheses WHERE user_id = ? ORDER BY version DESC').all(userId) as Array<{ version: string }>;
    return rows.map((r) => r.version);
  }

  getHypothesesByConfidence(userId: string, minConfidence: number = 0): ReflectiveHypothesis[] {
    const rows = this.db
      .prepare(`SELECT id, user_id AS userId, version, domain, hypothesis, evidence, confidence, observation_refs AS observationRefsStr, created_at AS createdAt FROM reflective_hypotheses WHERE user_id = ? AND confidence >= ? ORDER BY created_at DESC`)
      .all(userId, minConfidence) as Array<Record<string, unknown>>;
    return rows.map((r) => ({ ...r, observationRefs: JSON.parse((r as unknown as { observationRefsStr: string }).observationRefsStr) })) as unknown as ReflectiveHypothesis[];
  }

  // ==================== L4: 动态画像 ====================

  saveProfileSnapshot(profile: DynamicProfile): number {
    const stmt = this.db.prepare(`INSERT INTO profile_snapshots (user_id, version, profile_json, created_at) VALUES (?, ?, ?, ?)`);
    const result = stmt.run(profile.userId, profile.version, JSON.stringify(profile), profile.mergedAt);
    return result.lastInsertRowid as number;
  }

  getLatestProfile(userId: string): DynamicProfile | null {
    const row = this.db.prepare(`SELECT profile_json FROM profile_snapshots WHERE user_id = ? ORDER BY created_at DESC LIMIT 1`).get(userId) as { profile_json: string } | undefined;
    if (!row) return null;
    return JSON.parse(row.profile_json) as DynamicProfile;
  }

  // ==================== B5: 遗忘权 ====================

  deleteUserData(userId: string): void {
    this.db.transaction(() => {
      this.db.prepare('DELETE FROM observations WHERE user_id = ?').run(userId);
      this.db.prepare('DELETE FROM quantitative_metrics WHERE user_id = ?').run(userId);
      this.db.prepare('DELETE FROM reflective_hypotheses WHERE user_id = ?').run(userId);
      this.db.prepare('DELETE FROM profile_snapshots WHERE user_id = ?').run(userId);
    })();
  }

  // ==================== 清理 & 统计 ====================

  cleanOldObservations(retentionDays: number): number {
    const cutoff = Date.now() - retentionDays * 24 * 60 * 60 * 1000;
    const result = this.db.prepare('DELETE FROM observations WHERE created_at < ?').run(cutoff);
    return result.changes;
  }

  getStats(): { totalObservations: number; totalMetrics: number; totalHypotheses: number; totalSnapshots: number } {
    return {
      totalObservations: (this.db.prepare('SELECT COUNT(*) AS count FROM observations').get() as { count: number }).count,
      totalMetrics: (this.db.prepare('SELECT COUNT(*) AS count FROM quantitative_metrics').get() as { count: number }).count,
      totalHypotheses: (this.db.prepare('SELECT COUNT(*) AS count FROM reflective_hypotheses').get() as { count: number }).count,
      totalSnapshots: (this.db.prepare('SELECT COUNT(*) AS count FROM profile_snapshots').get() as { count: number }).count,
    };
  }

  close(): void {
    if (this.db && this.db.open) {
      this.db.close();
    }
  }
}

// ==================== 内存实现（单元测试用） ====================

/**
 * 基于内存数组的 INarrativeStore 实现
 * 用于单元测试，无需 SQLite 原生模块
 */
export class MemoryStore implements INarrativeStore {
  private observations: RawObservation[] = [];
  private metrics: QuantitativeMetric[] = [];
  private hypotheses: ReflectiveHypothesis[] = [];
  private profileSnapshots: DynamicProfile[] = [];
  private nextId = 1;

  insertObservation(params: CreateObservationParams & { userId: string; significant: boolean }): number {
    const obs: RawObservation = {
      id: this.nextId++,
      sessionId: params.sessionId,
      userId: params.userId,
      content: params.content,
      emotion: params.emotion,
      intensity: params.intensity,
      valence: params.valence,
      arousal: params.arousal,
      dominance: params.dominance,
      significant: params.significant,
      createdAt: Date.now(),
    };
    this.observations.unshift(obs); // 最新在最前
    return obs.id!;
  }

  getRecentObservations(userId: string, limit: number = 100, offset: number = 0): RawObservation[] {
    return this.observations.filter(o => o.userId === userId).slice(offset, offset + limit);
  }

  getObservationsByTimeRange(userId: string, startTime: number, endTime: number): RawObservation[] {
    return this.observations
      .filter(o => o.userId === userId && o.createdAt >= startTime && o.createdAt <= endTime)
      .sort((a, b) => a.createdAt - b.createdAt);
  }

  countObservations(userId: string): number {
    return this.observations.filter(o => o.userId === userId).length;
  }

  insertMetric(metric: Omit<QuantitativeMetric, 'id'>): number {
    const m: QuantitativeMetric = { ...metric, id: this.nextId++ };
    this.metrics.unshift(m);
    return m.id!;
  }

  getRecentMetrics(userId: string, metricType?: MetricType, limit: number = 50): QuantitativeMetric[] {
    let filtered = this.metrics.filter(m => m.userId === userId);
    if (metricType) filtered = filtered.filter(m => m.metricType === metricType);
    return filtered.slice(0, limit);
  }

  insertHypothesis(hypothesis: Omit<ReflectiveHypothesis, 'id'>): number {
    const h: ReflectiveHypothesis = { ...hypothesis, id: this.nextId++ };
    this.hypotheses.unshift(h);
    return h.id!;
  }

  getHypothesesByVersion(userId: string, version: string): ReflectiveHypothesis[] {
    return this.hypotheses.filter(h => h.userId === userId && h.version === version);
  }

  getAllVersions(userId: string): string[] {
    return [...new Set(this.hypotheses.filter(h => h.userId === userId).map(h => h.version))];
  }

  getHypothesesByConfidence(userId: string, minConfidence: number = 0): ReflectiveHypothesis[] {
    return this.hypotheses.filter(h => h.userId === userId && h.confidence >= minConfidence);
  }

  saveProfileSnapshot(profile: DynamicProfile): number {
    this.profileSnapshots.push(profile);
    return this.nextId++;
  }

  getLatestProfile(userId: string): DynamicProfile | null {
    const userProfiles = this.profileSnapshots.filter(p => p.userId === userId);
    if (userProfiles.length === 0) return null;
    return userProfiles.reduce((latest, p) => p.mergedAt > latest.mergedAt ? p : latest, userProfiles[0]);
  }

  deleteUserData(userId: string): void {
    this.observations = this.observations.filter(o => o.userId !== userId);
    this.metrics = this.metrics.filter(m => m.userId !== userId);
    this.hypotheses = this.hypotheses.filter(h => h.userId !== userId);
    this.profileSnapshots = this.profileSnapshots.filter(p => p.userId !== userId);
  }

  cleanOldObservations(retentionDays: number): number {
    const cutoff = Date.now() - retentionDays * 24 * 60 * 60 * 1000;
    const before = this.observations.length;
    this.observations = this.observations.filter(o => o.createdAt >= cutoff);
    return before - this.observations.length;
  }

  getStats(): { totalObservations: number; totalMetrics: number; totalHypotheses: number; totalSnapshots: number } {
    return {
      totalObservations: this.observations.length,
      totalMetrics: this.metrics.length,
      totalHypotheses: this.hypotheses.length,
      totalSnapshots: this.profileSnapshots.length,
    };
  }

  close(): void {
    // 内存实现无需关闭
  }
}
