/**
 * LLM API 连接管理
 * 管理 LLM API 连接（OpenAI 兼容接口），支持流式响应、中止生成、多 provider 切换
 */
/** LLM 连接器配置 */
export interface LlmConnectorConfig {
    apiKey: string;
    baseUrl: string;
    model: string;
}
/** LLM 消息格式 */
export interface LlmMessage {
    role: 'system' | 'user' | 'assistant' | 'tool';
    content: string;
    tool_call_id?: string;
    name?: string;
    tool_calls?: Array<{
        id: string;
        type: 'function';
        function: {
            name: string;
            arguments: string;
        };
    }>;
}
/** 聊天选项 */
export interface ChatOptions {
    temperature?: number;
    maxTokens?: number;
    /** 流式回调 — 每次收到增量文本时触发 */
    onStream?: (chunk: string) => void;
    /** 中止信号 */
    signal?: AbortSignal;
    /** 工具定义列表 */
    tools?: Array<{
        type: 'function';
        function: {
            name: string;
            description: string;
            parameters: Record<string, unknown>;
        };
    }>;
}
/** 聊天完成结果 */
export interface ChatResult {
    /** 完整回复内容 */
    content: string;
    /** 使用的模型 */
    model: string;
    /** token 用量 */
    usage?: {
        promptTokens: number;
        completionTokens: number;
        totalTokens: number;
    };
    /** 工具调用请求（由 LLM 发起的函数调用） */
    toolCalls?: Array<{
        id: string;
        type: 'function';
        function: {
            name: string;
            arguments: string;
        };
    }>;
}
/** 可用 provider 配置映射 */
export type ProviderMap = Record<string, LlmConnectorConfig>;
/** LLM 连接器 */
export declare class LlmConnector {
    private config;
    private abortController;
    private providers;
    private currentProvider;
    constructor(config: LlmConnectorConfig);
    /** 设置多 provider 配置 */
    setProviders(providers: ProviderMap): void;
    /** 切换当前 provider */
    switchProvider(name: string): boolean;
    /** 获取当前配置 */
    getConfig(): LlmConnectorConfig;
    /** 获取当前 provider 名称 */
    getCurrentProvider(): string;
    /**
     * 发送聊天请求（非流式）
     */
    chat(messages: LlmMessage[], options?: ChatOptions): Promise<ChatResult>;
    /**
     * 发送聊天请求（流式）
     * 通过 onStream 回调逐步返回增量文本
     */
    chatStream(messages: LlmMessage[], options?: ChatOptions): Promise<ChatResult>;
    /** 中止当前生成 */
    abort(): void;
    /** 更新配置 */
    updateConfig(config: Partial<LlmConnectorConfig>): void;
    /** 合并外部 signal 与内部 AbortController */
    private mergeSignals;
}
//# sourceMappingURL=llm-connector.d.ts.map