/**
 * MCP 服务器桥接（架构不变量 #11 — MCP 桥接作为独立通信层）
 * 连接外部 MCP 服务器（如 Trae Work），
 * 将 LLM 的工具调用请求转发到 MCP Server，
 * 将 MCP Server 的响应注入回 LLM Context。
 */

import { ChildProcess, fork } from 'child_process';
import * as path from 'path';

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
export enum McpServerStatus {
  DISCONNECTED = 'disconnected',
  CONNECTING = 'connecting',
  CONNECTED = 'connected',
  ERROR = 'error',
}

/** MCP 桥接事件回调 */
export interface McpBridgeCallbacks {
  onStatusChange?: (status: McpServerStatus, message?: string) => void;
  onToolCallResult?: (response: McpToolCallResponse) => void;
  onError?: (error: Error) => void;
}

/** 子进程消息协议 */
interface McpChildMessage {
  type: 'tool_list' | 'tool_result' | 'error' | 'ready';
  payload: unknown;
}

interface McpParentMessage {
  type: 'call_tool' | 'list_tools' | 'shutdown';
  payload: unknown;
}

/**
 * MCP 桥接器
 * 管理外部 MCP 服务器的生命周期和通信
 */
export class McpBridge {
  private process: ChildProcess | null = null;
  private config: McpServerConfig;
  private status: McpServerStatus = McpServerStatus.DISCONNECTED;
  private callbacks: McpBridgeCallbacks;
  private tools: McpToolDefinition[] = [];
  private pendingRequests: Map<
    string,
    {
      resolve: (value: McpToolCallResponse) => void;
      reject: (reason: Error) => void;
      timeout: ReturnType<typeof setTimeout>;
    }
  > = new Map();
  private requestCounter: number = 0;
  private reconnectTimer: ReturnType<typeof setInterval> | null = null;

  constructor(config: McpServerConfig, callbacks: McpBridgeCallbacks = {}) {
    this.config = {
      timeout: 30000,
      ...config,
    };
    this.callbacks = callbacks;
  }

  /** 获取当前连接的服务器名称 */
  getServerName(): string {
    return this.config.name;
  }

  /** 获取已注册的工具列表 */
  getTools(): McpToolDefinition[] {
    return [...this.tools];
  }

  /** 获取连接状态 */
  getStatus(): McpServerStatus {
    return this.status;
  }

  /** 连接到 MCP 服务器（启动子进程） */
  async connect(): Promise<void> {
    if (this.status === McpServerStatus.CONNECTED) {
      return;
    }

    this.setStatus(McpServerStatus.CONNECTING, '正在连接 MCP 服务器...');

    try {
      this.process = fork(this.config.scriptPath, this.config.args || [], {
        stdio: ['pipe', 'pipe', 'pipe', 'ipc'],
        env: { ...process.env },
      });

      this.setupProcessListeners();

      // 等待 ready 消息
      await this.waitForReady();
    } catch (error) {
      const message =
        error instanceof Error ? error.message : '未知错误';
      this.setStatus(McpServerStatus.ERROR, `连接失败: ${message}`);
      throw error;
    }
  }

  /** 断开连接（关闭子进程） */
  async disconnect(): Promise<void> {
    this.stopReconnect();

    if (this.process) {
      // 发送关闭消息
      this.sendToChild({ type: 'shutdown', payload: {} });

      // 等待进程退出
      const exitPromise = new Promise<void>((resolve) => {
        if (!this.process) {
          resolve();
          return;
        }
        this.process.once('exit', () => resolve());
      });

      // 强制杀死进程（如果 3 秒后仍未退出）
      setTimeout(() => {
        if (this.process && !this.process.killed) {
          this.process.kill('SIGKILL');
        }
      }, 3000);

      await exitPromise;
      this.process = null;
    }

    this.tools = [];
    this.setStatus(McpServerStatus.DISCONNECTED, '已断开连接');
  }

  /**
   * 调用 MCP 工具
   * 向 MCP 子进程发送工具调用请求，等待响应
   */
  async callTool(request: McpToolCallRequest): Promise<McpToolCallResponse> {
    if (this.status !== McpServerStatus.CONNECTED || !this.process) {
      throw new Error('MCP 服务器未连接');
    }

    return new Promise<McpToolCallResponse>((resolve, reject) => {
      const callId = request.callId || `call_${++this.requestCounter}`;

      const timeout = setTimeout(() => {
        this.pendingRequests.delete(callId);
        reject(
          new Error(
            `MCP 工具调用超时 [${request.toolName}]: ${this.config.timeout}ms`
          )
        );
      }, this.config.timeout);

      this.pendingRequests.set(callId, { resolve, reject, timeout });

      this.sendToChild({
        type: 'call_tool',
        payload: {
          name: request.toolName,
          arguments: request.arguments,
          callId,
        },
      });
    });
  }

