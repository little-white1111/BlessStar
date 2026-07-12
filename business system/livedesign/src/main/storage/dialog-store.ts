/**
 * 对话历史存储
 *
 * 基于 better-sqlite3 实现对话历史的持久化存储。
 * 提供插入、查询、清理对话历史的能力。
 * 架构不变量 #12：角色切换时清空上下文。
 */

import Database from 'better-sqlite3';
import * as path from 'path';
import { app } from 'electron';

/** 对话消息记录 */
export interface DialogMessage {
  id: number;
  /** 角色 ID（'user' 表示用户消息，角色名表示 AI 消息） */
  role: string;
  /** 消息内容 */
  content: string;
  /** 消息关联的情绪标签（仅 AI 消息有） */
  emotion?: string;
  /** 消息关联的动作标签（仅 AI 消息有） */
  action?: string;
  /** 时间戳（毫秒） */
  createdAt: number;
  /** 会话 ID，用于区分不同的对话会话 */
  sessionId: string;
  /** 角色卡 ID（关联当前使用的角色） */
  characterId: string;
}

/** 会话摘要 */
export interface SessionSummary {
  sessionId: string;
  characterId: string;
  messageCount: number;
  firstMessageAt: number;
  lastMessageAt: number;
  preview: string;
}

export class DialogStore {
  private db: Database.Database;
  private initialized = false;

  constructor() {
    // 数据库文件存储在用户数据目录下
    const userDataPath = app.getPath('userData');
    const dbPath = path.join(userDataPath, 'dialogs.db');
    this.db = new Database(dbPath);

    this.db.pragma('journal_mode = WAL');
    this.db.pragma('foreign_keys = ON');

    this.initializeSchema();
  }

  /**
   * 初始化数据库表结构
   */
  private initializeSchema(): void {
    this.db.exec(`
      CREATE TABLE IF NOT EXISTS sessions (
        id TEXT PRIMARY KEY,
        character_id TEXT NOT NULL,
        created_at INTEGER NOT NULL,
        updated_at INTEGER NOT NULL,
        title TEXT
      );

      CREATE TABLE IF NOT EXISTS messages (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        session_id TEXT NOT NULL,
        character_id TEXT NOT NULL,
        role TEXT NOT NULL,
        content TEXT NOT NULL,
        emotion TEXT,
        action TEXT,
        created_at INTEGER NOT NULL,
        FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE CASCADE
      );

      CREATE INDEX IF NOT EXISTS idx_messages_session_id
        ON messages(session_id);

      CREATE INDEX IF NOT EXISTS idx_messages_character_id
        ON messages(character_id);

      CREATE INDEX IF NOT EXISTS idx_messages_created_at
        ON messages(created_at);

      CREATE INDEX IF NOT EXISTS idx_sessions_character_id
        ON sessions(character_id);

      CREATE INDEX IF NOT EXISTS idx_sessions_updated_at
        ON sessions(updated_at);
    `);

    this.initialized = true;
  }

  /**
   * 创建新会话
   */
  createSession(characterId: string): string {
    const { v4: uuidv4 } = require('uuid');
    const sessionId = uuidv4();
    const now = Date.now();

    const stmt = this.db.prepare(`
      INSERT INTO sessions (id, character_id, created_at, updated_at, title)
      VALUES (?, ?, ?, ?, ?)
    `);

    stmt.run(sessionId, characterId, now, now, null);
    return sessionId;
  }

  /**
   * 插入一条消息
   */
  insertMessage(message: Omit<DialogMessage, 'id'>): number {
    const stmt = this.db.prepare(`
      INSERT INTO messages (session_id, character_id, role, content, emotion, action, created_at)
      VALUES (?, ?, ?, ?, ?, ?, ?)
    `);

    const result = stmt.run(
      message.sessionId,
      message.characterId,
      message.role,
      message.content,
      message.emotion ?? null,
      message.action ?? null,
      message.createdAt,
    );

    // 更新会话的 updated_at
    this.db.prepare(`
      UPDATE sessions SET updated_at = ? WHERE id = ?
    `).run(Date.now(), message.sessionId);

    return result.lastInsertRowid as number;
  }

