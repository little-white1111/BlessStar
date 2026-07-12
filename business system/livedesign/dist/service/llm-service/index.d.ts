/**
 * LLM Service 子进程入口
 * 通过 child_process.fork 启动，使用 process.on('message') / process.send() 与主进程通信
 *
 * 架构不变量遵守：
 *   #7  — 角色卡是唯一真理源
 *   #8  — 情绪推断结果必须同步输出 emotion + action 参数
 *   #11 — MCP 桥接作为独立通信层
 *   #12 — 收到 LLM_SWITCH_CHARACTER 时清空对话历史
 */
export {};
//# sourceMappingURL=index.d.ts.map