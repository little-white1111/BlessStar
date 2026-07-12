/**
 * MCP 服务器桥接（架构不变量 #11 — MCP 桥接作为独立通信层）
 * 连接外部 MCP 服务器（如 Trae Work），
 * 将 LLM 的工具调用请求转发到 MCP Server，
 * 将 MCP Server 的响应注入回 LLM Context。
 */
/** MCP 服务器配置 */
export interface McpServerConfig {
    /** 服务器名称 */
    name: string;
    /** 服务器脚本路径（Node.js 子进程入口） */
    scriptPath: string;
    /** 服务器启动参数 */
    args?: string[];
    /** 超时时间（毫秒） */
    timeout?: number;
}
/** MCP 工具定义 */
export interface McpToolDefinition {
    name: string;
    description: string;
    parameters: Record<string, unknown>;
}
/** MCP 调用请求 */
export interface McpToolCallRequest {
    toolName: string;
    arguments: Record<string, unknown>;
    callId: string;
}
/** MCP 调用响应 */
export interface McpToolCallResponse {
    callId: string;
    success: boolean;
    content: Array<{
        type: 'text' | 'image' | 'resource';
        text?: string;
        mimeType?: string;
        uri?: string;
    }>;
    isError?: boolean;
}
/** MCP 服务器状态 */
export declare enum McpServerStatus {
    DISCONNECTED = "disconnected",
    CONNECTING = "connecting",
    CONNECTED = "connected",
    ERROR = "error"
}
/** MCP 桥接事件回调 */
export interface McpBridgeCallbacks {
    onStatusChange?: (status: McpServerStatus, message?: string) => void;
    onToolCallResult?: (response: McpToolCallResponse) => void;
    onError?: (error: Error) => void;
}
/**
 * MCP 桥接器
 * 管理外部 MCP 服务器的生命周期和通信
 */
export declare class McpBridge {
    private process;
    private config;
    private status;
    private callbacks;
    private tools;
    private pendingRequests;
    private requestCounter;
    private reconnectTimer;
    constructor(config: McpServerConfig, callbacks?: McpBridgeCallbacks);
    /** 获取当前连接的服务器名称 */
    getServerName(): string;
    /** 获取已注册的工具列表 */
    getTools(): McpToolDefinition[];
    /** 获取连接状态 */
    getStatus(): McpServerStatus;
    /** 连接到 MCP 服务器（启动子进程） */
    connect(): Promise<void>;
    /** 断开连接（关闭子进程） */
    disconnect(): Promise<void>;
    /**
     * 调用 MCP 工具
     * 向 MCP 子进程发送工具调用请求，等待响应
     */
    callTool(request: McpToolCallRequest): Promise<McpToolCallResponse>;
    /**
     * 刷新工具列表
     * 向 MCP 子进程请求最新的工具列表
     */
    refreshTools(): Promise<McpToolDefinition[]>;
    /** 设置自动重连（间隔毫秒） */
    enableAutoReconnect(intervalMs?: number): void;
    /** 停止自动重连 */
    disableAutoReconnect(): void;
    /** 设置子进程事件监听 */
    private setupProcessListeners;
    /** 处理来自子进程的消息 */
    private handleChildMessage;
    /** 向子进程发送消息 */
    private sendToChild;
    /** 等待子进程 ready 信号 */
    private waitForReady;
    /** 更新状态并触发回调 */
    private setStatus;
    /** 停止重连定时器 */
    private stopReconnect;
}
/**
 * MCP 工具调用格式化工具
 * 将 LLM 的 tool_calls 转换为 MCP 调用请求
 */
export declare function mcpToolCallsToRequests(toolCalls: NonNullable<import('./llm-connector').ChatResult['toolCalls']>): McpToolCallRequest[];
/**
 * 将 MCP 工具调用结果转换为 LLM 可用的 tool message
 */
export declare function mcpResponseToToolMessage(response: McpToolCallResponse): {
    role: 'tool';
    content: string;
    tool_call_id: string;
};
//# sourceMappingURL=mcp-bridge.d.ts.map