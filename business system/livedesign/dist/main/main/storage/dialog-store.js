"use strict";
/**
 * 对话历史存储
 *
 * 基于 better-sqlite3 实现对话历史的持久化存储。
 * 提供插入、查询、清理对话历史的能力。
 * 架构不变量 #12：角色切换时清空上下文。
 */
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.dialogStore = exports.DialogStore = void 0;
const better_sqlite3_1 = __importDefault(require("better-sqlite3"));
const path = __importStar(require("path"));
const electron_1 = require("electron");
class DialogStore {
    db;
    initialized = false;
    constructor() {
        // 数据库文件存储在用户数据目录下
        const userDataPath = electron_1.app.getPath('userData');
        const dbPath = path.join(userDataPath, 'dialogs.db');
        this.db = new better_sqlite3_1.default(dbPath);
        this.db.pragma('journal_mode = WAL');
        this.db.pragma('foreign_keys = ON');
        this.initializeSchema();
    }
    /**
     * 初始化数据库表结构
     */
    initializeSchema() {
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
    createSession(characterId) {
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
    insertMessage(message) {
        const stmt = this.db.prepare(`
      INSERT INTO messages (session_id, character_id, role, content, emotion, action, created_at)
      VALUES (?, ?, ?, ?, ?, ?, ?)
    `);
        const result = stmt.run(message.sessionId, message.characterId, message.role, message.content, message.emotion ?? null, message.action ?? null, message.createdAt);
        // 更新会话的 updated_at
        this.db.prepare(`
      UPDATE sessions SET updated_at = ? WHERE id = ?
    `).run(Date.now(), message.sessionId);
        return result.lastInsertRowid;
    }
    /**
     * 查询指定会话的消息列表
     */
    getSessionMessages(sessionId, options = {}) {
        let query = `
      SELECT id, session_id AS sessionId, character_id AS characterId,
             role, content, emotion, action, created_at AS createdAt
      FROM messages
      WHERE session_id = ?
    `;
        const params = [sessionId];
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
        return stmt.all(...params);
    }
    /**
     * 获取最近的会话列表
     */
    getRecentSessions(limit = 20) {
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
        return stmt.all(limit);
    }
    /**
     * 获取指定角色的会话列表
     */
    getSessionsByCharacter(characterId, limit = 20) {
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
        return stmt.all(characterId, limit);
    }
    /**
     * 架构不变量 #12：角色切换时清空上下文
     * 清空指定角色的所有消息和会话
     */
    clearCharacterContext(characterId) {
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
    deleteSession(sessionId) {
        this.db.prepare('DELETE FROM sessions WHERE id = ?').run(sessionId);
    }
    /**
     * 清理超过指定天数的历史记录
     */
    cleanOldMessages(daysToKeep) {
        const cutoff = Date.now() - daysToKeep * 24 * 60 * 60 * 1000;
        const result = this.db.prepare(`
      DELETE FROM sessions WHERE updated_at < ?
    `).run(cutoff);
        return result.changes;
    }
    /**
     * 获取数据库统计信息
     */
    getStats() {
        const sessionCount = this.db.prepare('SELECT COUNT(*) as count FROM sessions').get().count;
        const messageCount = this.db.prepare('SELECT COUNT(*) as count FROM messages').get().count;
        // 获取数据库文件大小
        let dbSize = 0;
        try {
            const fs = require('fs');
            const userDataPath = electron_1.app.getPath('userData');
            const dbPath = path.join(userDataPath, 'dialogs.db');
            dbSize = fs.statSync(dbPath).size;
        }
        catch {
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
    close() {
        if (this.db && this.db.open) {
            this.db.close();
        }
    }
}
exports.DialogStore = DialogStore;
/** 全局单例 */
exports.dialogStore = new DialogStore();
//# sourceMappingURL=dialog-store.js.map