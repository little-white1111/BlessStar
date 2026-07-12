/**
 * Plugin Host 子进程入口
 * - 作为 Node.js 子进程运行（通过 child_process.fork）
 * - 通过 process.on('message') / process.send() 与主进程通信
 * - 监听主进程转发的 IpcEnvelope
 * - 转发 PLUGIN_INVOKE 到 plugin-loader
 * - 管理插件状态
 * - 发送 PLUGIN_HOST_READY 信号
 */
/** Plugin Host 配置 */
interface PluginHostConfig {
    /** 插件扫描根目录 */
    pluginsDir: string;
    /** 沙箱超时（毫秒） */
    sandboxTimeout?: number;
}
declare class PluginHostProcess {
    private loader;
    private initialized;
    /**
     * 初始化 Plugin Host：加载所有插件
     */
    initialize(config: PluginHostConfig): Promise<void>;
    /**
     * 发送 PLUGIN_HOST_READY 信号
     */
    private sendReady;
    /**
     * 通过 IPC 向主进程发送能力请求
     */
    private sendCapabilityRequest;
    /**
     * 处理主进程转发的 IpcEnvelope
     */
    private handleMessage;
    /**
     * 处理插件调用请求
     */
    private handlePluginInvoke;
    /**
     * 处理插件停止请求
     */
    private handlePluginStop;
    /**
     * 处理配置变更通知
     */
    private handleConfigChanged;
    /**
     * 通过 process.send 发送消息到主进程
     */
    private send;
    /**
     * 启动消息监听
     */
    startListening(config: PluginHostConfig): void;
    /**
     * 优雅关闭
     */
    shutdown(): Promise<void>;
}
export { PluginHostProcess, PluginHostConfig };
//# sourceMappingURL=index.d.ts.map