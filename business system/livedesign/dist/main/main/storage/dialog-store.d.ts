/**
 * 对话历史存储
 *
 * 基于 better-sqlite3 实现对话历史的持久化存储。
 * 提供插入、查询、清理对话历史的能力。
 * 架构不变量 #12：角色切换时清空上下文。
 */
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
export declare class DialogStore {
    private db;
    private initialized;
    constructor();
    /**
     * 初始化数据库表结构
     */
    private initializeSchema;
    /**
     * 创建新会话
     */
    createSession(characterId: string): string;
    /**
     * 插入一条消息
     */
    insertMessage(message: Omit<DialogMessage, 'id'>): number;
    /**
     * 查询指定会话的消息列表
     */
    getSessionMessages(sessionId: string, options?: {
        limit?: number;
        beforeId?: number;
    }): DialogMessage[];
    /**
     * 获取最近的会话列表
     */
    getRecentSessions(limit?: number): SessionSummary[];
    /**
     * 获取指定角色的会话列表
     */
    getSessionsByCharacter(characterId: string, limit?: number): SessionSummary[];
    /**
     * 架构不变量 #12：角色切换时清空上下文
     * 清空指定角色的所有消息和会话
     */
    clearCharacterContext(characterId: string): void;
    /**
     * 删除指定会话
     */
    deleteSession(sessionId: string): void;
    /**
     * 清理超过指定天数的历史记录
     */
    cleanOldMessages(daysToKeep: number): number;
    /**
     * 获取数据库统计信息
     */
    getStats(): {
        totalSessions: number;
        totalMessages: number;
        dbSize: number;
    };
    /**
     * 关闭数据库连接
     */
    close(): void;
}
/** 全局单例 */
export declare const dialogStore: DialogStore;
//# sourceMappingURL=dialog-store.d.ts.map