"use strict";
/**
 * 插件加载器
 * - 扫描插件目录，读取 PluginManifest
 * - 加载插件入口文件到沙箱
 * - 校验插件完整性（manifest 必须包含 id, name, version, main）
 * - 管理插件列表和状态
 */
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.PluginLoader = void 0;
const fs_1 = __importDefault(require("fs"));
const path_1 = __importDefault(require("path"));
const plugin_interface_1 = require("../shared/plugin-interface");
const sandbox_1 = require("./sandbox");
const lifecycle_manager_1 = require("./lifecycle-manager");
/**
 * 校验 PluginManifest 的完整性
 */
function validateManifest(manifest) {
    const errors = [];
    if (!manifest || typeof manifest !== 'object') {
        return { valid: false, errors: ['manifest 必须是一个对象'] };
    }
    const m = manifest;
    if (!m.id || typeof m.id !== 'string') {
        errors.push('缺少必填字段: id (插件唯一 ID)');
    }
    if (!m.name || typeof m.name !== 'string') {
        errors.push('缺少必填字段: name (插件名称)');
    }
    if (!m.version || typeof m.version !== 'string') {
        errors.push('缺少必填字段: version (版本号)');
    }
    if (!m.main || typeof m.main !== 'string') {
        errors.push('缺少必填字段: main (入口文件路径)');
    }
    if (m.permissions !== undefined && !Array.isArray(m.permissions)) {
        errors.push('permissions 必须是字符串数组');
    }
    return { valid: errors.length === 0, errors };
}
class PluginLoader {
    sandboxes = new Map();
    options;
    lifecycleManager;
    constructor(options) {
        this.options = options;
        this.lifecycleManager = new lifecycle_manager_1.LifecycleManager();
    }
    /**
     * 获取生命周期管理器引用
     */
    getLifecycleManager() {
        return this.lifecycleManager;
    }
    /**
     * 从插件目录读取 manifest.json
     */
    readManifest(pluginDir) {
        const manifestPath = path_1.default.join(pluginDir, 'manifest.json');
        if (!fs_1.default.existsSync(manifestPath)) {
            throw new Error(`插件目录 ${pluginDir} 中未找到 manifest.json`);
        }
        let raw;
        try {
            const content = fs_1.default.readFileSync(manifestPath, 'utf-8');
            raw = JSON.parse(content);
        }
        catch (err) {
            throw new Error(`读取/解析 manifest.json 失败 (${pluginDir}): ${err}`);
        }
        const validation = validateManifest(raw);
        if (!validation.valid) {
            throw new Error(`插件 manifest 校验失败 (${pluginDir}):\n${validation.errors.join('\n')}`);
        }
        const manifest = raw;
        // 确保 permissions 字段存在
        if (!manifest.permissions) {
            manifest.permissions = [];
        }
        return manifest;
    }
    /**
     * 扫描插件目录，返回所有插件 manifest
     */
    scanPlugins() {
        const pluginsDir = this.options.pluginsDir;
        const manifests = [];
        if (!fs_1.default.existsSync(pluginsDir)) {
            return manifests;
        }
        const entries = fs_1.default.readdirSync(pluginsDir, { withFileTypes: true });
        for (const entry of entries) {
            if (!entry.isDirectory()) {
                continue;
            }
            const pluginDir = path_1.default.join(pluginsDir, entry.name);
            try {
                const manifest = this.readManifest(pluginDir);
                manifests.push(manifest);
            }
            catch (err) {
                // 跳过无效插件目录
                console.warn(`跳过无效插件目录 ${pluginDir}:`, err);
            }
        }
        return manifests;
    }
    /**
     * 加载单个插件到沙箱
     */
    async loadPlugin(manifest) {
        // 校验是否已加载
        const existing = this.lifecycleManager.get(manifest.id);
        if (existing && existing.state !== plugin_interface_1.PluginState.STOPPED && existing.state !== plugin_interface_1.PluginState.ERROR) {
            throw new Error(`插件 ${manifest.id} 已加载，当前状态: ${existing.state}。` +
                `请先 stop 或 destroy 后再加载。`);
        }
        // 注册到生命周期管理器
        this.lifecycleManager.register(manifest);
        // 创建沙箱
        const sandbox = new sandbox_1.PluginSandbox(manifest, {
            timeout: this.options.sandboxTimeout,
            permissions: manifest.permissions,
            onCapabilityRequest: async (request) => {
                return this.options.onCapabilityRequest({
                    type: request.type,
                    params: request.params,
                });
            },
        });
        this.sandboxes.set(manifest.id, sandbox);
        try {
            // 读取插件入口文件
            const pluginDir = path_1.default.join(this.options.pluginsDir, manifest.id);
            const mainPath = path_1.default.join(pluginDir, manifest.main);
            if (!fs_1.default.existsSync(mainPath)) {
                throw new Error(`插件入口文件不存在: ${mainPath}`);
            }
            const code = fs_1.default.readFileSync(mainPath, 'utf-8');
            // 在沙箱中加载模块，期望导出 Plugin 接口实现
            const pluginExports = await sandbox.loadModule(code);
            // 验证插件是否正确导出了生命周期接口
            const pluginInstance = pluginExports;
            if (typeof pluginInstance.init !== 'function') {
                throw new Error('插件模块未导出 init 方法');
            }
            if (typeof pluginInstance.start !== 'function') {
                throw new Error('插件模块未导出 start 方法');
            }
            if (typeof pluginInstance.stop !== 'function') {
                throw new Error('插件模块未导出 stop 方法');
            }
            if (typeof pluginInstance.destroy !== 'function') {
                throw new Error('插件模块未导出 destroy 方法');
            }
            // 将插件实例注入到生命周期管理器
            const plugin = this.lifecycleManager.get(manifest.id);
            if (plugin) {
                plugin.instance = pluginInstance;
            }
        }
        catch (err) {
            // 加载失败：清理沙箱，标记为 ERROR
            sandbox.destroy();
            this.sandboxes.delete(manifest.id);
            const plugin = this.lifecycleManager.get(manifest.id);
            if (plugin) {
                plugin.state = plugin_interface_1.PluginState.ERROR;
                plugin.error = err instanceof Error ? err.message : String(err);
            }
            throw err;
        }
    }
    /**
     * 卸载插件（销毁沙箱和实例）
     */
    async unloadPlugin(pluginId) {
        // 先停止
        const plugin = this.lifecycleManager.get(pluginId);
        if (!plugin) {
            throw new Error(`插件 ${pluginId} 未注册`);
        }
        // 如果在运行状态，先停止
        if (plugin.state === plugin_interface_1.PluginState.RUNNING || plugin.state === plugin_interface_1.PluginState.LOADING) {
            await this.lifecycleManager.stop(pluginId);
        }
        // 销毁生命周期实例
        await this.lifecycleManager.destroy(pluginId);
        // 销毁沙箱
        const sandbox = this.sandboxes.get(pluginId);
        if (sandbox) {
            sandbox.destroy();
            this.sandboxes.delete(pluginId);
        }
        // 注销
        this.lifecycleManager.unregister(pluginId);
    }
    /**
     * 获取插件实例的快照（只读状态信息）
     */
    getPluginInfo(pluginId) {
        return this.lifecycleManager.get(pluginId);
    }
    /**
     * 获取所有插件信息
     */
    getAllPlugins() {
        return this.lifecycleManager.getAll();
    }
    /**
     * 获取指定插件的沙箱（供生命周期管理器调用）
     */
    getSandbox(pluginId) {
        return this.sandboxes.get(pluginId);
    }
    /**
     * 销毁加载器（清理所有资源）
     */
    async destroy() {
        await this.lifecycleManager.destroyAll();
        for (const [pluginId, sandbox] of this.sandboxes) {
            sandbox.destroy();
        }
        this.sandboxes.clear();
    }
}
exports.PluginLoader = PluginLoader;
//# sourceMappingURL=plugin-loader.js.map