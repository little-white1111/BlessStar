/**
 * LLM API 连接管理
 * 管理 LLM API 连接（OpenAI 兼容接口），支持流式响应、中止生成、多 provider 切换
 */

import { parseStream } from './stream-parser';

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
export class LlmConnector {
  private config: LlmConnectorConfig;
  private abortController: AbortController | null = null;
  private providers: ProviderMap = {};
  private currentProvider: string = 'default';

  constructor(config: LlmConnectorConfig) {
    this.config = config;
  }

  /** 设置多 provider 配置 */
  setProviders(providers: ProviderMap): void {
    this.providers = providers;
  }

  /** 切换当前 provider */
  switchProvider(name: string): boolean {
    if (!this.providers[name]) {
      return false;
    }
    this.currentProvider = name;
    this.config = this.providers[name];
    return true;
  }

  /** 获取当前配置 */
  getConfig(): LlmConnectorConfig {
    return { ...this.config };
  }

  /** 获取当前 provider 名称 */
  getCurrentProvider(): string {
    return this.currentProvider;
  }

  /**
   * 发送聊天请求（非流式）
   */
  async chat(
    messages: LlmMessage[],
    options: ChatOptions = {}
  ): Promise<ChatResult> {
    const { temperature = 0.7, signal, tools } = options;

    // 如果传入了外部 signal，与内部 AbortController 组合
    const mergedSignal = this.mergeSignals(signal);

    const body: Record<string, unknown> = {
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
      throw new Error(
        `LLM API 请求失败 [${response.status}]: ${errorText}`
      );
    }

    const data = (await response.json()) as {
      id: string;
      object: string;
      created: number;
      model: string;
      choices: Array<{
        index: number;
        message: {
          role: string;
          content: string | null;
          tool_calls?: Array<{
            id: string;
            type: 'function';
            function: {
              name: string;
              arguments: string;
            };
          }>;
        };
        finish_reason: string;
      }>;
      usage?: {
        prompt_tokens: number;
        completion_tokens: number;
        total_tokens: number;
      };
    };

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
  async chatStream(
    messages: LlmMessage[],
    options: ChatOptions = {}
  ): Promise<ChatResult> {
    const { temperature = 0.7, onStream, signal, tools } = options;

    const mergedSignal = this.mergeSignals(signal);

    // 创建内部 AbortController 用于 abort()
    this.abortController = new AbortController();

    const body: Record<string, unknown> = {
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
      throw new Error(
        `LLM API 流式请求失败 [${response.status}]: ${errorText}`
      );
    }

    const fullText = await parseStream(
      response,
      (chunk) => {
        onStream?.(chunk);
      },
      abortController.signal
    );

    return {
      content: fullText,
      model: this.config.model,
    };
  }

  /** 中止当前生成 */
  abort(): void {
    if (this.abortController) {
      this.abortController.abort();
      this.abortController = null;
    }
  }

  /** 更新配置 */
  updateConfig(config: Partial<LlmConnectorConfig>): void {
    this.config = { ...this.config, ...config };
  }

  /** 合并外部 signal 与内部 AbortController */
  private mergeSignals(externalSignal?: AbortSignal): AbortSignal | undefined {
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
