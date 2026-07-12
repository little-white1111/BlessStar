<script setup lang="ts">
import { ref, onMounted } from 'vue';
import type { CharacterCard } from '../../shared/character-card';

const props = defineProps<{
  visible: boolean;
  currentCharacter: CharacterCard | null;
}>();

const emit = defineEmits<{
  close: [];
  select: [card: CharacterCard];
}>();

const characters = ref<CharacterCard[]>([]);
const isOpen = ref(false);
const loading = ref(false);

/** 加载角色卡列表 */
async function loadCharacters(): Promise<void> {
  loading.value = true;
  try {
    const list = await window.electronAPI.getCharacters();
    characters.value = list;
  } catch {
    // 加载失败时使用空列表
    characters.value = [];
  } finally {
    loading.value = false;
  }
}

/** 切换角色（架构不变量 #12） */
function selectCharacter(card: CharacterCard): void {
  emit('select', card);
  window.electronAPI.switchCharacter(card);
  isOpen.value = false;
}

onMounted(() => {
  if (props.visible) {
    loadCharacters();
  }
});
</script>

<template>
  <div v-if="visible" class="role-selector">
    <!-- 触发按钮 -->
    <button class="role-selector-trigger" @click="isOpen = !isOpen">
      <span>{{ currentCharacter?.name ?? '选择角色' }}</span>
      <span style="font-size: 10px">{{ isOpen ? '▲' : '▼' }}</span>
    </button>

    <!-- 下拉列表 -->
    <div v-if="isOpen" class="role-selector-dropdown">
      <div v-if="loading" style="padding: 12px; text-align: center; color: var(--text-muted); font-size: 13px">
        加载中...
      </div>
      <div
        v-for="card in characters"
        :key="card.name"
        class="role-selector-item"
        :class="{ active: currentCharacter?.name === card.name }"
        @click="selectCharacter(card)"
      >
        {{ card.name }}
      </div>
      <div
        v-if="!loading && characters.length === 0"
        style="padding: 12px; text-align: center; color: var(--text-muted); font-size: 13px"
      >
        暂无可用角色卡
      </div>
    </div>
  </div>
</template>