  /**
   * 刷新工具列表
   * 向 MCP 子进程请求最新的工具列表
   */
  async refreshTools(): Promise<McpToolDefinition[]> {
    if (this.status !== McpServerStatus.CONNECTED || !this.process) {
      throw new Error('MCP 服务器未连接');
    }

    return new Promise<McpToolDefinition[]>((resolve, reject) => {
      const timeout = setTimeout(() => {
        reject(new Error('获取工具列表超时'));
      }, this.config.timeout);

      const handler = (msg: McpChildMessage) => {
        if (msg.type === 'tool_list') {
          clearTimeout(timeout);
          this.process?.removeListener('message', handler);
          this.tools = (msg.payload as { tools: McpToolDefinition[] }).tools;
          resolve(this.tools);
        }
      };

      if (this.process) {
        this.process.on('message', handler);
      }
      this.sendToChild({ type: 'list_tools', payload: {} });
    });
  }

  /** 设置自动重连（间隔毫秒） */
  enableAutoReconnect(intervalMs: number = 10000): void {
    this.stopReconnect();
    this.reconnectTimer = setInterval(() => {
      if (
        this.status === McpServerStatus.DISCONNECTED ||
        this.status === McpServerStatus.ERROR
      ) {
        this.connect().catch(() => {
          // 重连失败，下次再试
        });
      }
    }, intervalMs);
  }

  /** 停止自动重连 */
  disableAutoReconnect(): void {
    this.stopReconnect();
  }

  /** 设置子进程事件监听 */
  private setupProcessListeners(): void {
    if (!this.process) {
      return;
    }

    this.process.on('message', (msg: McpChildMessage) => {
      this.handleChildMessage(msg);
    });

    this.process.on('error', (err) => {
      this.setStatus(McpServerStatus.ERROR, `进程错误: ${err.message}`);
      this.callbacks.onError?.(err);
    });

    this.process.on('exit', (code, signal) => {
      const reason = signal
        ? `信号 ${signal}`
        : `退出码 ${code}`;
      this.setStatus(
        McpServerStatus.DISCONNECTED,
        `进程已退出 (${reason})`
      );
      this.process = null;
    });

    // 转发子进程 stdout/stderr
    if (this.process.stdout) {
      this.process.stdout.on('data', (data: Buffer) => {
        // 可以在此处添加日志
      });
    }

    if (this.process.stderr) {
      this.process.stderr.on('data', (data: Buffer) => {
        // 可以在此处添加日志
      });
    }
  }

  /** 处理来自子进程的消息 */
  private handleChildMessage(msg: McpChildMessage): void {
    switch (msg.type) {
      case 'ready':
        this.setStatus(McpServerStatus.CONNECTED, 'MCP 服务器已就绪');
        this.refreshTools().catch(() => {
          // 工具列表刷新失败不阻塞
        });
        break;

      case 'tool_result': {
        const response = msg.payload as McpToolCallResponse;
        const pending = this.pendingRequests.get(response.callId);
        if (pending) {
          clearTimeout(pending.timeout);
          this.pendingRequests.delete(response.callId);
          pending.resolve(response);
        }
        this.callbacks.onToolCallResult?.(response);
        break;
      }

      case 'error': {
        const errorPayload = msg.payload as { message: string };
        const error = new Error(errorPayload.message || 'MCP 子进程错误');
        this.callbacks.onError?.(error);
        break;
      }
    }
  }

  /** 向子进程发送消息 */
  private sendToChild(msg: McpParentMessage): void {
    if (this.process && this.process.connected) {
      this.process.send(msg);
    }
  }

  /** 等待子进程 ready 信号 */
  private waitForReady(): Promise<void> {
    return new Promise<void>((resolve, reject) => {
      if (!this.process) {
        reject(new Error('子进程未创建'));
        return;
      }

      const timeout = setTimeout(() => {
        reject(new Error('MCP 子进程就绪超时'));
      }, this.config.timeout);

      const handler = (msg: McpChildMessage) => {
        if (msg.type === 'ready') {
          clearTimeout(timeout);
          this.process?.removeListener('message', handler);
          resolve();
        }
      };

      this.process.on('message', handler);
    });
  }

  /** 更新状态并触发回调 */
  private setStatus(status: McpServerStatus, message?: string): void {
    this.status = status;
    this.callbacks.onStatusChange?.(status, message);
  }

  /** 停止重连定时器 */
  private stopReconnect(): void {
    if (this.reconnectTimer) {
      clearInterval(this.reconnectTimer);
      this.reconnectTimer = null;
    }
  }
}

/**
 * MCP 工具调用格式化工具
 * 将 LLM 的 tool_calls 转换为 MCP 调用请求
 */
export function mcpToolCallsToRequests(
  toolCalls: NonNullable<import('./llm-connector').ChatResult['toolCalls']>
): McpToolCallRequest[] {
  return toolCalls.map((tc: { id: string; type: string; function: { name: string; arguments: string } }) => ({
    toolName: tc.function.name,
    arguments: JSON.parse(tc.function.arguments) as Record<string, unknown>,
    callId: tc.id,
  }));
}

/**
 * 将 MCP 工具调用结果转换为 LLM 可用的 tool message
 */
export function mcpResponseToToolMessage(
  response: McpToolCallResponse
): {
  role: 'tool';
  content: string;
  tool_call_id: string;
} {
  const contentText = response.content
    .filter((c) => c.type === 'text')
    .map((c) => c.text || '')
    .join('\n');

  return {
    role: 'tool',
    content: contentText,
    tool_call_id: response.callId,
  };
}
