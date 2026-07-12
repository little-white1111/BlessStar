/**
 * Plugin Host 子进程入口
 * - 作为 Node.js 子进程运行（通过 child_process.fork）
 * - 通过 process.on('message') / process.send() 与主进程通信
 * - 监听主进程转发的 IpcEnvelope
 * - 转发 PLUGIN_INVOKE 到 plugin-loader
 * - 管理插件状态
 * - 发送 PLUGIN_HOST_READY 信号
 */

import path from 'path';
import {
  IpcChannel,
  IpcEnvelope,
  PluginInvokePayload,
  ProcessId,
} from '../shared/ipc-protocol';
import { PluginState } from '../shared/plugin-interface';
import { PluginLoader } from './plugin-loader';

/** Plugin Host 配置 */
interface PluginHostConfig {
  /** 插件扫描根目录 */
  pluginsDir: string;
  /** 沙箱超时（毫秒） */
  sandboxTimeout?: number;
}

class PluginHostProcess {
  private loader: PluginLoader | null = null;
  private initialized = false;

  /**
   * 初始化 Plugin Host：加载所有插件
   */
  async initialize(config: PluginHostConfig): Promise<void> {
    if (this.initialized) {
      throw new Error('Plugin Host 已初始化');
    }

    // 解析插件目录路径（相对于进程工作目录）
    const pluginsDir = path.resolve(config.pluginsDir);

    this.loader = new PluginLoader({
      pluginsDir,
      sandboxTimeout: config.sandboxTimeout ?? 30000,
      onCapabilityRequest: async (request) => {
        // 能力请求通过 IPC 转发给主进程审批
        return this.sendCapabilityRequest(request);
      },
    });

    // 扫描并加载所有插件
    const manifests = this.loader.scanPlugins();
    for (const manifest of manifests) {
      try {
        await this.loader.loadPlugin(manifest);
        console.log(`插件加载成功: ${manifest.id}@${manifest.version}`);

        // 自动 init + start
        const lifecycle = this.loader.getLifecycleManager();
        await lifecycle.init(manifest.id, {});
        await lifecycle.start(manifest.id);
        console.log(`插件启动成功: ${manifest.id}`);
      } catch (err) {
        console.error(`插件加载/启动失败: ${manifest.id}`, err);
      }
    }

    this.initialized = true;

    // 发送 PLUGIN_HOST_READY 信号给主进程
    this.sendReady();
  }

  /**
   * 发送 PLUGIN_HOST_READY 信号
   */
  private sendReady(): void {
    const envelope: IpcEnvelope = {
      id: `ready-${Date.now()}`,
      channel: IpcChannel.PLUGIN_HOST_READY,
      payload: {
        processId: ProcessId.PLUGIN_HOST,
        timestamp: Date.now(),
        plugins: this.loader?.getAllPlugins().map((p) => ({
          id: p.manifest.id,
          name: p.manifest.name,
          version: p.manifest.version,
          state: p.state,
        })),
      },
      source: ProcessId.PLUGIN_HOST,
    };

    this.send(envelope);
  }

