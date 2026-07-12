"use strict";
/**
 * 沙箱环境（架构不变量 #2）
 * - 插件运行在沙箱中，禁止直接访问文件系统和网络
 * - 使用 Node.js vm 模块创建隔离上下文
 * - 限制插件只能通过 IPC 请求文件/网络能力
 * - 设置执行超时（默认 30s）
 */
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.PluginSandbox = void 0;
const vm_1 = __importDefault(require("vm"));
const plugin_interface_1 = require("../shared/plugin-interface");
/** 沙箱安全 API 集合 */
const SAFE_GLOBALS = {
    // 基本类型工具
    console: {
        log: (...args) => {
            // 将插件日志发送到主进程（通过 process.send 在外部处理）
            if (typeof process !== 'undefined' && process.send) {
                process.send({ type: 'plugin:log', args });
            }
        },
        warn: (...args) => {
            if (typeof process !== 'undefined' && process.send) {
                process.send({ type: 'plugin:log', level: 'warn', args });
            }
        },
        error: (...args) => {
            if (typeof process !== 'undefined' && process.send) {
                process.send({ type: 'plugin:log', level: 'error', args });
            }
        },
    },
    JSON: JSON,
    Math: Math,
    Date: Date,
    RegExp: RegExp,
    String: String,
    Number: Number,
    Boolean: Boolean,
    Array: Array,
    Object: Object,
    Map: Map,
    Set: Set,
    Promise: Promise,
    parseInt: parseInt,
    parseFloat: parseFloat,
    isNaN: isNaN,
    isFinite: isFinite,
    encodeURI: encodeURI,
    encodeURIComponent: encodeURIComponent,
    decodeURI: decodeURI,
    decodeURIComponent: decodeURIComponent,
    Error: Error,
    TypeError: TypeError,
    RangeError: RangeError,
    SyntaxError: SyntaxError,
    ReferenceError: ReferenceError,
};
/** 沙箱上下文中的能力请求函数，挂载为全局 __requestCapability */
function createCapabilityRequestFn(onCapabilityRequest) {
    return async (type, params) => {
        const capabilityType = type;
        // 校验类型是否合法
        const validTypes = Object.values(plugin_interface_1.CapabilityRequestType);
        if (!validTypes.includes(capabilityType)) {
            throw new Error(`未知的能力类型: ${type}。允许的类型: ${validTypes.join(', ')}`);
        }
        return onCapabilityRequest({ type: capabilityType, params });
    };
}
const DEFAULT_TIMEOUT = 30000; // 30 秒
class PluginSandbox {
    context;
    timeout;
    destroyed = false;
    constructor(manifest, options) {
        this.timeout = options.timeout ?? DEFAULT_TIMEOUT;
        // 构建沙箱上下文：安全全局对象 + 插件元数据 + 能力请求接口
        const sandboxGlobals = {
            ...SAFE_GLOBALS,
            __plugin_manifest: {
                id: manifest.id,
                name: manifest.name,
                version: manifest.version,
                permissions: manifest.permissions,
            },
            __requestCapability: createCapabilityRequestFn(options.onCapabilityRequest),
        };
        this.context = vm_1.default.createContext(sandboxGlobals);
    }
    /**
     * 在沙箱中执行插件代码
     * @param code 插件 JavaScript 代码
     * @param context 额外的上下文变量（注入到沙箱中）
     * @returns 执行结果
     */
    async execute(code, context) {
        if (this.destroyed) {
            throw new Error('沙箱已销毁，无法执行代码');
        }
        // 如果有额外上下文，注入到沙箱
        if (context) {
            for (const [key, value] of Object.entries(context)) {
                this.context[key] = value;
            }
        }
        const script = new vm_1.default.Script(`(function() { ${code} })()`, {
            filename: 'plugin-sandbox',
            lineOffset: 0,
        });
        const result = script.runInContext(this.context, {
            timeout: this.timeout,
            breakOnSigint: true,
        });
        return result;
    }
    /**
     * 在沙箱中执行插件模块代码，并导出 module.exports
     * 插件代码使用 CommonJS 风格包装
     */
    async loadModule(code) {
        if (this.destroyed) {
            throw new Error('沙箱已销毁，无法加载模块');
        }
        // 构造一个沙箱中的 module 和 exports
        const moduleObj = { exports: {} };
        const exportsObj = moduleObj.exports;
        this.context.module = moduleObj;
        this.context.exports = exportsObj;
        // 包装代码使其像 CommonJS 模块一样运行
        const wrappedCode = `
      (function(module, exports) {
        ${code}
      })(module, exports);
    `;
        const script = new vm_1.default.Script(wrappedCode, {
            filename: 'plugin-module',
            lineOffset: 0,
        });
        script.runInContext(this.context, {
            timeout: this.timeout,
            breakOnSigint: true,
        });
        return moduleObj.exports;
    }
    /**
     * 销毁沙箱，清理所有引用
     */
    destroy() {
        this.destroyed = true;
        // 释放上下文引用
        const keys = Object.keys(this.context);
        for (const key of keys) {
            delete this.context[key];
        }
    }
    /**
     * 获取沙箱上下文（谨慎使用，仅用于注入运行时依赖）
     */
    getContext() {
        return this.context;
    }
}
exports.PluginSandbox = PluginSandbox;
//# sourceMappingURL=sandbox.js.map