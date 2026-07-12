"use strict";
/**
 * 插件系统接口定义（架构不变量 #2、#9）
 * - 插件运行在 Plugin Host 沙箱中，禁止直接访问文件系统和网络
 * - 插件提供标准生命周期接口
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.CapabilityRequestType = exports.PluginState = void 0;
/** 插件状态枚举 */
var PluginState;
(function (PluginState) {
    PluginState["INSTALLED"] = "installed";
    PluginState["LOADING"] = "loading";
    PluginState["RUNNING"] = "running";
    PluginState["STOPPED"] = "stopped";
    PluginState["ERROR"] = "error";
})(PluginState || (exports.PluginState = PluginState = {}));
/** 插件能力请求类型 */
var CapabilityRequestType;
(function (CapabilityRequestType) {
    CapabilityRequestType["FILE_READ"] = "file:read";
    CapabilityRequestType["FILE_WRITE"] = "file:write";
    CapabilityRequestType["NETWORK_FETCH"] = "network:fetch";
    CapabilityRequestType["SHELL_EXEC"] = "shell:exec";
})(CapabilityRequestType || (exports.CapabilityRequestType = CapabilityRequestType = {}));
//# sourceMappingURL=plugin-interface.js.map