"use strict";
/**
 * IPC 协议定义（架构不变量 #1 的实现基础）
 * 所有进程间通信必须通过主进程 IPC 路由，
 * 使用此协议定义的消息格式。
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.IpcChannel = exports.ProcessId = void 0;
/** 进程标识枚举 */
var ProcessId;
(function (ProcessId) {
    ProcessId["MAIN"] = "main";
    ProcessId["RENDERER"] = "renderer";
    ProcessId["LLM_SERVICE"] = "llm-service";
    ProcessId["PLUGIN_HOST"] = "plugin-host";
})(ProcessId || (exports.ProcessId = ProcessId = {}));
/** IPC 消息通道枚举 */
var IpcChannel;
(function (IpcChannel) {
    // ============ Renderer ↔ LLM Service (经主进程路由) ============
    /** Renderer → LLM Service: 发送聊天消息 */
    IpcChannel["LLM_CHAT"] = "llm:chat";
    /** LLM Service → Renderer: 流式响应块 */
    IpcChannel["LLM_STREAM_CHUNK"] = "llm:stream:chunk";
    /** LLM Service → Renderer: 完整响应结束 */
    IpcChannel["LLM_RESPONSE"] = "llm:response";
    /** Renderer → LLM Service: 中止当前生成 */
    IpcChannel["LLM_ABORT"] = "llm:abort";
    // ============ Renderer ↔ LLM Service: 情绪更新 ============
    /** LLM Service → Renderer: 情绪+动作参数更新 */
    IpcChannel["EMOTION_UPDATE"] = "emotion:update";
    // ============ Renderer ↔ Plugin Host (经主进程路由) ============
    /** Renderer → Plugin Host: 调用插件方法 */
    IpcChannel["PLUGIN_INVOKE"] = "plugin:invoke";
    /** Renderer → Plugin Host: 停止插件执行 */
    IpcChannel["PLUGIN_STOP"] = "plugin:stop";
    /** Plugin Host → Renderer: 插件调用结果 */
    IpcChannel["PLUGIN_RESULT"] = "plugin:result";
    // ============ Plugin Host → Main Process: 能力请求 ============
    /** Plugin Host → Main: 请求读取文件 */
    IpcChannel["PLUGIN_REQUEST_FILE"] = "plugin:request:file";
    /** Plugin Host → Main: 请求网络访问 */
    IpcChannel["PLUGIN_REQUEST_NETWORK"] = "plugin:request:network";
    /** Main → Plugin Host: 能力请求审批结果 */
    IpcChannel["PLUGIN_APPROVAL_RESULT"] = "plugin:approval:result";
    // ============ 主进程 → 通用广播 ============
    /** 配置变更通知 */
    IpcChannel["CONFIG_CHANGED"] = "config:changed";
    /** 系统通知 */
    IpcChannel["NOTIFICATION"] = "notification:show";
    // ============ 子进程管理 ============
    /** Main → LLM Service: 切换角色 */
    IpcChannel["LLM_SWITCH_CHARACTER"] = "llm:switch:character";
    /** LLM Service → Main: 进程就绪 */
    IpcChannel["SERVICE_READY"] = "service:ready";
    /** Plugin Host → Main: 进程就绪 */
    IpcChannel["PLUGIN_HOST_READY"] = "plugin-host:ready";
})(IpcChannel || (exports.IpcChannel = IpcChannel = {}));
//# sourceMappingURL=ipc-protocol.js.map