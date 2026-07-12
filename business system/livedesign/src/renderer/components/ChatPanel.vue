<script setup lang="ts">
import { ref, nextTick, watch } from 'vue';
import { useChat } from '../composables/useChat';
import type { CharacterCard } from '../../shared/character-card';

const props = defineProps<{
  visible: boolean;
  character: CharacterCard | null;
  currentEmotion?: string;
}>();

const emit = defineEmits<{
  close: [];
}>();

const { messages, isGenerating, sendMessage, abortGeneration, clearMessages } = useChat();

const inputText = ref('');
const messagesContainer = ref<HTMLElement | null>(null);

/** 发送消息 */
function handleSend(): void {
  if (!inputText.value.trim() || !props.character) return;

  // 如果正在生成，先中止（发送新消息自动开始新轮次）
  if (isGenerating.value) {
    abortGeneration();
  }

  sendMessage(inputText.value, props.character);
  inputText.value = '';

  // 发送后滚动到底部
  nextTick(() => scrollToBottom());
}

/** 键盘发送（Enter 发送，Shift+Enter 换行） */
function handleKeydown(e: KeyboardEvent): void {
  if (e.key === 'Enter' && !e.shiftKey) {
    e.preventDefault();
    handleSend();
  }
}

/** 滚动到底部 */
function scrollToBottom(): void {
  if (messagesContainer.value) {
    messagesContainer.value.scrollTop = messagesContainer.value.scrollHeight;
  }
}

// 新消息到达时滚动到底部
watch(
  () => messages.value.length,
  () => {
    nextTick(() => scrollToBottom());
  },
);
</script>

<template>
  <div v-if="visible" class="chat-panel">
    <!-- 头部：角色信息 -->
    <div class="chat-header">
      <div class="chat-header-info">
        <span class="chat-header-name">
          {{ character?.name ?? '未选择角色' }}
        </span>
        <span class="chat-header-emotion">
          情绪: {{ currentEmotion ?? 'neutral' }}
        </span>
      </div>
      <button class="settings-btn secondary" style="padding: 4px 10px; font-size: 12px" @click="clearMessages">
        清空
      </button>
      <button
        class="settings-btn secondary"
        style="padding: 4px 10px; font-size: 12px"
        @click="emit('close')"
      >
        ✕
      </button>
    </div>

    <!-- 消息列表 -->
    <div ref="messagesContainer" class="chat-messages">
      <div
        v-for="msg in messages"
        :key="msg.id"
        class="message"
        :class="{
          user: msg.role === 'user',
          assistant: msg.role === 'assistant',
          streaming: msg.isStreaming,
        }"
      >
        {{ msg.content }}
        <span v-if="msg.isStreaming" class="cursor-blink">▌</span>
      </div>

      <!-- 空状态 -->
      <div
        v-if="messages.length === 0"
        style="text-align: center; color: var(--text-muted); font-size: 13px; padding: 40px 0"
      >
        开始和 {{ character?.name ?? '角色' }} 对话吧
      </div>
    </div>

    <!-- 输入区 -->
    <div class="chat-input-area">
      <textarea
        v-model="inputText"
        class="chat-input"
        rows="1"
        placeholder="输入消息..."
        :disabled="!character"
        @keydown="handleKeydown"
      />
      <button
        v-if="isGenerating"
        class="chat-send-btn"
        style="background: var(--danger-color)"
        @click="abortGeneration"
      >
        停止
      </button>
      <button
        v-else
        class="chat-send-btn"
        :disabled="!inputText.trim() || !character"
        @click="handleSend"
      >
        发送
      </button>
    </div>
  </div>
</template>

<style scoped>
.cursor-blink {
  animation: blink 1s step-end infinite;
}

@keyframes blink {
  50% {
    opacity: 0;
  }
}
</style>
