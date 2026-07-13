/**
 * IPC 协议定义（架构不变量 #1 的实现基础）
 * 所有进程间通信必须通过主进程 IPC 路由，
 * 使用此协议定义的消息格式。
 */

/** 进程标识枚举 */
export enum ProcessId {
  MAIN = 'main',
  RENDERER = 'renderer',
  LLM_SERVICE = 'llm-service',
  PLUGIN_HOST = 'plugin-host',
}

/** IPC 消息通道枚举 */
export enum IpcChannel {
  // ============ Renderer ↔ LLM Service (经主进程路由) ============

  /** Renderer → LLM Service: 发送聊天消息 */
  LLM_CHAT = 'llm:chat',
  /** LLM Service → Renderer: 流式响应块 */
  LLM_STREAM_CHUNK = 'llm:stream:chunk',
  /** LLM Service → Renderer: 完整响应结束 */
  LLM_RESPONSE = 'llm:response',
  /** Renderer → LLM Service: 中止当前生成 */
  LLM_ABORT = 'llm:abort',

  // ============ Renderer ↔ LLM Service: 情绪更新 ============

  /** LLM Service → Renderer: 情绪+动作参数更新 */
  EMOTION_UPDATE = 'emotion:update',

  // ============ Renderer ↔ Plugin Host (经主进程路由) ============

  /** Renderer → Plugin Host: 调用插件方法 */
  PLUGIN_INVOKE = 'plugin:invoke',
  /** Renderer → Plugin Host: 停止插件执行 */
  PLUGIN_STOP = 'plugin:stop',
  /** Plugin Host → Renderer: 插件调用结果 */
  PLUGIN_RESULT = 'plugin:result',

  // ============ Plugin Host → Main Process: 能力请求 ============

  /** Plugin Host → Main: 请求读取文件 */
  PLUGIN_REQUEST_FILE = 'plugin:request:file',
  /** Plugin Host → Main: 请求网络访问 */
  PLUGIN_REQUEST_NETWORK = 'plugin:request:network',
  /** Main → Plugin Host: 能力请求审批结果 */
  PLUGIN_APPROVAL_RESULT = 'plugin:approval:result',

  // ============ 主进程 → 通用广播 ============

  /** 配置变更通知 */
  CONFIG_CHANGED = 'config:changed',
  /** 系统通知 */
  NOTIFICATION = 'notification:show',

  // ============ 子进程管理 ============

  /** Main → LLM Service: 切换角色 */
  LLM_SWITCH_CHARACTER = 'llm:switch:character',
  /** LLM Service → Main: 进程就绪 */
  SERVICE_READY = 'service:ready',
  /** Plugin Host → Main: 进程就绪 */
  PLUGIN_HOST_READY = 'plugin-host:ready',

  // ============ 口头禅系统 IPC 通道 ============

  /** LLM Service → Renderer: 请求用户确认口头禅晋升 */
  CATCHPHRASE_PROMOTION_REQUEST = 'catchphrase:promotion:request',
  /** Renderer → Main: 用户确认/拒绝口头禅晋升 */
  CATCHPHRASE_PROMOTION_RESULT = 'catchphrase:promotion:result',
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

/** 情绪更新载荷（架构不变量 #8, A1） */
export interface EmotionUpdatePayload {
  emotion: string;
  action: string;
  intensity: number; // 0.0 ~ 1.0
  /** VAD 三轴情感值（架构不变量 A1 — 来源 PersonalityEngine） */
  valence?: number;   // 效价: 0.0=负面, 0.5=中性, 1.0=正面
  arousal?: number;   // 唤醒度: 0.0=平静, 0.5=中性, 1.0=兴奋
  dominance?: number; // 支配度: 0.0=顺从, 0.5=中性, 1.0=支配
  /** 混合情绪 Top-3 概率（架构不变量 A9），供 Live2D 混合表情驱动 */
  mixedEmotions?: Array<{ emotion: string; probability: number }>;
}

/** 统一输入事件（架构不变量 A7 — 多模态输入归一化） */
export interface InputEvent {
  /** 事件类型 */
  type: 'text' | 'voice' | 'system' | 'screen';
  /** 事件载荷 */
  payload: string;
  /** 事件时间戳 */
  timestamp: number;
  /** 可选的元数据 */
  metadata?: Record<string, unknown>;
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
