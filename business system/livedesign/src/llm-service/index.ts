/**
 * LLM Service 子进程入口
 * 通过 child_process.fork 启动，使用 process.on('message') / process.send() 与主进程通信
 *
 * 架构不变量遵守：
 *   #7  — 角色卡是唯一真理源
 *   #8  — 情绪推断结果必须同步输出 emotion + action 参数
 *   #11 — MCP 桥接作为独立通信层
 *   #12 — 收到 LLM_SWITCH_CHARACTER 时清空对话历史
 */

import { IpcChannel, IpcEnvelope, ProcessId, InputEvent } from '../shared/ipc-protocol';
import { LlmConnector, LlmMessage, ChatOptions } from './llm-connector';
import { CharacterManager } from './character-manager';
import { McpBridge } from './mcp-bridge';
import {
  PersonalityEngine,
  DEFAULT_PERSONALITY_CONFIG,
} from './personality/engine';
import { AgentOrchestrator } from './agent/orchestrator';
import { NarrativeSystem } from './narrative';
import { PersonaSystem } from './persona/index';

// ==================== 模块实例 ====================

/** LLM API 连接器 */
const connector = new LlmConnector({
  apiKey: process.env.LLM_API_KEY || '',
  baseUrl: process.env.LLM_BASE_URL || 'https://api.openai.com/v1',
  model: process.env.LLM_MODEL || 'gpt-4o',
});

/** 角色卡管理器 — 从 resources/characters/ 目录加载 */
const characterManager = new CharacterManager(
  process.env.CHARACTERS_DIR || './resources/characters'
);

/**
 * 人格引擎（架构不变量 A1/A2）
 * — 情感系统的唯一真理源（A1）
 * — 构造时自动启动情感衰减定时器（A2）
 */
const personalityEngine = new PersonalityEngine({
  playfulness: parseFloat(process.env.PERSONALITY_PLAYFULNESS ?? String(DEFAULT_PERSONALITY_CONFIG.playfulness)),
  empathy: parseFloat(process.env.PERSONALITY_EMPATHY ?? String(DEFAULT_PERSONALITY_CONFIG.empathy)),
  decayIntervalMs: parseInt(process.env.PERSONALITY_DECAY_INTERVAL_MS ?? String(DEFAULT_PERSONALITY_CONFIG.decayIntervalMs), 10),
  decayRate: parseFloat(process.env.PERSONALITY_DECAY_RATE ?? String(DEFAULT_PERSONALITY_CONFIG.decayRate)),
});

/** Agent 编排器（架构不变量 A5/A7）
 * — 所有请求必须经过 Perceptor → Planner → Executor → Expresser（A5）
 * — 输入已归一化为 InputEvent（A7）
 */
const orchestrator = new AgentOrchestrator();

/**
 * 叙事系统（架构不变量 B1-B7）
 * — 用户叙事与动态画像
 * — L1 原始观察写入 → L2 量化计算 → L3 LLM 反思 → L4 画像合成
 */
const narrativeSystem = new NarrativeSystem();

/**
 * 人格成长弧光系统（架构不变量 C1-C7）
 * — L1 核心人格从环境变量加载（immutable）
 * — L2 自适应人格定期演进
 * — L3 情境人格实时选择
 */
const personaSystem = new PersonaSystem();

// 每 30 分钟检查一次演进条件（约 1800000ms），测试期间可缩短
const EVOLUTION_INTERVAL_MS = parseInt(
  process.env.PERSONA_EVOLUTION_INTERVAL_MS || '1800000',
  10
);
let evolutionTimer: ReturnType<typeof setInterval> | null = null;

/** 启动演进定时器 */
function startEvolutionTimer(): void {
  if (evolutionTimer) return;
  evolutionTimer = setInterval(() => {
    const result = personaSystem.evolve();
    if (result.changes.length > 0) {
      sendToParent({
        id: crypto.randomUUID(),
        channel: IpcChannel.NOTIFICATION,
        payload: {
          type: 'persona_evolution',
          changes: result.changes,
          rolledBack: result.rolledBack,
        },
        source: ProcessId.LLM_SERVICE,
      });
    }
  }, EVOLUTION_INTERVAL_MS);
}

/** MCP 桥接器（可选，由配置决定是否启用） */
let mcpBridge: McpBridge | null = null;

// ==================== 对话历史（架构不变量 #12） ====================

let conversationHistory: LlmMessage[] = [];

// ==================== 配置 ====================

/** LLM Service 配置 */
interface LlmServiceConfig {
  mcpEnabled: boolean;
  mcpServerPath?: string;
  mcpServerName?: string;
  maxHistoryLength: number;
}

