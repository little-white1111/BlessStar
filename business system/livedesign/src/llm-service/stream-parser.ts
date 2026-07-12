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

/** OpenAI 流式响应中的 delta 结构 */
interface OpenAIDelta {
  content?: string;
  role?: string;
  tool_calls?: unknown[];
}

/** OpenAI 流式响应 choice 结构 */
interface OpenAIStreamChoice {
  index: number;
  delta: OpenAIDelta;
  finish_reason: string | null;
}

/** OpenAI 流式响应整体结构 */
interface OpenAIStreamChunk {
  id: string;
  object: string;
  created: number;
  model: string;
  choices: OpenAIStreamChoice[];
}

/**
 * SSE 解析器
 * 逐行读取 SSE 格式数据流，提取 data 字段内容
 */
export class SseParser {
  private buffer: string = '';
  private callbacks: SseCallbacks;

  constructor(callbacks: SseCallbacks) {
    this.callbacks = callbacks;
  }

  /**
   * 喂入原始文本块（来自 fetch ReadableStream）
   * 每次喂入后尝试解析完整的 SSE 行
   */
  feed(chunk: string): void {
    this.buffer += chunk;

    // 按行分割（SSE 使用 \n 作为分隔符）
    const lines = this.buffer.split('\n');
    // 最后一个元素可能是不完整的行，留到下次
    this.buffer = lines.pop() || '';

    for (const line of lines) {
      this.processLine(line);
    }
  }

  /** 处理单行 SSE 数据 */
  private processLine(line: string): void {
    const trimmed = line.trim();

    // 空行表示事件结束
    if (trimmed === '') {
      return;
    }

    // data: 前缀行
    if (trimmed.startsWith('data:')) {
      const data = trimmed.slice(5).trim();

      // SSE 结束标记
      if (data === '[DONE]') {
        this.callbacks.onDone?.();
        return;
      }

      this.callbacks.onData?.(data);
      return;
    }

    // 忽略其他 SSE 字段（event:, id:, retry: 等）
  }

  /** 重置解析器状态 */
  reset(): void {
    this.buffer = '';
  }
}

/**
 * OpenAI 流式响应解析器
 * 将 SSE data 内容解析为 OpenAI 格式的流式块
 */
export class OpenAIStreamParser {
  private sseParser: SseParser;
  private accumulatedText: string = '';
  private onChunk: (chunk: StreamChunk) => void;

  constructor(onChunk: (chunk: StreamChunk) => void) {
    this.onChunk = onChunk;
    this.sseParser = new SseParser({
      onData: (data) => this.handleData(data),
      onDone: () => this.handleDone(),
      onError: (err) => this.handleError(err),
    });
  }

  /** 喂入原始 SSE 数据 */
  feed(chunk: string): void {
    this.sseParser.feed(chunk);
  }

  /** 处理单条 data 内容 */
  private handleData(data: string): void {
    try {
      const parsed: OpenAIStreamChunk = JSON.parse(data);
      if (!parsed.choices || parsed.choices.length === 0) {
        return;
      }

      const delta = parsed.choices[0].delta;
      const finishReason = parsed.choices[0].finish_reason;

      const contentDelta = delta.content || '';
      this.accumulatedText += contentDelta;

      const done = finishReason !== null;

      this.onChunk({
        text: this.accumulatedText,
        delta: contentDelta,
        done,
      });
    } catch {
      // 如果 JSON 解析失败，当成纯文本处理
      this.accumulatedText += data;
      this.onChunk({
        text: this.accumulatedText,
        delta: data,
        done: false,
      });
    }
  }

  /** 流结束处理 */
  private handleDone(): void {
    this.onChunk({
      text: this.accumulatedText,
      delta: '',
      done: true,
    });
  }

  /** 错误处理 */
  private handleError(error: Error): void {
    this.onChunk({
      text: this.accumulatedText,
      delta: '',
      done: true,
    });
    throw error;
  }

  /** 获取当前累积的文本 */
  getAccumulatedText(): string {
    return this.accumulatedText;
  }

  /** 重置状态 */
  reset(): void {
    this.accumulatedText = '';
    this.sseParser.reset();
  }
}

/**
 * 通用流解析函数
 * 从 fetch Response body 读取流并解析
 */
export async function parseStream(
  response: Response,
  onChunk: (chunk: string) => void,
  signal?: AbortSignal
): Promise<string> {
  if (!response.body) {
    throw new Error('响应体为空，无法读取流');
  }

  const reader = response.body.getReader();
  const decoder = new TextDecoder();
  const parser = new OpenAIStreamParser((chunk) => {
    onChunk(chunk.delta);
  });

  let fullText = '';

  try {
    while (true) {
      if (signal?.aborted) {
        await reader.cancel();
        break;
      }

      const { done, value } = await reader.read();
      if (done) {
        break;
      }

      const text = decoder.decode(value, { stream: true });
      parser.feed(text);

      // 同步累积的完整文本
      fullText = parser.getAccumulatedText();
    }
  } finally {
    reader.releaseLock();
  }

  return fullText;
}
