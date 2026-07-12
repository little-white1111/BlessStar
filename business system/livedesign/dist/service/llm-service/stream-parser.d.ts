/**
 * 流式响应解析器
 * 解析 SSE (Server-Sent Events) 格式，支持 OpenAI 兼容接口
 */
/** SSE 解析事件回调 */
export interface SseCallbacks {
    /** 收到一个完整的数据行（data: 之后的内容） */
    onData?: (data: string) => void;
    /** 流结束 */
    onDone?: () => void;
    /** 发生错误 */
    onError?: (error: Error) => void;
}
/** 解析后的流式数据块 */
export interface StreamChunk {
    /** 累积的纯文本内容（不含情绪标签） */
    text: string;
    /** 本次增量文本 */
    delta: string;
    /** 是否完成 */
    done: boolean;
}
/**
 * SSE 解析器
 * 逐行读取 SSE 格式数据流，提取 data 字段内容
 */
export declare class SseParser {
    private buffer;
    private callbacks;
    constructor(callbacks: SseCallbacks);
    /**
     * 喂入原始文本块（来自 fetch ReadableStream）
     * 每次喂入后尝试解析完整的 SSE 行
     */
    feed(chunk: string): void;
    /** 处理单行 SSE 数据 */
    private processLine;
    /** 重置解析器状态 */
    reset(): void;
}
/**
 * OpenAI 流式响应解析器
 * 将 SSE data 内容解析为 OpenAI 格式的流式块
 */
export declare class OpenAIStreamParser {
    private sseParser;
    private accumulatedText;
    private onChunk;
    constructor(onChunk: (chunk: StreamChunk) => void);
    /** 喂入原始 SSE 数据 */
    feed(chunk: string): void;
    /** 处理单条 data 内容 */
    private handleData;
    /** 流结束处理 */
    private handleDone;
    /** 错误处理 */
    private handleError;
    /** 获取当前累积的文本 */
    getAccumulatedText(): string;
    /** 重置状态 */
    reset(): void;
}
/**
 * 通用流解析函数
 * 从 fetch Response body 读取流并解析
 */
export declare function parseStream(response: Response, onChunk: (chunk: string) => void, signal?: AbortSignal): Promise<string>;
//# sourceMappingURL=stream-parser.d.ts.map