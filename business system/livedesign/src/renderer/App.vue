<script setup lang="ts">
import { ref, onMounted } from 'vue';
import type { CharacterCard } from '../shared/character-card';
import FloatBall from './components/FloatBall.vue';
import Live2DCanvas from './components/Live2DCanvas.vue';
import ChatPanel from './components/ChatPanel.vue';
import RoleSelector from './components/RoleSelector.vue';
import SettingsPanel from './components/SettingsPanel.vue';

// 面板可见性状态
const chatVisible = ref(false);
const roleSelectorVisible = ref(false);
const settingsVisible = ref(false);

// 当前角色
const currentCharacter = ref<CharacterCard | null>(null);

// 当前情绪（接收自 emotion-update 事件）
const currentEmotion = ref('neutral');

// 悬浮球透明度
const floatBallOpacity = ref(0.85);

/** 切换 ChatPanel 显示 */
function toggleChat(): void {
  chatVisible.value = !chatVisible.value;
}

/** 角色选择回调 */
function onCharacterSelect(card: CharacterCard): void {
  currentCharacter.value = card;
}

/**
 * 监听 window 上的 emotion-update 事件（架构不变量 #8）
 * 主进程转发 LLM Service 的情绪推断结果
 */
function setupEmotionListener(): void {
  window.addEventListener('emotion-update', ((event: CustomEvent) => {
    if (event.detail?.emotion) {
      currentEmotion.value = event.detail.emotion;
    }
  }) as EventListener);
}

/** 加载初始配置 */
async function loadInitialConfig(): Promise<void> {
  try {
    const config = await window.electronAPI.getConfig();
    if (config.floatBallOpacity !== undefined) {
      floatBallOpacity.value = config.floatBallOpacity as number;
    }
    if (config.theme === 'dark') {
      document.documentElement.setAttribute('data-theme', 'dark');
    }
  } catch {
    // 使用默认值
  }
}

/** 加载默认角色 */
async function loadDefaultCharacter(): Promise<void> {
  try {
    const characters = await window.electronAPI.getCharacters();
    if (characters.length > 0) {
      currentCharacter.value = characters[0];
    }
  } catch {
    // 无可用角色卡
  }
}

onMounted(() => {
  setupEmotionListener();
  loadInitialConfig();
  loadDefaultCharacter();
});
</script>

<template>
  <FloatBall
    :opacity="floatBallOpacity"
    @open-chat="toggleChat"
    @open-settings="settingsVisible = !settingsVisible"
    @open-role-selector="roleSelectorVisible = !roleSelectorVisible"
  />

  <Live2DCanvas />

  <ChatPanel
    :visible="chatVisible"
    :character="currentCharacter"
    :current-emotion="currentEmotion"
    @close="chatVisible = false"
  />

  <RoleSelector
    :visible="roleSelectorVisible"
    :current-character="currentCharacter"
    @close="roleSelectorVisible = false"
    @select="onCharacterSelect"
  />

  <SettingsPanel
    :visible="settingsVisible"
    @close="settingsVisible = false"
  />
</template>