  /**
   * 通过 IPC 向主进程发送能力请求
   */
  private async sendCapabilityRequest(request: {
    type: string;
    params: Record<string, unknown>;
  }): Promise<unknown> {
    const channel =
      request.type.startsWith('file:')
        ? IpcChannel.PLUGIN_REQUEST_FILE
        : IpcChannel.PLUGIN_REQUEST_NETWORK;

    const requestId = `cap-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`;

    const envelope: IpcEnvelope = {
      id: requestId,
      channel,
      payload: {
        requestId,
        type: request.type,
        params: request.params,
      },
      source: ProcessId.PLUGIN_HOST,
    };

    this.send(envelope);

    // 等待主进程审批结果（通过消息监听）
    return new Promise((resolve, reject) => {
      const handler = (msg: IpcEnvelope) => {
        if (
          msg.channel === IpcChannel.PLUGIN_APPROVAL_RESULT &&
          (msg.payload as { requestId?: string })?.requestId === requestId
        ) {
          process.removeListener('message', handler);

          if (msg.error) {
            reject(new Error(msg.error));
          } else {
            resolve(msg.payload);
          }
        }
      };

      process.on('message', handler);

      // 超时处理（默认 60 秒）
      const timeout = setTimeout(() => {
        process.removeListener('message', handler);
        reject(new Error(`能力请求超时: ${request.type}`));
      }, 60000);

      // 确保超时定时器在收到响应时被清理
      const originalHandler = handler;
      const wrappedHandler = (msg: IpcEnvelope) => {
        if (
          msg.channel === IpcChannel.PLUGIN_APPROVAL_RESULT &&
          (msg.payload as { requestId?: string })?.requestId === requestId
        ) {
          clearTimeout(timeout);
          originalHandler(msg);
        }
      };

      // 替换为带超时清理的 handler
      process.removeListener('message', handler);
      process.on('message', wrappedHandler);
    });
  }

  /**
   * 处理主进程转发的 IpcEnvelope
   */
  private handleMessage(envelope: IpcEnvelope): void {
    if (!this.loader) {
      console.error('Plugin Host 未初始化，无法处理消息');
      return;
    }

    switch (envelope.channel) {
      case IpcChannel.PLUGIN_INVOKE: {
        this.handlePluginInvoke(envelope);
        break;
      }
      case IpcChannel.PLUGIN_STOP: {
        this.handlePluginStop(envelope);
        break;
      }
      case IpcChannel.CONFIG_CHANGED: {
        this.handleConfigChanged(envelope);
        break;
      }
      default:
        console.warn(`未处理的 IPC 通道: ${envelope.channel}`);
    }
  }

  /**
   * 处理插件调用请求
   */
  private async handlePluginInvoke(envelope: IpcEnvelope): Promise<void> {
    const payload = envelope.payload as PluginInvokePayload;
    const { pluginId, method, args } = payload;

    try {
      const lifecycle = this.loader!.getLifecycleManager();
      const plugin = lifecycle.get(pluginId);

      if (!plugin) {
        throw new Error(`插件 ${pluginId} 未找到`);
      }

      if (plugin.state !== PluginState.RUNNING) {
        throw new Error(
          `插件 ${pluginId} 当前状态为 ${plugin.state}，` +
          `需要 RUNNING 状态才能调用方法`,
        );
      }

      if (!plugin.instance) {
        throw new Error(`插件 ${pluginId} 实例为空`);
      }

      // 调用插件实例的方法
      const pluginObj = plugin.instance as unknown as Record<string, unknown>;
      const fn = pluginObj[method];

      if (typeof fn !== 'function') {
        throw new Error(`插件 ${pluginId} 没有可调用的方法: ${method}`);
      }

      const result = await (fn as (...args: unknown[]) => unknown)(...(args || []));

      // 发送结果回主进程
      const response: IpcEnvelope = {
        id: envelope.id,
        channel: IpcChannel.PLUGIN_RESULT,
        payload: {
          pluginId,
          method,
          result,
        },
        source: ProcessId.PLUGIN_HOST,
      };

      this.send(response);
    } catch (err) {
      const errorMessage = err instanceof Error ? err.message : String(err);

      const errorResponse: IpcEnvelope = {
        id: envelope.id,
        channel: IpcChannel.PLUGIN_RESULT,
        payload: {
          pluginId,
          method,
          result: null,
        },
        error: errorMessage,
        source: ProcessId.PLUGIN_HOST,
      };

      this.send(errorResponse);
    }
  }

