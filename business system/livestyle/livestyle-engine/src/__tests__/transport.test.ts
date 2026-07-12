/**
 * Transport 层测试
 * 覆盖 TransportAdapter 接口契约 + PostMessageTransport 实现
 * 使用 jsdom 环境模拟浏览器 API
 */

import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { PostMessageTransport } from '../bridge/transport/PostMessageTransport';
import { createTransport, detectTransportType } from '../bridge/transport/TransportFactory';

// ============================================================
// PostMessageTransport 实现测试
// ============================================================
describe('PostMessageTransport', () => {
  let mockTargetWindow: Window;
  let transport: PostMessageTransport;

  beforeEach(() => {
    vi.useFakeTimers();
    mockTargetWindow = {
      postMessage: vi.fn(),
    } as unknown as Window;
    transport = new PostMessageTransport({
      targetWindow: mockTargetWindow,
      targetOrigin: '*',
      timeoutMs: 1000,
      maxRetries: 2,
      retryIntervalMs: 100,
    });
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  // ---- 连接状态 ----
  it('connect() 应将状态从 disconnected 变为 connecting', () => {
    expect(transport.status).toBe('disconnected');
    transport.connect();
    expect(transport.status).toBe('connecting');
  });

  it('connect() 在已连接时应保持 connected 状态', () => {
    transport.connect();
    transport.confirmConnected();
    expect(transport.status).toBe('connected');

    // 再次 connect 不应改变状态
    transport.connect();
    expect(transport.status).toBe('connected');
  });

  it('confirmConnected() 应将状态变为 connected', () => {
    transport.connect();
    transport.confirmConnected();
    expect(transport.status).toBe('connected');
  });

  // ---- send() ----
  it('send() 在未连接时应 console.warn 且不调用 postMessage', () => {
    const warnSpy = vi.spyOn(console, 'warn').mockImplementation(() => {});
    transport.send('hello');
    expect(warnSpy).toHaveBeenCalled();
    expect(mockTargetWindow.postMessage).not.toHaveBeenCalled();
    warnSpy.mockRestore();
  });

  it('send() 在已连接时应调用 targetWindow.postMessage', () => {
    transport.connect();
    transport.confirmConnected();
    transport.send('hello');
    expect(mockTargetWindow.postMessage).toHaveBeenCalledWith('hello', '*');
  });

  // ---- disconnect() ----
  it('disconnect() 应清理事件监听并将状态设为 disconnected', () => {
    const removeSpy = vi.spyOn(window, 'removeEventListener');
    transport.connect();
    transport.disconnect();
    expect(transport.status).toBe('disconnected');
    expect(removeSpy).toHaveBeenCalled();
    removeSpy.mockRestore();
  });

  // ---- 超时逻辑 ----
  it('connect() 后超过 timeoutMs 未收到确认应触发超时错误', () => {
    const warnSpy = vi.spyOn(console, 'warn').mockImplementation(() => {});
    transport.connect();
    expect(transport.status).toBe('connecting');

    // 快进到超时触发（maxRetries=2，第一次超时进入 reconnecting）
    vi.advanceTimersByTime(1000);
    expect(transport.status).toBe('reconnecting');
    expect(warnSpy).toHaveBeenCalledWith(
      '[PostMessageTransport] 连接错误:',
      '连接超时',
    );
    warnSpy.mockRestore();
  });

  // ---- 重试逻辑 ----
  it('maxRetries 次重试后仍未连接应进入 disconnected', () => {
    const warnSpy = vi.spyOn(console, 'warn').mockImplementation(() => {});
    transport.connect(); // 第1次 connecting
    // 每次超时后重试间隔 retryIntervalMs(100) 触发一次重连，
    // 重连后再过 timeoutMs(1000) 再次超时
    // maxRetries=2，总共尝试 3 次后进入 disconnected

    // 第1次超时 → reconnecting
    vi.advanceTimersByTime(1000);
    expect(transport.status).toBe('reconnecting');

    // 重试间隔后自动重连 → connecting
    vi.advanceTimersByTime(100);
    expect(transport.status).toBe('connecting');

    // 第2次超时 → reconnecting
    vi.advanceTimersByTime(1000);
    expect(transport.status).toBe('reconnecting');

    // 重试间隔后自动重连 → connecting
    vi.advanceTimersByTime(100);
    expect(transport.status).toBe('connecting');

    // 第3次超时 → maxRetries exhausted → disconnected
    vi.advanceTimersByTime(1000);
    expect(transport.status).toBe('disconnected');
    warnSpy.mockRestore();
  });

  // ---- onStatusChange 回调 ----
  it('onStatusChange 应在状态变更时触发', () => {
    const statusChanges: string[] = [];
    transport.onStatusChange((status) => statusChanges.push(status));

    transport.connect();
    transport.confirmConnected();
    transport.disconnect();

    // 预期：connecting → connected → disconnected
    expect(statusChanges).toEqual(['connecting', 'connected', 'disconnected']);
  });

  // ---- confirmConnected 重置重试计数 ----
  it('confirmConnected 应重置 retryCount', () => {
    const warnSpy = vi.spyOn(console, 'warn').mockImplementation(() => {});
    transport.connect();
    // 触发一次超时，retryCount = 1
    vi.advanceTimersByTime(1000);
    expect(transport.status).toBe('reconnecting');

    // 重试后收到确认
    vi.advanceTimersByTime(100);
    transport.confirmConnected();
    expect(transport.status).toBe('connected');

    // 断开后重新连接，确认 retryCount 已被重置（0）
    transport.disconnect();
    transport.connect();
    transport.confirmConnected();
    expect(transport.status).toBe('connected');
    warnSpy.mockRestore();
  });

  // ---- onMessage ----
  it('收到消息时应调用注册的 messageHandler', () => {
    transport.connect();
    const handler = vi.fn();
    transport.onMessage(handler);

    // 模拟收到一条消息
    const event = new MessageEvent('message', {
      data: 'test-message',
      origin: window.location.origin,
    });
    window.dispatchEvent(event);

    expect(handler).toHaveBeenCalledWith('test-message');
  });

  it('收到连接确认消息时应调用 confirmConnected', () => {
    transport.connect();
    expect(transport.status).toBe('connecting');

    const event = new MessageEvent('message', {
      data: '__LIVESTYLE_CONNECTED__',
      origin: window.location.origin,
    });
    window.dispatchEvent(event);

    expect(transport.status).toBe('connected');
  });
});

// ============================================================
// TransportFactory 测试
// ============================================================
describe('TransportFactory', () => {
  it('createTransport("postmessage") 应返回 PostMessageTransport', () => {
    const transport = createTransport({
      type: 'postmessage',
      iframeWindow: { postMessage: vi.fn() } as unknown as Window,
      targetOrigin: '*',
    });
    expect(transport).toBeInstanceOf(PostMessageTransport);
  });

  it('createTransport("postmessage") 缺少 iframeWindow 应抛出错误', () => {
    expect(() =>
      createTransport({
        type: 'postmessage',
      } as any),
    ).toThrow('iframeWindow');
  });

  it('createTransport 传入未知类型应抛出错误', () => {
    expect(() =>
      createTransport({ type: 'unknown' as any }),
    ).toThrow('未知的传输类型');
  });
});

// ============================================================
// detectTransportType 测试
// ============================================================
describe('detectTransportType', () => {
  const originalSharedWorker = (globalThis as any).SharedWorker;
  const originalWorker = (globalThis as any).Worker;

  afterEach(() => {
    (globalThis as any).SharedWorker = originalSharedWorker;
    (globalThis as any).Worker = originalWorker;
  });

  it('SharedWorker 可用时应返回 sharedworker', () => {
    (globalThis as any).SharedWorker = class MockSharedWorker {};
    expect(detectTransportType()).toBe('sharedworker');
  });

  it('仅 Worker 可用时应返回 worker', () => {
    (globalThis as any).SharedWorker = undefined;
    (globalThis as any).Worker = class MockWorker {} as any;
    expect(detectTransportType()).toBe('worker');
  });

  it('Worker 也不可用时应返回 postmessage', () => {
    (globalThis as any).SharedWorker = undefined;
    (globalThis as any).Worker = undefined;
    expect(detectTransportType()).toBe('postmessage');
  });
});
