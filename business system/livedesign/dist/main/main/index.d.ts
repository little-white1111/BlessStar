/**
 * 主进程入口
 *
 * 架构不变量：
 *   #1  — IPC Router 是唯一消息路由 — 全局 ipcRouter 单例
 *   #4  — LLM Service 崩溃不影响 UI — ProcessManager 自动重启
 *   #5  — 所有用户数据仅存储在本地 — electron-store + better-sqlite3
 *   #10 — 悬浮球位置变更实时持久化 — WindowManager.persistWindowBounds
 *
 * 启动流程：
 *   1. app.whenReady()
 *   2. 初始化配置存储 (configStore)
 *   3. 初始化 IPC Router (ipcRouter)
 *   4. 创建主窗口 (windowManager)
 *   5. 初始化原生桥接 (nativeBridge)
 *   6. 启动系统托盘
 *   7. 启动子进程 (LLM Service, Plugin Host)
 *   8. 加载当前角色
 */
export {};
//# sourceMappingURL=index.d.ts.map