const serviceConfig: LlmServiceConfig = {
  mcpEnabled: process.env.MCP_ENABLED === 'true',
  mcpServerPath: process.env.MCP_SERVER_PATH,
  mcpServerName: process.env.MCP_SERVER_NAME || 'trae-work',
  maxHistoryLength: parseInt(process.env.MAX_HISTORY_LENGTH || '50', 10),
};

// ==================== 初始化 ====================

async function initialize(): Promise<void> {
  try {
    // 初始化 LLM Connector 配置（从环境变量读取 provider 配置）
    setupConnectorProviders();

    // 如果 MCP 启用，初始化 MCP 桥接
    if (serviceConfig.mcpEnabled && serviceConfig.mcpServerPath) {
      await initializeMcpBridge();
    }

    // 注册角色切换回调
    characterManager.onSwitch((card) => {
      // 角色切换时清空对话历史（架构不变量 #12）
      conversationHistory = [];

      // 同时更新情绪推断器的自定义映射
      if (card.emotionActionMap) {
        orchestrator.emotionInferrer.updateEmotionActionMap(card.emotionActionMap);
      } else {
        orchestrator.emotionInferrer.resetToDefaultMap();
      }
    });

    // 启动人格演进定时器（架构不变量 C1-C5）
    startEvolutionTimer();

    // 注册叙事系统到 Expresser（架构不变量 B1/B7）
    // B1: Agent 输出自动写入 L1 原始观察层
    // B7: 叙事子系统故障不影响主管线（Expresser 内部 try-catch）
    orchestrator.expresser.setNarrativeHook(
      (result) => narrativeSystem.recordFromExpression(result),
    );

    // 启动叙事系统定时器（L2 量化 / L3 反思）
    narrativeSystem.start();

    // 发送 SERVICE_READY 信号
    sendToParent({
      id: crypto.randomUUID(),
      channel: IpcChannel.SERVICE_READY,
      payload: { status: 'ok', pid: process.pid },
      source: ProcessId.LLM_SERVICE,
    });
  } catch (error) {
    const message = error instanceof Error ? error.message : '初始化失败';
    sendError('初始化失败', message);
    process.exit(1);
  }
}

/** 从环境变量读取多 provider 配置 */
function setupConnectorProviders(): void {
  const providers: Record<string, { apiKey: string; baseUrl: string; model: string }> = {};

  // 遍历环境变量，查找 LLM_PROVIDER_xxx_APIKEY 模式
  for (const key of Object.keys(process.env)) {
    const match = key.match(/^LLM_PROVIDER_(\w+)_APIKEY$/);
    if (match) {
      const name = match[1].toLowerCase();
      providers[name] = {
        apiKey: process.env[key] || '',
        baseUrl: process.env[`LLM_PROVIDER_${match[1]}_BASEURL`] || '',
        model: process.env[`LLM_PROVIDER_${match[1]}_MODEL`] || 'gpt-4o',
      };
    }
  }

  if (Object.keys(providers).length > 0) {
    connector.setProviders(providers);
  }
}

/** 初始化 MCP 桥接 */
async function initializeMcpBridge(): Promise<void> {
  if (!serviceConfig.mcpServerPath) {
    return;
  }

  mcpBridge = new McpBridge(
    {
      name: serviceConfig.mcpServerName!,
      scriptPath: serviceConfig.mcpServerPath,
      timeout: 30000,
    },
    {
      onStatusChange: (status, message) => {
        sendToParent({
          id: crypto.randomUUID(),
          channel: IpcChannel.NOTIFICATION,
          payload: {
            type: 'mcp_status',
            server: serviceConfig.mcpServerName,
            status,
            message,
          },
          source: ProcessId.LLM_SERVICE,
        });
      },
      onError: (error) => {
        sendToParent({
          id: crypto.randomUUID(),
          channel: IpcChannel.NOTIFICATION,
          payload: {
            type: 'mcp_error',
            server: serviceConfig.mcpServerName,
            error: error.message,
          },
          source: ProcessId.LLM_SERVICE,
        });
      },
    }
  );

  await mcpBridge.connect();
}

// ==================== 消息分发 ====================

/** 分发主进程发来的 IPC 消息 */
async function handleMessage(envelope: IpcEnvelope): Promise<void> {
  const { channel, payload, id } = envelope;

  try {
    switch (channel) {
      case IpcChannel.LLM_CHAT:
        await handleChat(id, payload as { message: string; rolePreset: string; temperature?: number; tools?: string[] });
        break;

      case IpcChannel.LLM_ABORT:
        handleAbort();
        break;

      case IpcChannel.LLM_SWITCH_CHARACTER:
        handleSwitchCharacter(id, payload as { characterName: string });
        break;

      case IpcChannel.CONFIG_CHANGED:
        handleConfigChange(payload as Record<string, unknown>);
        break;

      default:
        sendError(
          id,
          `未知 channel: ${channel}`,
          channel as unknown as IpcChannel
        );
    }
  } catch (error) {
    const message = error instanceof Error ? error.message : '处理消息失败';
    sendError(id, message, channel as unknown as IpcChannel);
  }
}

