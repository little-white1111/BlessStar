import { ref, onUnmounted } from 'vue';
import { IpcChannel } from '../../shared/ipc-protocol';
import type {
  LlmChatPayload,
  LlmStreamChunkPayload,
  EmotionUpdatePayload,
} from '../../shared/ipc-protocol';
import type { CharacterCard } from '../../shared/character-card';

export interface ChatMessage {
  id: string;
  role: 'user' | 'assistant';
  content: string;
  isStreaming: boolean;
  timestamp: number;
}

/** 简易 ID 生成 */
function generateId(): string {
  return `msg_${Date.now()}_${Math.random().toString(36).slice(2, 8)}`;
}

export function useChat() {
  const messages = ref<ChatMessage[]>([]);
  const isGenerating = ref(false);
  const streamingMessageId = ref<string | null>(null);

  // 流式响应回调引用，用于移除监听
  let onStreamChunk: ((event: unknown, payload: LlmStreamChunkPayload) => void) | null = null;
  let onResponse: ((event: unknown) => void) | null = null;
  let onEmotionUpdate: ((event: unknown, payload: EmotionUpdatePayload) => void) | null = null;

  /** 发送消息到 LLM Service（经主进程路由） */
  function sendMessage(content: string, character: CharacterCard): void {
    if (!content.trim() || isGenerating.value) return;

    // 添加用户消息
    const userMsg: ChatMessage = {
      id: generateId(),
      role: 'user',
      content: content.trim(),
      isStreaming: false,
      timestamp: Date.now(),
    };
    messages.value.push(userMsg);

    // 创建占位的流式响应消息
    const assistantId = generateId();
    const assistantMsg: ChatMessage = {
      id: assistantId,
      role: 'assistant',
      content: '',
      isStreaming: true,
      timestamp: Date.now(),
    };
    messages.value.push(assistantMsg);
    streamingMessageId.value = assistantId;
    isGenerating.value = true;

    // 构建聊天载荷
    const payload: LlmChatPayload = {
      message: content.trim(),
      rolePreset: character.description + '\n' + character.style,
      temperature: undefined,
      tools: undefined,
    };

    // 注册 IPC 监听（流式块）
    onStreamChunk = (_event: unknown, chunk: LlmStreamChunkPayload) => {
      const msg = messages.value.find((m) => m.id === assistantId);
      if (msg) {
        msg.content = chunk.text;
        if (chunk.done) {
          msg.isStreaming = false;
          isGenerating.value = false;
          streamingMessageId.value = null;
        }
      }
    };

    // 完整响应（备用：如果流式块不触发 done）
    onResponse = () => {
      isGenerating.value = false;
      streamingMessageId.value = null;
      const msg = messages.value.find((m) => m.id === assistantId);
      if (msg) {
        msg.isStreaming = false;
      }
    };

    // 情绪更新事件（架构不变量 #8）
    onEmotionUpdate = (_event: unknown, _payload: EmotionUpdatePayload) => {
      // 情绪更新由 useEmotion composable 处理，此处仅透传
      window.dispatchEvent(
        new CustomEvent('emotion-update', { detail: _payload }),
      );
    };

    window.electronAPI.on(IpcChannel.LLM_STREAM_CHUNK, onStreamChunk);
    window.electronAPI.on(IpcChannel.LLM_RESPONSE, onResponse);
    window.electronAPI.on(IpcChannel.EMOTION_UPDATE, onEmotionUpdate);

    // 发送消息
    window.electronAPI.send(IpcChannel.LLM_CHAT, payload);
  }

  /** 中止当前生成 */
  function abortGeneration(): void {
    if (!isGenerating.value) return;
    window.electronAPI.send(IpcChannel.LLM_ABORT, {});
    isGenerating.value = false;
    streamingMessageId.value = null;
  }

  /** 清空对话记录 */
  function clearMessages(): void {
    messages.value = [];
  }

  /** 组件卸载时清理 IPC 监听 */
  onUnmounted(() => {
    if (onStreamChunk) {
      window.electronAPI.removeListener(IpcChannel.LLM_STREAM_CHUNK, onStreamChunk);
    }
    if (onResponse) {
      window.electronAPI.removeListener(IpcChannel.LLM_RESPONSE, onResponse);
    }
    if (onEmotionUpdate) {
      window.electronAPI.removeListener(IpcChannel.EMOTION_UPDATE, onEmotionUpdate);
    }
  });

  return {
    messages,
    isGenerating,
    streamingMessageId,
    sendMessage,
    abortGeneration,
    clearMessages,
  };
}