  /**
   * 查询指定会话的消息列表
   */
  getSessionMessages(
    sessionId: string,
    options: { limit?: number; beforeId?: number } = {},
  ): DialogMessage[] {
    let query = `
      SELECT id, session_id AS sessionId, character_id AS characterId,
             role, content, emotion, action, created_at AS createdAt
      FROM messages
      WHERE session_id = ?
    `;
    const params: unknown[] = [sessionId];

    if (options.beforeId) {
      query += ' AND id < ?';
      params.push(options.beforeId);
    }

    query += ' ORDER BY created_at ASC';

    if (options.limit) {
      query += ' LIMIT ?';
      params.push(options.limit);
    }

    const stmt = this.db.prepare(query);
    return stmt.all(...params) as DialogMessage[];
  }

  /**
   * 获取最近的会话列表
   */
  getRecentSessions(limit: number = 20): SessionSummary[] {
    const query = `
      SELECT
        s.id AS sessionId,
        s.character_id AS characterId,
        COUNT(m.id) AS messageCount,
        MIN(m.created_at) AS firstMessageAt,
        MAX(m.created_at) AS lastMessageAt,
        COALESCE(SUBSTR(MIN(CASE WHEN m.role = 'user' THEN m.content END), 1, 100), '') AS preview
      FROM sessions s
      LEFT JOIN messages m ON m.session_id = s.id
      GROUP BY s.id
      ORDER BY s.updated_at DESC
      LIMIT ?
    `;

    const stmt = this.db.prepare(query);
    return stmt.all(limit) as SessionSummary[];
  }

  /**
   * 获取指定角色的会话列表
   */
  getSessionsByCharacter(characterId: string, limit: number = 20): SessionSummary[] {
    const query = `
      SELECT
        s.id AS sessionId,
        s.character_id AS characterId,
        COUNT(m.id) AS messageCount,
        MIN(m.created_at) AS firstMessageAt,
        MAX(m.created_at) AS lastMessageAt,
        COALESCE(SUBSTR(MIN(CASE WHEN m.role = 'user' THEN m.content END), 1, 100), '') AS preview
      FROM sessions s
      LEFT JOIN messages m ON m.session_id = s.id
      WHERE s.character_id = ?
      GROUP BY s.id
      ORDER BY s.updated_at DESC
      LIMIT ?
    `;

    const stmt = this.db.prepare(query);
    return stmt.all(characterId, limit) as SessionSummary[];
  }

  /**
   * 架构不变量 #12：角色切换时清空上下文
   * 清空指定角色的所有消息和会话
   */
  clearCharacterContext(characterId: string): void {
    const deleteSessions = this.db.prepare(`
      DELETE FROM sessions WHERE character_id = ?
    `);

    this.db.transaction(() => {
      // messages 表通过外键级联删除，先删 sessions 会自动删 messages
      deleteSessions.run(characterId);
    })();
  }

  /**
   * 删除指定会话
   */
  deleteSession(sessionId: string): void {
    this.db.prepare('DELETE FROM sessions WHERE id = ?').run(sessionId);
  }

  /**
   * 清理超过指定天数的历史记录
   */
  cleanOldMessages(daysToKeep: number): number {
    const cutoff = Date.now() - daysToKeep * 24 * 60 * 60 * 1000;

    const result = this.db.prepare(`
      DELETE FROM sessions WHERE updated_at < ?
    `).run(cutoff);

    return result.changes;
  }

  /**
   * 获取数据库统计信息
   */
  getStats(): {
    totalSessions: number;
    totalMessages: number;
    dbSize: number;
  } {
    const sessionCount = (this.db.prepare('SELECT COUNT(*) as count FROM sessions').get() as { count: number }).count;
    const messageCount = (this.db.prepare('SELECT COUNT(*) as count FROM messages').get() as { count: number }).count;

    // 获取数据库文件大小
    let dbSize = 0;
    try {
      const fs = require('fs');
      const userDataPath = app.getPath('userData');
      const dbPath = path.join(userDataPath, 'dialogs.db');
      dbSize = fs.statSync(dbPath).size;
    } catch {
      // 忽略文件大小获取失败
    }

    return {
      totalSessions: sessionCount,
      totalMessages: messageCount,
      dbSize,
    };
  }

  /**
   * 关闭数据库连接
   */
  close(): void {
    if (this.db && this.db.open) {
      this.db.close();
    }
  }
}

/** 全局单例 */
export const dialogStore = new DialogStore();
