"use strict";
/**
 * 生命周期管理（架构不变量 #9）
 * - 实现标准生命周期：init(config) → start() → stop() → destroy()
 * - 状态机管理：INSTALLED → LOADING → RUNNING → STOPPED / ERROR
 * - 异常处理：启动失败时的状态回滚
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.LifecycleManager = void 0;
const plugin_interface_1 = require("../shared/plugin-interface");
/** 生命周期状态转换矩阵 */
const VALID_TRANSITIONS = {
    [plugin_interface_1.PluginState.INSTALLED]: [plugin_interface_1.PluginState.LOADING, plugin_interface_1.PluginState.ERROR],
    [plugin_interface_1.PluginState.LOADING]: [plugin_interface_1.PluginState.RUNNING, plugin_interface_1.PluginState.ERROR, plugin_interface_1.PluginState.STOPPED],
    [plugin_interface_1.PluginState.RUNNING]: [plugin_interface_1.PluginState.STOPPED, plugin_interface_1.PluginState.ERROR],
    [plugin_interface_1.PluginState.STOPPED]: [plugin_interface_1.PluginState.LOADING, plugin_interface_1.PluginState.INSTALLED],
    [plugin_interface_1.PluginState.ERROR]: [plugin_interface_1.PluginState.INSTALLED, plugin_interface_1.PluginState.LOADING],
};
class LifecycleManager {
    plugins = new Map();
    /**
     * 注册一个插件实例到生命周期管理器
     */
    register(manifest) {
        if (this.plugins.has(manifest.id)) {
            throw new Error(`插件 ${manifest.id} 已注册`);
        }
        const pluginInstance = {
            manifest,
            state: plugin_interface_1.PluginState.INSTALLED,
            instance: null,
        };
        this.plugins.set(manifest.id, pluginInstance);
    }
    /**
     * 获取插件实例
     */
    get(pluginId) {
        return this.plugins.get(pluginId);
    }
    /**
     * 获取所有插件实例
     */
    getAll() {
        return Array.from(this.plugins.values());
    }
    /**
     * 状态转换，校验合法性
     */
    transitionTo(pluginId, newState) {
        const plugin = this.plugins.get(pluginId);
        if (!plugin) {
            throw new Error(`插件 ${pluginId} 未注册`);
        }
        const currentState = plugin.state;
        const allowedTransitions = VALID_TRANSITIONS[currentState];
        if (!allowedTransitions.includes(newState)) {
            throw new Error(`非法的状态转换: ${currentState} → ${newState}（插件: ${pluginId}）。` +
                `允许的目标状态: ${allowedTransitions.join(', ')}`);
        }
        plugin.state = newState;
    }
    /**
     * init(config) — 初始化插件，传入配置
     */
    async init(pluginId, config) {
        const plugin = this.plugins.get(pluginId);
        if (!plugin) {
            throw new Error(`插件 ${pluginId} 未注册`);
        }
        if (plugin.state !== plugin_interface_1.PluginState.INSTALLED) {
            throw new Error(`插件 ${pluginId} 当前状态为 ${plugin.state}，` +
                `需要 INSTALLED 状态才能 init`);
        }
        if (!plugin.instance) {
            throw new Error(`插件 ${pluginId} 实例未加载`);
        }
        this.transitionTo(pluginId, plugin_interface_1.PluginState.LOADING);
        try {
            await plugin.instance.init(config);
            // 初始化为 LOADING 状态，由 start() 进入 RUNNING
        }
        catch (err) {
            const errorMessage = err instanceof Error ? err.message : String(err);
            plugin.state = plugin_interface_1.PluginState.ERROR;
            plugin.error = `初始化失败: ${errorMessage}`;
            throw err;
        }
    }
    /**
     * start() — 启动插件
     */
    async start(pluginId) {
        const plugin = this.plugins.get(pluginId);
        if (!plugin) {
            throw new Error(`插件 ${pluginId} 未注册`);
        }
        if (plugin.state !== plugin_interface_1.PluginState.LOADING) {
            throw new Error(`插件 ${pluginId} 当前状态为 ${plugin.state}，` +
                `需要 LOADING 状态才能 start`);
        }
        if (!plugin.instance) {
            throw new Error(`插件 ${pluginId} 实例未加载`);
        }
        try {
            await plugin.instance.start();
            this.transitionTo(pluginId, plugin_interface_1.PluginState.RUNNING);
        }
        catch (err) {
            const errorMessage = err instanceof Error ? err.message : String(err);
            // 启动失败：回滚到 STOPPED 状态
            plugin.state = plugin_interface_1.PluginState.STOPPED;
            plugin.error = `启动失败: ${errorMessage}`;
            throw err;
        }
    }
    /**
     * stop() — 停止插件
     */
    async stop(pluginId) {
        const plugin = this.plugins.get(pluginId);
        if (!plugin) {
            throw new Error(`插件 ${pluginId} 未注册`);
        }
        if (plugin.state !== plugin_interface_1.PluginState.RUNNING && plugin.state !== plugin_interface_1.PluginState.LOADING) {
            throw new Error(`插件 ${pluginId} 当前状态为 ${plugin.state}，` +
                `需要 RUNNING 或 LOADING 状态才能 stop`);
        }
        if (!plugin.instance) {
            // 没有实例但需要停止，直接切换状态
            this.transitionTo(pluginId, plugin_interface_1.PluginState.STOPPED);
            return;
        }
        try {
            await plugin.instance.stop();
        }
        catch (err) {
            // stop 失败仍然标记为 STOPPED，由 destroy 清理
            plugin.error = `停止时发生错误: ${err instanceof Error ? err.message : String(err)}`;
        }
        this.transitionTo(pluginId, plugin_interface_1.PluginState.STOPPED);
    }
    /**
     * destroy() — 销毁插件，清理所有副作用
     */
    async destroy(pluginId) {
        const plugin = this.plugins.get(pluginId);
        if (!plugin) {
            throw new Error(`插件 ${pluginId} 未注册`);
        }
        if (plugin.instance) {
            try {
                await plugin.instance.destroy();
            }
            catch (err) {
                plugin.error = `销毁时发生错误: ${err instanceof Error ? err.message : String(err)}`;
            }
        }
        plugin.instance = null;
        plugin.state = plugin_interface_1.PluginState.INSTALLED;
    }
    /**
     * 销毁所有插件
     */
    async destroyAll() {
        const errors = [];
        for (const [pluginId] of this.plugins) {
            try {
                await this.destroy(pluginId);
            }
            catch (err) {
                errors.push({
                    pluginId,
                    error: err instanceof Error ? err.message : String(err),
                });
            }
        }
        if (errors.length > 0) {
            throw new Error(`部分插件销毁失败:\n${errors.map((e) => `  ${e.pluginId}: ${e.error}`).join('\n')}`);
        }
    }
    /**
     * 注销插件（从管理器中移除）
     */
    unregister(pluginId) {
        const plugin = this.plugins.get(pluginId);
        if (!plugin) {
            throw new Error(`插件 ${pluginId} 未注册`);
        }
        if (plugin.instance) {
            throw new Error(`插件 ${pluginId} 仍有活跃实例，请先调用 destroy`);
        }
        this.plugins.delete(pluginId);
    }
}
exports.LifecycleManager = LifecycleManager;
//# sourceMappingURL=lifecycle-manager.js.map