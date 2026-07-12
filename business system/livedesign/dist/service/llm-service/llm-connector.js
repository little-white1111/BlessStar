"use strict";
/**
 * LLM API 连接管理
 * 管理 LLM API 连接（OpenAI 兼容接口），支持流式响应、中止生成、多 provider 切换
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.LlmConnector = void 0;
const stream_parser_1 = require("./stream-parser");
/** LLM 连接器 */
class LlmConnector {
    config;
    abortController = null;
    providers = {};
    currentProvider = 'default';
    constructor(config) {
        this.config = config;
    }
    /** 设置多 provider 配置 */
    setProviders(providers) {
        this.providers = providers;
    }
    /** 切换当前 provider */
    switchProvider(name) {
        if (!this.providers[name]) {
            return false;
        }
        this.currentProvider = name;
        this.config = this.providers[name];
        return true;
    }
    /** 获取当前配置 */
    getConfig() {
        return { ...this.config };
    }
    /** 获取当前 provider 名称 */
    getCurrentProvider() {
        return this.currentProvider;
    }
    /**
     * 发送聊天请求（非流式）
     */
    async chat(messages, options = {}) {
        const { temperature = 0.7, signal, tools } = options;
        // 如果传入了外部 signal，与内部 AbortController 组合
        const mergedSignal = this.mergeSignals(signal);
        const body = {
            model: this.config.model,
            messages: messages.map((m) => ({
                role: m.role,
                content: m.content,
                ...(m.tool_calls ? { tool_calls: m.tool_calls } : {}),
                ...(m.tool_call_id ? { tool_call_id: m.tool_call_id } : {}),
                ...(m.name ? { name: m.name } : {}),
            })),
            temperature,
            stream: false,
        };
        if (tools && tools.length > 0) {
            body.tools = tools;
        }
        if (options.maxTokens) {
            body.max_tokens = options.maxTokens;
        }
        const response = await fetch(`${this.config.baseUrl}/chat/completions`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                Authorization: `Bearer ${this.config.apiKey}`,
            },
            body: JSON.stringify(body),
            signal: mergedSignal,
        });
        if (!response.ok) {
            const errorText = await response.text().catch(() => '未知错误');
            throw new Error(`LLM API 请求失败 [${response.status}]: ${errorText}`);
        }
        const data = (await response.json());
        const choice = data.choices[0];
        return {
            content: choice.message.content || '',
            model: data.model,
            usage: data.usage
                ? {
                    promptTokens: data.usage.prompt_tokens,
                    completionTokens: data.usage.completion_tokens,
                    totalTokens: data.usage.total_tokens,
                }
                : undefined,
            toolCalls: choice.message.tool_calls,
        };
    }
    /**
     * 发送聊天请求（流式）
     * 通过 onStream 回调逐步返回增量文本
     */
    async chatStream(messages, options = {}) {
        const { temperature = 0.7, onStream, signal, tools } = options;
        const mergedSignal = this.mergeSignals(signal);
        // 创建内部 AbortController 用于 abort()
        this.abortController = new AbortController();
        const body = {
            model: this.config.model,
            messages: messages.map((m) => ({
                role: m.role,
                content: m.content,
                ...(m.tool_calls ? { tool_calls: m.tool_calls } : {}),
                ...(m.tool_call_id ? { tool_call_id: m.tool_call_id } : {}),
                ...(m.name ? { name: m.name } : {}),
            })),
            temperature,
            stream: true,
        };
        if (tools && tools.length > 0) {
            body.tools = tools;
        }
        if (options.maxTokens) {
            body.max_tokens = options.maxTokens;
        }
        const abortController = this.abortController;
        const combinedSignal = mergedSignal;
        const response = await fetch(`${this.config.baseUrl}/chat/completions`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                Authorization: `Bearer ${this.config.apiKey}`,
            },
            body: JSON.stringify(body),
            signal: combinedSignal,
        });
        if (!response.ok) {
            const errorText = await response.text().catch(() => '未知错误');
            throw new Error(`LLM API 流式请求失败 [${response.status}]: ${errorText}`);
        }
        const fullText = await (0, stream_parser_1.parseStream)(response, (chunk) => {
            onStream?.(chunk);
        }, abortController.signal);
        return {
            content: fullText,
            model: this.config.model,
        };
    }
    /** 中止当前生成 */
    abort() {
        if (this.abortController) {
            this.abortController.abort();
            this.abortController = null;
        }
    }
    /** 更新配置 */
    updateConfig(config) {
        this.config = { ...this.config, ...config };
    }
    /** 合并外部 signal 与内部 AbortController */
    mergeSignals(externalSignal) {
        if (!externalSignal) {
            // 如果没有外部 signal，创建一个新的 AbortController
            this.abortController = new AbortController();
            return this.abortController.signal;
        }
        // 如果有外部 signal，使用外部 signal
        // 同时内部 abort() 也会触发外部 signal
        this.abortController = new AbortController();
        const internalSignal = this.abortController.signal;
        // 当任一 signal 中止时，另一个也中止
        const onAbort = () => {
            this.abortController?.abort();
        };
        externalSignal.addEventListener('abort', onAbort, { once: true });
        internalSignal.addEventListener('abort', () => {
            externalSignal.removeEventListener('abort', onAbort);
        });
        return internalSignal;
    }
}
exports.LlmConnector = LlmConnector;
//# sourceMappingURL=llm-connector.js.map