/**
 * 演进日志（Evolution Log）
 *
 * 架构不变量 C5: 每次演进必须记录日志 — evolution_log 表含旧值/新值/原因/consistency_check 状态
 *
 * 使用 better-sqlite3 存储演进历史，数据库路径由环境变量 EVOLUTION_DB_PATH 控制。
 */

import Database from 'better-sqlite3';
import path from 'path';
import type { EvolutionLogEntry } from './types';

/**
 * 演进日志存储接口。
 * 架构不变量 C5: 每次演进必须记录日志。
 * 支持多实现：SQLite（生产）和 InMemory（测试）。
 */
export interface EvolutionLogStore {
  write(entry: Omit<EvolutionLogEntry, 'timestamp'>): void;
  query(limit?: number, offset?: number): EvolutionLogEntry[];
  count(): number;
  close(): void;
}

/** 默认数据库路径 */
const DEFAULT_DB_PATH = './data/evolution-log.db';

/** 数据库表名 */
const TABLE_NAME = 'evolution_log';

/** 建表 SQL */
const CREATE_TABLE_SQL = `
  CREATE TABLE IF NOT EXISTS ${TABLE_NAME} (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    trait_name TEXT NOT NULL,
    old_value REAL NOT NULL,
    new_value REAL NOT NULL,
    reason TEXT NOT NULL,
    consistency_check TEXT NOT NULL DEFAULT 'skipped',
    timestamp INTEGER NOT NULL
  )
`;

/** 插入 SQL */
const INSERT_SQL = `
  INSERT INTO ${TABLE_NAME} (trait_name, old_value, new_value, reason, consistency_check, timestamp)
  VALUES (@trait_name, @old_value, @new_value, @reason, @consistency_check, @timestamp)
`;

/** 查询 SQL */
const QUERY_SQL = `
  SELECT * FROM ${TABLE_NAME}
  ORDER BY timestamp DESC
  LIMIT @limit OFFSET @offset
`;

/** SQLite 演进日志管理器 */
export class EvolutionLog implements EvolutionLogStore {
  private db: Database.Database;

  constructor(dbPath?: string) {
    const resolvedPath = dbPath || process.env.EVOLUTION_DB_PATH || DEFAULT_DB_PATH;
    // 确保目录存在
    const dir = path.dirname(resolvedPath);
    // better-sqlite3 会自动创建文件，但需要确保目录存在
    this.db = new Database(resolvedPath);
    this.db.pragma('journal_mode = WAL');
    this.initialize();
  }

  /** 初始化表结构 */
  private initialize(): void {
    this.db.exec(CREATE_TABLE_SQL);
  }

  /**
   * 写入演进日志条目（架构不变量 C5）
   */
  write(entry: Omit<EvolutionLogEntry, 'timestamp'>): void {
    const stmt = this.db.prepare(INSERT_SQL);
    stmt.run({
      trait_name: entry.trait_name,
      old_value: entry.old_value,
      new_value: entry.new_value,
      reason: entry.reason,
      consistency_check: entry.consistency_check,
      timestamp: Date.now(),
    });
  }

  /**
   * 查询最近的演进日志
   * @param limit 返回条数上限
   * @param offset 偏移量
   */
  query(limit: number = 50, offset: number = 0): EvolutionLogEntry[] {
    const stmt = this.db.prepare(QUERY_SQL);
    const rows = stmt.all({ limit, offset }) as Array<{
      id: number;
      trait_name: string;
      old_value: number;
      new_value: number;
      reason: string;
      consistency_check: string;
      timestamp: number;
    }>;

    return rows.map((row) => ({
      trait_name: row.trait_name,
      old_value: row.old_value,
      new_value: row.new_value,
      reason: row.reason,
      consistency_check: row.consistency_check as EvolutionLogEntry['consistency_check'],
      timestamp: row.timestamp,
    }));
  }

  /** 获取日志总数 */
  count(): number {
    const row = this.db.prepare(`SELECT COUNT(*) as cnt FROM ${TABLE_NAME}`).get() as { cnt: number };
    return row.cnt;
  }

  /** 关闭数据库连接 */
  close(): void {
    this.db.close();
  }
}