// ==================== 聊天处理 ====================

async function handleChat(
  requestId: string,
  params: { message: string; rolePreset: string; temperature?: number; tools?: string[] }
): Promise<void> {
  const { message, temperature } = params;

  // 确保角色卡已加载
  let currentCard = characterManager.getCurrentCard();

  if (!currentCard) {
    // 如果还没有角色卡，尝试从角色预设名加载
    if (params.rolePreset) {
      try {
        currentCard = characterManager.loadByName(params.rolePreset);
      } catch {
        sendError(requestId, `未找到角色预设: ${params.rolePreset}`);
        return;
      }
    } else {
      sendError(requestId, '未加载角色卡且未指定 rolePreset');
      return;
    }
  }

  // 记录对话（人格演进计数）
  personaSystem.recordConversation();

  // 架构不变量 A7: 归一化输入为 InputEvent
  const inputEvent: InputEvent = {
    type: 'text',
    payload: message,
    timestamp: Date.now(),
  };

  // 架构不变量 A5: 经过 Perceptor → PersonalityEngine 管线
  const perception = orchestrator.perceptor.perceive(inputEvent);
  personalityEngine.applyEvent({
    type: perception.userEmotion,
    intensity: perception.intensity,
  });

  // 构建 system prompt（架构不变量 #7 — 完全基于角色卡动态生成）
  const systemPrompt = characterManager.generateSystemPrompt();

  // 构建消息列表
  const messages: LlmMessage[] = [
    { role: 'system', content: systemPrompt },
    ...conversationHistory,
    { role: 'user', content: message },
  ];

  // 如果 MCP 可用，获取已注册的工具定义
  let tools: ChatOptions['tools'];
  if (mcpBridge && mcpBridge.getStatus() === 'connected') {
    const mcpTools = mcpBridge.getTools();
    if (mcpTools.length > 0) {
      tools = mcpTools.map((t) => ({
        type: 'function' as const,
        function: {
          name: t.name,
          description: t.description,
          parameters: t.parameters as Record<string, unknown>,
        },
      }));
    }
  }

  // 发送流式响应
  try {
    let fullResponse = '';

    await connector.chatStream(messages, {
      temperature: temperature ?? 0.7,
      tools,
      onStream: (chunk) => {
        fullResponse += chunk;
        // 转发流式块给主进程
        sendToParent({
          id: requestId,
          channel: IpcChannel.LLM_STREAM_CHUNK,
          payload: { text: fullResponse, done: false },
          source: ProcessId.LLM_SERVICE,
        });
      },
    });

    // 架构不变量 A5: 经过 EmotionInferrer → PersonalityEngine → Expresser 管线
    const inferred = orchestrator.emotionInferrer.infer(fullResponse);
    personalityEngine.applyEvent({
      type: 'llm_response',
      emotion: inferred.emotionUpdate.emotion || 'calm',
      intensity: inferred.emotionUpdate.intensity ?? 0.5,
    });
    const filteredVAD = personalityEngine.getAffective();

    // L3 情境人格选择（架构不变量 C6/C7）
    const situationalMod = personaSystem.selectSituational({
      userEmotion: inferred.emotionUpdate.emotion,
      userMessage: message,
      turnNumber: Math.ceil(conversationHistory.length / 2) + 1,
      userValence: filteredVAD.valence,
    });

    const expressed = orchestrator.expresser.express(inferred, filteredVAD, undefined, situationalMod);

    // 发送最终响应（纯文本）
    sendToParent({
      id: requestId,
      channel: IpcChannel.LLM_RESPONSE,
      payload: { text: expressed.text },
      source: ProcessId.LLM_SERVICE,
    });

    // 发送流式结束标记
    sendToParent({
      id: requestId,
      channel: IpcChannel.LLM_STREAM_CHUNK,
      payload: { text: expressed.text, done: true },
      source: ProcessId.LLM_SERVICE,
    });

    // 发送情绪更新（架构不变量 #8/#A1/#A6 — 经管线过滤的 VAD）
    sendToParent({
      id: crypto.randomUUID(),
      channel: IpcChannel.EMOTION_UPDATE,
      payload: expressed.emotionUpdate,
      source: ProcessId.LLM_SERVICE,
    });

    // 更新对话历史（架构不变量 #12）
    conversationHistory.push({ role: 'user', content: message });
    conversationHistory.push({ role: 'assistant', content: expressed.text });

    // 限制历史长度
    if (conversationHistory.length > serviceConfig.maxHistoryLength * 2) {
      conversationHistory = conversationHistory.slice(
        conversationHistory.length - serviceConfig.maxHistoryLength * 2
      );
    }
  } catch (error) {
    if (error instanceof Error && error.name === 'AbortError') {
      // 用户中止生成，不报错
      return;
    }
    throw error;
  }
}

