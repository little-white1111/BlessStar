/**
 * 插件系统接口定义（架构不变量 #2、#9）
 * - 插件运行在 Plugin Host 沙箱中，禁止直接访问文件系统和网络
 * - 插件提供标准生命周期接口
 */
/** 插件元数据 */
export interface PluginManifest {
    /** 插件唯一 ID */
    id: string;
    /** 插件名称 */
    name: string;
    /** 版本号 */
    version: string;
    /** 描述 */
    description: string;
    /** 作者 */
    author: string;
    /** 插件所需的能力列表（如 "file:read", "network:fetch"） */
    permissions: string[];
    /** 入口文件路径（相对于插件包根目录） */
    main: string;
}
/** 插件实例必须实现的接口（架构不变量 #9） */
export interface Plugin {
    /** 插件初始化，接收配置参数 */
    init(config: Record<string, unknown>): Promise<void>;
    /** 插件启动，开始监听/提供服务 */
    start(): Promise<void>;
    /** 插件停止，释放资源 */
    stop(): Promise<void>;
    /** 插件销毁，清理所有副作用 */
    destroy(): Promise<void>;
}
/** 插件状态枚举 */
export declare enum PluginState {
    INSTALLED = "installed",
    LOADING = "loading",
    RUNNING = "running",
    STOPPED = "stopped",
    ERROR = "error"
}
/** 插件能力请求类型 */
export declare enum CapabilityRequestType {
    FILE_READ = "file:read",
    FILE_WRITE = "file:write",
    NETWORK_FETCH = "network:fetch",
    SHELL_EXEC = "shell:exec"
}
/** 插件能力请求 */
export interface CapabilityRequest {
    requestId: string;
    pluginId: string;
    type: CapabilityRequestType;
    params: Record<string, unknown>;
}
//# sourceMappingURL=plugin-interface.d.ts.map