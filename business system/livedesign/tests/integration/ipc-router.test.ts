/**
 * IPC 路由集成测试
 * 模拟 Renderer 和 LLM Service 进程，测试 IpcRouter 的消息转发、请求-响应和超时
 */
import { describe, it, expect, beforeEach, afterEach, vi } from 'vitest';
import { IpcRouter } from '../../src/main/ipc-router';
import { IpcEnvelope, IpcChannel, ProcessId } from '../../src/shared/ipc-protocol';

/**
 * 创建模拟的 IpcEnvelope
 */
function createEnvelope(
  id: string,
  channel: IpcChannel,
  payload: unknown,
  source?: ProcessId,
): IpcEnvelope {
  return { id, channel, payload, source };
}

describe('IpcRouter 集成测试', () => {
  let router: IpcRouter;

  beforeEach(() => {
    router = new IpcRouter();
  });

  afterEach(() => {
    router.dispose();
  });

  describe('消息路由转发', () => {
    it('应正确转发 Renderer 的 LLM_CHAT 消息到 LLM Service', () => {
      const sendFn = vi.fn();
      router.registerChildProcessSender(ProcessId.LLM_SERVICE, sendFn);

      const envelope = createEnvelope('req-1', IpcChannel.LLM_CHAT, {
        message: '你好',
        rolePreset: '助手',
      });

      // 模拟 Renderer 消息
      const mockEvent = {} as Electron.IpcMainEvent;
      router.handleRendererMessage(mockEvent, envelope);

      // 验证 LLM Service 收到消息
      expect(sendFn).toHaveBeenCalledTimes(1);
      const sentEnvelope = sendFn.mock.calls[0][0] as IpcEnvelope;
      expect(sentEnvelope.channel).toBe(IpcChannel.LLM_CHAT);
      expect(sentEnvelope.source).toBe(ProcessId.RENDERER);
      expect(sentEnvelope.id).toBe('req-1');
    });

    it('应正确转发子进程的 LLM_STREAM_CHUNK 到 Renderer', () => {
      // 设置主窗口（模拟）
      const mockWebContents = { send: vi.fn() };
      const mockWin = {
        webContents: mockWebContents,
        isDestroyed: () => false,
      } as any;
      router.setMainWindow(mockWin);

      const envelope = createEnvelope('stream-1', IpcChannel.LLM_STREAM_CHUNK, {
        text: '你好',
        done: false,
      }, ProcessId.LLM_SERVICE);

      router.handleChildProcessMessage(ProcessId.LLM_SERVICE, envelope);

      // 验证 Renderer 收到消息
      expect(mockWebContents.send).toHaveBeenCalledWith('ipc-message', envelope);
    });

    it('应正确转发 Renderer 的 PLUGIN_INVOKE 消息到 Plugin Host', () => {
      const sendFn = vi.fn();
      router.registerChildProcessSender(ProcessId.PLUGIN_HOST, sendFn);

      const envelope = createEnvelope('req-2', IpcChannel.PLUGIN_INVOKE, {
        pluginId: 'test-plugin',
        method: 'doSomething',
        args: [],
      });

      const mockEvent = {} as Electron.IpcMainEvent;
      router.handleRendererMessage(mockEvent, envelope);

      expect(sendFn).toHaveBeenCalledTimes(1);
      const sent = sendFn.mock.calls[0][0] as IpcEnvelope;
      expect(sent.channel).toBe(IpcChannel.PLUGIN_INVOKE);
    });

    it('未知通道的消息应被拒绝', () => {
      const sendFn = vi.fn();
      router.registerChildProcessSender(ProcessId.LLM_SERVICE, sendFn);

      const envelope = createEnvelope('req-3', 'unknown:channel' as IpcChannel, {});

      const mockEvent = {} as Electron.IpcMainEvent;
      // 不应抛出异常，应在内部处理
      expect(() => router.handleRendererMessage(mockEvent, envelope)).not.toThrow();
      expect(sendFn).not.toHaveBeenCalled();
    });

    it('无效来源的进程注册应被拒绝', () => {
      expect(() => router.registerChildProcess(ProcessId.MAIN, null)).toThrow('不允许注册核心进程');
      expect(() => router.registerChildProcess(ProcessId.RENDERER, null)).toThrow('不允许注册核心进程');
      expect(() => router.registerChildProcessSender(ProcessId.MAIN, vi.fn())).toThrow('不允许注册核心进程');
      expect(() => router.registerChildProcessSender(ProcessId.RENDERER, vi.fn())).toThrow('不允许注册核心进程');
    });
  });

  describe('请求-响应配对', () => {
    it('应正确配对请求和响应', async () => {
      const sendFn = vi.fn();
      router.registerChildProcessSender(ProcessId.LLM_SERVICE, sendFn);

      // 发送请求
      const responsePromise = router.send(
        ProcessId.LLM_SERVICE,
        IpcChannel.LLM_CHAT,
        { message: '你好', rolePreset: '助手' },
      );

      // 验证请求已发送
      expect(sendFn).toHaveBeenCalledTimes(1);
      const sentEnvelope = sendFn.mock.calls[0][0] as IpcEnvelope;

      // 模拟 LLM Service 返回响应
      const responseEnvelope = createEnvelope(
        sentEnvelope.id,
        IpcChannel.LLM_RESPONSE,
        { text: '你好！有什么可以帮你的？' },
        ProcessId.LLM_SERVICE,
      );

      router.handleChildProcessMessage(ProcessId.LLM_SERVICE, responseEnvelope);

      // 验证响应正确解析
      const result = await responsePromise;
      expect(result).toEqual({ text: '你好！有什么可以帮你的？' });
    });

    it('错误响应应被 reject', async () => {
      const sendFn = vi.fn();
      router.registerChildProcessSender(ProcessId.LLM_SERVICE, sendFn);

      const responsePromise = router.send(
        ProcessId.LLM_SERVICE,
        IpcChannel.LLM_CHAT,
        { message: 'test', rolePreset: '助手' },
      );

      const sentEnvelope = sendFn.mock.calls[0][0] as IpcEnvelope;

      // 模拟 LLM Service 返回错误
      const errorEnvelope = createEnvelope(
        sentEnvelope.id,
        IpcChannel.LLM_RESPONSE,
        null,
        ProcessId.LLM_SERVICE,
      );
      errorEnvelope.error = 'LLM 服务超时';

      router.handleChildProcessMessage(ProcessId.LLM_SERVICE, errorEnvelope);

      await expect(responsePromise).rejects.toThrow('LLM 服务超时');
    });
  });

  describe('超时机制', () => {
    it('超时未收到响应时应 reject', async () => {
      const sendFn = vi.fn();
      router.registerChildProcessSender(ProcessId.LLM_SERVICE, sendFn);

      // 使用极短超时
      const responsePromise = router.send(
        ProcessId.LLM_SERVICE,
        IpcChannel.LLM_CHAT,
        { message: 'test', rolePreset: '助手' },
        10, // 10ms 超时
      );

      // 不发送响应，等待超时
      await expect(responsePromise).rejects.toThrow('消息超时');
    }, 1000); // 测试本身超时设为 1s

    it('超时后收到迟到的响应应被忽略', async () => {
      const sendFn = vi.fn();
      router.registerChildProcessSender(ProcessId.LLM_SERVICE, sendFn);

      const responsePromise = router.send(
        ProcessId.LLM_SERVICE,
        IpcChannel.LLM_CHAT,
        { message: 'test', rolePreset: '助手' },
        10, // 10ms 超时
      );

      // 等待超时
      await expect(responsePromise).rejects.toThrow('消息超时');

      const sentEnvelope = sendFn.mock.calls[0][0] as IpcEnvelope;

      // 延迟后发送响应
      const lateResponse = createEnvelope(
        sentEnvelope.id,
        IpcChannel.LLM_RESPONSE,
        { text: '迟到的响应' },
        ProcessId.LLM_SERVICE,
      );

      // 不应抛出异常
      expect(() => {
        router.handleChildProcessMessage(ProcessId.LLM_SERVICE, lateResponse);
      }).not.toThrow();
    });
  });

  describe('dispose 清理', () => {
    it('dispose 应清理所有 pending request', async () => {
      const sendFn = vi.fn();
      router.registerChildProcessSender(ProcessId.LLM_SERVICE, sendFn);

      const responsePromise = router.send(
        ProcessId.LLM_SERVICE,
        IpcChannel.LLM_CHAT,
        { message: 'test', rolePreset: '助手' },
        5000,
      );

      router.dispose();

      await expect(responsePromise).rejects.toThrow('路由器已关闭');
    });

    it('dispose 后应能重新注册', () => {
      router.dispose();

      const sendFn = vi.fn();
      expect(() => {
        router.registerChildProcessSender(ProcessId.LLM_SERVICE, sendFn);
      }).not.toThrow();
    });
  });
});