// ==================== 中止处理 ====================

function handleAbort(): void {
  connector.abort();
}

// ==================== 角色切换处理（架构不变量 #12） ====================

function handleSwitchCharacter(
  requestId: string,
  params: { characterName: string }
): void {
  const { characterName } = params;

  try {
    // 加载新角色卡 — onSwitch 回调会自动清空对话历史
    characterManager.loadByName(characterName);

    // 从角色卡 settings 更新人格引擎配置（架构不变量 A1）
    const card = characterManager.getCurrentCard();
    if (card?.settings?.personality) {
      const p = card.settings.personality as Record<string, unknown>;
      if (typeof p.playfulness === 'number') {
        personalityEngine.updateTraits({ playfulness: p.playfulness });
      }
      if (typeof p.empathy === 'number') {
        personalityEngine.updateTraits({ empathy: p.empathy });
      }
    }

    // 回复确认
    sendToParent({
      id: requestId,
      channel: IpcChannel.LLM_RESPONSE,
      payload: {
        text: `已切换到角色: ${card?.name}`,
      },
      source: ProcessId.LLM_SERVICE,
    });
  } catch (error) {
    const message =
      error instanceof Error ? error.message : '角色切换失败';
    sendError(requestId, message);
  }
}

// ==================== 配置变更处理 ====================

function handleConfigChange(config: Record<string, unknown>): void {
  // 更新 LLM Connector 配置
  if (config.apiKey) connector.updateConfig({ apiKey: config.apiKey as string });
  if (config.baseUrl) connector.updateConfig({ baseUrl: config.baseUrl as string });
  if (config.model) connector.updateConfig({ model: config.model as string });

  // 更新 provider 配置
  if (config.providers && typeof config.providers === 'object') {
    connector.setProviders(config.providers as Record<string, { apiKey: string; baseUrl: string; model: string }>);
  }

  // 切换 provider
  if (config.switchProvider && typeof config.switchProvider === 'string') {
    const switched = connector.switchProvider(config.switchProvider);
    if (!switched) {
      sendError(
        crypto.randomUUID(),
        `未找到 provider: ${config.switchProvider}`
      );
    }
  }

  // 更新人格引擎配置（架构不变量 A1）
  if (config.playfulness !== undefined && typeof config.playfulness === 'number') {
    personalityEngine.updateTraits({ playfulness: config.playfulness });
  }
  if (config.empathy !== undefined && typeof config.empathy === 'number') {
    personalityEngine.updateTraits({ empathy: config.empathy });
  }
  if (config.decayIntervalMs !== undefined && config.decayRate !== undefined) {
    personalityEngine.updateDecay(
      config.decayIntervalMs as number,
      config.decayRate as number
    );
  }
}

// ==================== IPC 通信辅助 ====================

/** 向主进程发送消息 */
function sendToParent(envelope: IpcEnvelope): void {
  if (process.send) {
    process.send(envelope);
  }
}

/** 向主进程发送错误 */
function sendError(
  requestId: string,
  errorMessage: string,
  channel?: IpcChannel
): void {
  sendToParent({
    id: requestId,
    channel: channel || (IpcChannel.LLM_RESPONSE as IpcChannel),
    payload: {},
    source: ProcessId.LLM_SERVICE,
    error: errorMessage,
  });
}

// ==================== 进程生命周期 ====================

/** 主进程消息监听 */
process.on(
  'message',
  (envelope: IpcEnvelope) => {
    handleMessage(envelope).catch((error) => {
      const message =
        error instanceof Error ? error.message : '未知错误';
      sendError(envelope.id, message);
    });
  }
);

/** 进程退出清理 */
process.on('exit', () => {
  if (mcpBridge) {
    mcpBridge.disconnect().catch(() => {
      // 忽略断开连接时的错误
    });
  }
  // 架构不变量 A2: 进程退出时停止情感衰减
  personalityEngine.destroy();
  // 停止演进定时器并清理资源
  if (evolutionTimer) {
    clearInterval(evolutionTimer);
    evolutionTimer = null;
  }
  personaSystem.destroy();
  // B7: 叙事子系统清理
  narrativeSystem.destroy();
});

/** 未捕获异常处理 */
process.on('uncaughtException', (error) => {
  sendError(
    crypto.randomUUID(),
    `未捕获异常: ${error.message}`,
    IpcChannel.NOTIFICATION
  );
});

// ==================== 启动 ====================

initialize().catch((error) => {
  const message = error instanceof Error ? error.message : '启动失败';
  console.error(`[LLM Service] ${message}`);
  process.exit(1);
});
