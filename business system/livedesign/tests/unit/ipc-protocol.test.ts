/**
 * IPC 协议单元测试
 * 测试 IpcEnvelope、ProcessId、IpcChannel 的定义和行为
 */
import { describe, it, expect } from 'vitest';
import {
  IpcEnvelope,
  IpcChannel,
  ProcessId,
  PendingRequest,
} from '../../src/shared/ipc-protocol';

describe('ProcessId 枚举', () => {
  it('应包含所有必需的进程标识', () => {
    expect(ProcessId.MAIN).toBe('main');
    expect(ProcessId.RENDERER).toBe('renderer');
    expect(ProcessId.LLM_SERVICE).toBe('llm-service');
    expect(ProcessId.PLUGIN_HOST).toBe('plugin-host');
  });

  it('枚举值应唯一', () => {
    const values = Object.values(ProcessId);
    const uniqueValues = new Set(values);
    expect(uniqueValues.size).toBe(values.length);
  });
});

describe('IpcChannel 枚举', () => {
  it('应包含 LLM 相关通道', () => {
    expect(IpcChannel.LLM_CHAT).toBe('llm:chat');
    expect(IpcChannel.LLM_STREAM_CHUNK).toBe('llm:stream:chunk');
    expect(IpcChannel.LLM_RESPONSE).toBe('llm:response');
    expect(IpcChannel.LLM_ABORT).toBe('llm:abort');
  });

  it('应包含情绪更新通道', () => {
    expect(IpcChannel.EMOTION_UPDATE).toBe('emotion:update');
  });

  it('应包含插件相关通道', () => {
    expect(IpcChannel.PLUGIN_INVOKE).toBe('plugin:invoke');
    expect(IpcChannel.PLUGIN_STOP).toBe('plugin:stop');
    expect(IpcChannel.PLUGIN_RESULT).toBe('plugin:result');
    expect(IpcChannel.PLUGIN_REQUEST_FILE).toBe('plugin:request:file');
    expect(IpcChannel.PLUGIN_REQUEST_NETWORK).toBe('plugin:request:network');
    expect(IpcChannel.PLUGIN_APPROVAL_RESULT).toBe('plugin:approval:result');
  });

  it('应包含系统广播通道', () => {
    expect(IpcChannel.CONFIG_CHANGED).toBe('config:changed');
    expect(IpcChannel.NOTIFICATION).toBe('notification:show');
  });

  it('应包含进程管理通道', () => {
    expect(IpcChannel.LLM_SWITCH_CHARACTER).toBe('llm:switch:character');
    expect(IpcChannel.SERVICE_READY).toBe('service:ready');
    expect(IpcChannel.PLUGIN_HOST_READY).toBe('plugin-host:ready');
  });

  it('枚举值应唯一', () => {
    const values = Object.values(IpcChannel);
    const uniqueValues = new Set(values);
    expect(uniqueValues.size).toBe(values.length);
  });
});

describe('IpcEnvelope 接口', () => {
  it('应能创建一个有效的 IpcEnvelope 对象', () => {
    const envelope: IpcEnvelope = {
      id: 'msg-001',
      channel: IpcChannel.LLM_CHAT,
      payload: { message: '你好' },
      source: ProcessId.RENDERER,
    };

    expect(envelope.id).toBe('msg-001');
    expect(envelope.channel).toBe(IpcChannel.LLM_CHAT);
    expect(envelope.payload).toEqual({ message: '你好' });
    expect(envelope.source).toBe(ProcessId.RENDERER);
  });

  it('应能序列化为 JSON 并反序列化回原对象', () => {
    const envelope: IpcEnvelope = {
      id: 'msg-002',
      channel: IpcChannel.EMOTION_UPDATE,
      payload: { emotion: 'happy', action: 'smile', intensity: 0.8 },
      source: ProcessId.LLM_SERVICE,
    };

    const json = JSON.stringify(envelope);
    const parsed = JSON.parse(json) as IpcEnvelope;

    expect(parsed.id).toBe('msg-002');
    expect(parsed.channel).toBe(IpcChannel.EMOTION_UPDATE);
    expect(parsed.payload).toEqual({ emotion: 'happy', action: 'smile', intensity: 0.8 });
    expect(parsed.source).toBe(ProcessId.LLM_SERVICE);
  });

  it('应支持不带 source 字段的信封（来源由 Router 注入）', () => {
    const envelope: IpcEnvelope = {
      id: 'msg-003',
      channel: IpcChannel.LLM_CHAT,
      payload: 'test',
    };

    expect(envelope.source).toBeUndefined();
  });

  it('应支持 error 字段表示错误消息', () => {
    const envelope: IpcEnvelope = {
      id: 'msg-004',
      channel: IpcChannel.LLM_RESPONSE,
      payload: null,
      error: 'LLM 服务不可用',
    };

    expect(envelope.error).toBe('LLM 服务不可用');
  });
});

describe('PendingRequest 接口', () => {
  it('应能创建 PendingRequest 对象', () => {
    const resolve = (value: unknown) => value;
    const reject = (reason: unknown) => reason;
    const timeout = setTimeout(() => {}, 1000);

    const pending: PendingRequest = { resolve, reject, timeout };

    expect(typeof pending.resolve).toBe('function');
    expect(typeof pending.reject).toBe('function');
    expect(typeof pending.timeout).toBe('object');

    clearTimeout(timeout);
  });
});
