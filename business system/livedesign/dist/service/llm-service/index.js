"use strict";
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
Object.defineProperty(exports, "__esModule", { value: true });
const ipc_protocol_1 = require("../shared/ipc-protocol");
const llm_connector_1 = require("./llm-connector");
const character_manager_1 = require("./character-manager");
const emotion_inferrer_1 = require("./emotion-inferrer");
const mcp_bridge_1 = require("./mcp-bridge");
// ==================== 模块实例 ====================
/** LLM API 连接器 */
const connector = new llm_connector_1.LlmConnector({
    apiKey: process.env.LLM_API_KEY || '',
    baseUrl: process.env.LLM_BASE_URL || 'https://api.openai.com/v1',
    model: process.env.LLM_MODEL || 'gpt-4o',
});
/** 角色卡管理器 — 从 resources/characters/ 目录加载 */
const characterManager = new character_manager_1.CharacterManager(process.env.CHARACTERS_DIR || './resources/characters');
/** 情绪推断器 */
const emotionInferrer = new emotion_inferrer_1.EmotionInferrer();
/** MCP 桥接器（可选，由配置决定是否启用） */
let mcpBridge = null;
// ==================== 对话历史（架构不变量 #12） ====================
let conversationHistory = [];
const serviceConfig = {
    mcpEnabled: process.env.MCP_ENABLED === 'true',
    mcpServerPath: process.env.MCP_SERVER_PATH,
    mcpServerName: process.env.MCP_SERVER_NAME || 'trae-work',
    maxHistoryLength: parseInt(process.env.MAX_HISTORY_LENGTH || '50', 10),
};
// ==================== 初始化 ====================
async function initialize() {
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
                emotionInferrer.updateEmotionActionMap(card.emotionActionMap);
            }
            else {
                emotionInferrer.resetToDefaultMap();
            }
        });
        // 发送 SERVICE_READY 信号
        sendToParent({
            id: crypto.randomUUID(),
            channel: ipc_protocol_1.IpcChannel.SERVICE_READY,
            payload: { status: 'ok', pid: process.pid },
            source: ipc_protocol_1.ProcessId.LLM_SERVICE,
        });
    }
    catch (error) {
        const message = error instanceof Error ? error.message : '初始化失败';
        sendError('初始化失败', message);
        process.exit(1);
    }
}
/** 从环境变量读取多 provider 配置 */
function setupConnectorProviders() {
    const providers = {};
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
async function initializeMcpBridge() {
    if (!serviceConfig.mcpServerPath) {
        return;
    }
    mcpBridge = new mcp_bridge_1.McpBridge({
        name: serviceConfig.mcpServerName,
        scriptPath: serviceConfig.mcpServerPath,
        timeout: 30000,
    }, {
        onStatusChange: (status, message) => {
            sendToParent({
                id: crypto.randomUUID(),
                channel: ipc_protocol_1.IpcChannel.NOTIFICATION,
                payload: {
                    type: 'mcp_status',
                    server: serviceConfig.mcpServerName,
                    status,
                    message,
                },
                source: ipc_protocol_1.ProcessId.LLM_SERVICE,
            });
        },
        onError: (error) => {
            sendToParent({
                id: crypto.randomUUID(),
                channel: ipc_protocol_1.IpcChannel.NOTIFICATION,
                payload: {
                    type: 'mcp_error',
                    server: serviceConfig.mcpServerName,
                    error: error.message,
                },
                source: ipc_protocol_1.ProcessId.LLM_SERVICE,
            });
        },
    });
    await mcpBridge.connect();
}
// ==================== 消息分发 ====================
/** 分发主进程发来的 IPC 消息 */
async function handleMessage(envelope) {
    const { channel, payload, id } = envelope;
    try {
        switch (channel) {
            case ipc_protocol_1.IpcChannel.LLM_CHAT:
                await handleChat(id, payload);
                break;
            case ipc_protocol_1.IpcChannel.LLM_ABORT:
                handleAbort();
                break;
            case ipc_protocol_1.IpcChannel.LLM_SWITCH_CHARACTER:
                handleSwitchCharacter(id, payload);
                break;
            case ipc_protocol_1.IpcChannel.CONFIG_CHANGED:
                handleConfigChange(payload);
                break;
            default:
                sendError(id, `未知 channel: ${channel}`, channel);
        }
    }
    catch (error) {
        const message = error instanceof Error ? error.message : '处理消息失败';
        sendError(id, message, channel);
    }
}
// ==================== 聊天处理 ====================
async function handleChat(requestId, params) {
    const { message, temperature } = params;
    // 确保角色卡已加载
    let currentCard = characterManager.getCurrentCard();
    if (!currentCard) {
        // 如果还没有角色卡，尝试从角色预设名加载
        if (params.rolePreset) {
            try {
                currentCard = characterManager.loadByName(params.rolePreset);
            }
            catch {
                sendError(requestId, `未找到角色预设: ${params.rolePreset}`);
                return;
            }
        }
        else {
            sendError(requestId, '未加载角色卡且未指定 rolePreset');
            return;
        }
    }
    // 构建 system prompt（架构不变量 #7 — 完全基于角色卡动态生成）
    const systemPrompt = characterManager.generateSystemPrompt();
    // 构建消息列表
    const messages = [
        { role: 'system', content: systemPrompt },
        ...conversationHistory,
        { role: 'user', content: message },
    ];
    // 如果 MCP 可用，获取已注册的工具定义
    let tools;
    if (mcpBridge && mcpBridge.getStatus() === 'connected') {
        const mcpTools = mcpBridge.getTools();
        if (mcpTools.length > 0) {
            tools = mcpTools.map((t) => ({
                type: 'function',
                function: {
                    name: t.name,
                    description: t.description,
                    parameters: t.parameters,
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
                    channel: ipc_protocol_1.IpcChannel.LLM_STREAM_CHUNK,
                    payload: { text: fullResponse, done: false },
                    source: ipc_protocol_1.ProcessId.LLM_SERVICE,
                });
            },
        });
        // 推断情绪（架构不变量 #8）
        const inferred = emotionInferrer.infer(fullResponse);
        // 发送最终响应（纯文本）
        sendToParent({
            id: requestId,
            channel: ipc_protocol_1.IpcChannel.LLM_RESPONSE,
            payload: { text: inferred.text },
            source: ipc_protocol_1.ProcessId.LLM_SERVICE,
        });
        // 发送流式结束标记
        sendToParent({
            id: requestId,
            channel: ipc_protocol_1.IpcChannel.LLM_STREAM_CHUNK,
            payload: { text: inferred.text, done: true },
            source: ipc_protocol_1.ProcessId.LLM_SERVICE,
        });
        // 发送情绪更新（架构不变量 #8 — emotion + action 同步输出）
        sendToParent({
            id: crypto.randomUUID(),
            channel: ipc_protocol_1.IpcChannel.EMOTION_UPDATE,
            payload: inferred.emotionUpdate,
            source: ipc_protocol_1.ProcessId.LLM_SERVICE,
        });
        // 更新对话历史（架构不变量 #12）
        conversationHistory.push({ role: 'user', content: message });
        conversationHistory.push({ role: 'assistant', content: inferred.text });
        // 限制历史长度
        if (conversationHistory.length > serviceConfig.maxHistoryLength * 2) {
            conversationHistory = conversationHistory.slice(conversationHistory.length - serviceConfig.maxHistoryLength * 2);
        }
    }
    catch (error) {
        if (error instanceof Error && error.name === 'AbortError') {
            // 用户中止生成，不报错
            return;
        }
        throw error;
    }
}
// ==================== 中止处理 ====================
function handleAbort() {
    connector.abort();
}
// ==================== 角色切换处理（架构不变量 #12） ====================
function handleSwitchCharacter(requestId, params) {
    const { characterName } = params;
    try {
        // 加载新角色卡 — onSwitch 回调会自动清空对话历史
        characterManager.loadByName(characterName);
        // 回复确认
        sendToParent({
            id: requestId,
            channel: ipc_protocol_1.IpcChannel.LLM_RESPONSE,
            payload: {
                text: `已切换到角色: ${characterManager.getCurrentCard()?.name}`,
            },
            source: ipc_protocol_1.ProcessId.LLM_SERVICE,
        });
    }
    catch (error) {
        const message = error instanceof Error ? error.message : '角色切换失败';
        sendError(requestId, message);
    }
}
// ==================== 配置变更处理 ====================
function handleConfigChange(config) {
    // 更新 LLM Connector 配置
    if (config.apiKey)
        connector.updateConfig({ apiKey: config.apiKey });
    if (config.baseUrl)
        connector.updateConfig({ baseUrl: config.baseUrl });
    if (config.model)
        connector.updateConfig({ model: config.model });
    // 更新 provider 配置
    if (config.providers && typeof config.providers === 'object') {
        connector.setProviders(config.providers);
    }
    // 切换 provider
    if (config.switchProvider && typeof config.switchProvider === 'string') {
        const switched = connector.switchProvider(config.switchProvider);
        if (!switched) {
            sendError(crypto.randomUUID(), `未找到 provider: ${config.switchProvider}`);
        }
    }
}
// ==================== IPC 通信辅助 ====================
/** 向主进程发送消息 */
function sendToParent(envelope) {
    if (process.send) {
        process.send(envelope);
    }
}
/** 向主进程发送错误 */
function sendError(requestId, errorMessage, channel) {
    sendToParent({
        id: requestId,
        channel: channel || ipc_protocol_1.IpcChannel.LLM_RESPONSE,
        payload: {},
        source: ipc_protocol_1.ProcessId.LLM_SERVICE,
        error: errorMessage,
    });
}
// ==================== 进程生命周期 ====================
/** 主进程消息监听 */
process.on('message', (envelope) => {
    handleMessage(envelope).catch((error) => {
        const message = error instanceof Error ? error.message : '未知错误';
        sendError(envelope.id, message);
    });
});
/** 进程退出清理 */
process.on('exit', () => {
    if (mcpBridge) {
        mcpBridge.disconnect().catch(() => {
            // 忽略断开连接时的错误
        });
    }
});
/** 未捕获异常处理 */
process.on('uncaughtException', (error) => {
    sendError(crypto.randomUUID(), `未捕获异常: ${error.message}`, ipc_protocol_1.IpcChannel.NOTIFICATION);
});
// ==================== 启动 ====================
initialize().catch((error) => {
    const message = error instanceof Error ? error.message : '启动失败';
    console.error(`[LLM Service] ${message}`);
    process.exit(1);
});
//# sourceMappingURL=index.js.map