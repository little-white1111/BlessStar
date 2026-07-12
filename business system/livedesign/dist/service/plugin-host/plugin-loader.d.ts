/**
 * 插件加载器
 * - 扫描插件目录，读取 PluginManifest
 * - 加载插件入口文件到沙箱
 * - 校验插件完整性（manifest 必须包含 id, name, version, main）
 * - 管理插件列表和状态
 */
import { PluginManifest } from '../shared/plugin-interface';
import { PluginSandbox } from './sandbox';
import { LifecycleManager, PluginInstance } from './lifecycle-manager';
/** 插件加载选项 */
export interface PluginLoaderOptions {
    /** 插件扫描根目录 */
    pluginsDir: string;
    /** 沙箱执行超时（毫秒，默认 30000） */
    sandboxTimeout?: number;
    /** 能力请求回调 */
    onCapabilityRequest: (request: {
        type: string;
        params: Record<string, unknown>;
    }) => Promise<unknown>;
}
export declare class PluginLoader {
    private sandboxes;
    private options;
    private lifecycleManager;
    constructor(options: PluginLoaderOptions);
    /**
     * 获取生命周期管理器引用
     */
    getLifecycleManager(): LifecycleManager;
    /**
     * 从插件目录读取 manifest.json
     */
    readManifest(pluginDir: string): PluginManifest;
    /**
     * 扫描插件目录，返回所有插件 manifest
     */
    scanPlugins(): PluginManifest[];
    /**
     * 加载单个插件到沙箱
     */
    loadPlugin(manifest: PluginManifest): Promise<void>;
    /**
     * 卸载插件（销毁沙箱和实例）
     */
    unloadPlugin(pluginId: string): Promise<void>;
    /**
     * 获取插件实例的快照（只读状态信息）
     */
    getPluginInfo(pluginId: string): PluginInstance | undefined;
    /**
     * 获取所有插件信息
     */
    getAllPlugins(): PluginInstance[];
    /**
     * 获取指定插件的沙箱（供生命周期管理器调用）
     */
    getSandbox(pluginId: string): PluginSandbox | undefined;
    /**
     * 销毁加载器（清理所有资源）
     */
    destroy(): Promise<void>;
}
//# sourceMappingURL=plugin-loader.d.ts.map