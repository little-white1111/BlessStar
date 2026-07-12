/**
 * 生命周期管理（架构不变量 #9）
 * - 实现标准生命周期：init(config) → start() → stop() → destroy()
 * - 状态机管理：INSTALLED → LOADING → RUNNING → STOPPED / ERROR
 * - 异常处理：启动失败时的状态回滚
 */
import { Plugin, PluginManifest, PluginState } from '../shared/plugin-interface';
/** 插件实例包装：包含元数据、状态和运行时实例 */
export interface PluginInstance {
    manifest: PluginManifest;
    state: PluginState;
    instance: Plugin | null;
    error?: string;
}
export declare class LifecycleManager {
    private plugins;
    /**
     * 注册一个插件实例到生命周期管理器
     */
    register(manifest: PluginManifest): void;
    /**
     * 获取插件实例
     */
    get(pluginId: string): PluginInstance | undefined;
    /**
     * 获取所有插件实例
     */
    getAll(): PluginInstance[];
    /**
     * 状态转换，校验合法性
     */
    private transitionTo;
    /**
     * init(config) — 初始化插件，传入配置
     */
    init(pluginId: string, config: Record<string, unknown>): Promise<void>;
    /**
     * start() — 启动插件
     */
    start(pluginId: string): Promise<void>;
    /**
     * stop() — 停止插件
     */
    stop(pluginId: string): Promise<void>;
    /**
     * destroy() — 销毁插件，清理所有副作用
     */
    destroy(pluginId: string): Promise<void>;
    /**
     * 销毁所有插件
     */
    destroyAll(): Promise<void>;
    /**
     * 注销插件（从管理器中移除）
     */
    unregister(pluginId: string): void;
}
//# sourceMappingURL=lifecycle-manager.d.ts.map