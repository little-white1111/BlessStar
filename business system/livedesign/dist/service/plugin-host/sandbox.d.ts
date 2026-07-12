/**
 * 沙箱环境（架构不变量 #2）
 * - 插件运行在沙箱中，禁止直接访问文件系统和网络
 * - 使用 Node.js vm 模块创建隔离上下文
 * - 限制插件只能通过 IPC 请求文件/网络能力
 * - 设置执行超时（默认 30s）
 */
import vm from 'vm';
import { PluginManifest, CapabilityRequestType } from '../shared/plugin-interface';
/** 沙箱配置选项 */
export interface SandboxOptions {
    /** 脚本执行超时时间（毫秒，默认 30000） */
    timeout?: number;
    /** 插件声明所需的能力列表 */
    permissions: string[];
    /** 能力请求回调：插件请求文件/网络能力时调用，返回审批结果 */
    onCapabilityRequest: (request: {
        type: CapabilityRequestType;
        params: Record<string, unknown>;
    }) => Promise<unknown>;
}
export declare class PluginSandbox {
    private context;
    private timeout;
    private destroyed;
    constructor(manifest: PluginManifest, options: SandboxOptions);
    /**
     * 在沙箱中执行插件代码
     * @param code 插件 JavaScript 代码
     * @param context 额外的上下文变量（注入到沙箱中）
     * @returns 执行结果
     */
    execute<T>(code: string, context?: Record<string, unknown>): Promise<T>;
    /**
     * 在沙箱中执行插件模块代码，并导出 module.exports
     * 插件代码使用 CommonJS 风格包装
     */
    loadModule<T>(code: string): Promise<T>;
    /**
     * 销毁沙箱，清理所有引用
     */
    destroy(): void;
    /**
     * 获取沙箱上下文（谨慎使用，仅用于注入运行时依赖）
     */
    getContext(): vm.Context;
}
//# sourceMappingURL=sandbox.d.ts.map