  /**
   * 处理插件停止请求
   */
  private async handlePluginStop(envelope: IpcEnvelope): Promise<void> {
    const pluginId = envelope.payload as string;

    try {
      const lifecycle = this.loader!.getLifecycleManager();

      if (lifecycle.get(pluginId)?.state === PluginState.RUNNING) {
        await lifecycle.stop(pluginId);
      }

      // 发送确认
      const response: IpcEnvelope = {
        id: envelope.id,
        channel: IpcChannel.PLUGIN_RESULT,
        payload: {
          pluginId,
          stopped: true,
        },
        source: ProcessId.PLUGIN_HOST,
      };

      this.send(response);
    } catch (err) {
      const errorMessage = err instanceof Error ? err.message : String(err);

      const errorResponse: IpcEnvelope = {
        id: envelope.id,
        channel: IpcChannel.PLUGIN_RESULT,
        payload: {
          pluginId,
          stopped: false,
        },
        error: errorMessage,
        source: ProcessId.PLUGIN_HOST,
      };

      this.send(errorResponse);
    }
  }

  /**
   * 处理配置变更通知
   */
  private async handleConfigChanged(envelope: IpcEnvelope): Promise<void> {
    const { pluginId, config } = envelope.payload as {
      pluginId?: string;
      config: Record<string, unknown>;
    };

    const lifecycle = this.loader!.getLifecycleManager();

    if (pluginId) {
      // 通知指定插件
      const plugin = lifecycle.get(pluginId);
      if (plugin?.instance && typeof (plugin.instance as unknown as Record<string, unknown>).onConfigChanged === 'function') {
        try {
          await ((plugin.instance as unknown as Record<string, unknown>).onConfigChanged as (
            config: Record<string, unknown>,
          ) => Promise<void>)(config);
        } catch (err) {
          console.error(`插件 ${pluginId} 配置更新失败:`, err);
        }
      }
    } else {
      // 通知所有插件
      for (const p of lifecycle.getAll()) {
        if (
          p.instance &&
          typeof (p.instance as unknown as Record<string, unknown>).onConfigChanged === 'function'
        ) {
          try {
            await ((p.instance as unknown as Record<string, unknown>).onConfigChanged as (
              config: Record<string, unknown>,
            ) => Promise<void>)(config);
          } catch (err) {
            console.error(`插件 ${p.manifest.id} 配置更新失败:`, err);
          }
        }
      }
    }
  }

  /**
   * 通过 process.send 发送消息到主进程
   */
  private send(message: IpcEnvelope): void {
    if (process.send) {
      process.send(message);
    } else {
      console.error('process.send 不可用，无法发送消息到主进程');
    }
  }

  /**
   * 启动消息监听
   */
  startListening(config: PluginHostConfig): void {
    // 监听主进程消息
    process.on('message', (msg: IpcEnvelope) => {
      this.handleMessage(msg);
    });

    // 初始化
    this.initialize(config).catch((err) => {
      console.error('Plugin Host 初始化失败:', err);
      process.exit(1);
    });

    // 优雅退出
    process.on('SIGTERM', () => {
      this.shutdown();
    });

    process.on('SIGINT', () => {
      this.shutdown();
    });

    // 未捕获异常处理
    process.on('uncaughtException', (err) => {
      console.error('Plugin Host 未捕获异常:', err);
    });
  }

  /**
   * 优雅关闭
   */
  async shutdown(): Promise<void> {
    console.log('Plugin Host 正在关闭...');

    if (this.loader) {
      try {
        await this.loader.destroy();
      } catch (err) {
        console.error('清理资源时出错:', err);
      }
    }

    process.exit(0);
  }
}

// ============ 启动逻辑 ============

// 从环境变量读取配置（主进程通过 fork 传递）
const config: PluginHostConfig = {
  pluginsDir: process.env.PLUGINS_DIR || path.join(__dirname, '..', '..', 'plugins'),
  sandboxTimeout: process.env.SANDBOX_TIMEOUT
    ? parseInt(process.env.SANDBOX_TIMEOUT, 10)
    : 30000,
};

const host = new PluginHostProcess();
host.startListening(config);

// 导出以便测试
export { PluginHostProcess, PluginHostConfig };
