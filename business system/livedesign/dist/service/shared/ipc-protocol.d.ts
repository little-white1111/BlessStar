/**
 * IPC 协议定义（架构不变量 #1 的实现基础）
 * 所有进程间通信必须通过主进程 IPC 路由，
 * 使用此协议定义的消息格式。
 */
/** 进程标识枚举 */
export declare enum ProcessId {
    MAIN = "main",
    RENDERER = "renderer",
    LLM_SERVICE = "llm-service",
    PLUGIN_HOST = "plugin-host"
}
/** IPC 消息通道枚举 */
export declare enum IpcChannel {
    /** Renderer → LLM Service: 发送聊天消息 */
    LLM_CHAT = "llm:chat",
    /** LLM Service → Renderer: 流式响应块 */
    LLM_STREAM_CHUNK = "llm:stream:chunk",
    /** LLM Service → Renderer: 完整响应结束 */
    LLM_RESPONSE = "llm:response",
    /** Renderer → LLM Service: 中止当前生成 */
    LLM_ABORT = "llm:abort",
    /** LLM Service → Renderer: 情绪+动作参数更新 */
    EMOTION_UPDATE = "emotion:update",
    /** Renderer → Plugin Host: 调用插件方法 */
    PLUGIN_INVOKE = "plugin:invoke",
    /** Renderer → Plugin Host: 停止插件执行 */
    PLUGIN_STOP = "plugin:stop",
    /** Plugin Host → Renderer: 插件调用结果 */
    PLUGIN_RESULT = "plugin:result",
    /** Plugin Host → Main: 请求读取文件 */
    PLUGIN_REQUEST_FILE = "plugin:request:file",
    /** Plugin Host → Main: 请求网络访问 */
    PLUGIN_REQUEST_NETWORK = "plugin:request:network",
    /** Main → Plugin Host: 能力请求审批结果 */
    PLUGIN_APPROVAL_RESULT = "plugin:approval:result",
    /** 配置变更通知 */
    CONFIG_CHANGED = "config:changed",
    /** 系统通知 */
    NOTIFICATION = "notification:show",
    /** Main → LLM Service: 切换角色 */
    LLM_SWITCH_CHARACTER = "llm:switch:character",
    /** LLM Service → Main: 进程就绪 */
    SERVICE_READY = "service:ready",
    /** Plugin Host → Main: 进程就绪 */
    PLUGIN_HOST_READY = "plugin-host:ready"
}
/** LLM 聊天请求载荷 */
export interface LlmChatPayload {
    message: string;
    rolePreset: string;
    temperature?: number;
    tools?: string[];
}
/** LLM 流式响应块载荷 */
export interface LlmStreamChunkPayload {
    /** 累积文本块 */
    text: string;
    /** 是否完成 */
    done: boolean;
}
/** 情绪更新载荷（架构不变量 #8） */
export interface EmotionUpdatePayload {
    emotion: string;
    action: string;
    intensity: number;
}
/** 插件调用请求载荷 */
export interface PluginInvokePayload {
    pluginId: string;
    method: string;
    args: unknown[];
}
/** IPC 消息统一信封（架构不变量 #1 — 所有消息经由此格式） */
export interface IpcEnvelope {
    /** 消息唯一 ID */
    id: string;
    /** 消息目标通道 */
    channel: IpcChannel;
    /** 消息载荷 */
    payload: unknown;
    /** 来源进程标识（由 IPC Router 自动注入） */
    source?: ProcessId;
    /** 错误信息（响应端填充） */
    error?: string;
}
/** IPC Router 的请求-响应配对 */
export interface PendingRequest {
    resolve: (value: unknown) => void;
    reject: (reason: unknown) => void;
    timeout: ReturnType<typeof setTimeout>;
}
//# sourceMappingURL=ipc-protocol.d.ts.map