<script setup lang="ts">
import { ref, onMounted, watch } from 'vue';

const props = defineProps<{
  visible: boolean;
}>();

const emit = defineEmits<{
  close: [];
}>();

// 配置项
const temperature = ref(0.7);
const floatBallOpacity = ref(85);
const theme = ref<'light' | 'dark'>('light');
const selectedTools = ref<string[]>([]);

// 可用工具列表（架构不变量 #6：工具调用必须经过动作审批）
const availableTools = ref([
  { id: 'web_search', label: '网络搜索' },
  { id: 'file_read', label: '读取文件' },
  { id: 'code_exec', label: '代码执行' },
  { id: 'image_gen', label: '图片生成' },
  { id: 'weather', label: '天气查询' },
  { id: 'calendar', label: '日历管理' },
]);

/** 切换工具选择 */
function toggleTool(toolId: string): void {
  const idx = selectedTools.value.indexOf(toolId);
  if (idx >= 0) {
    selectedTools.value.splice(idx, 1);
  } else {
    selectedTools.value.push(toolId);
  }
}

/** 切换主题 */
function toggleTheme(): void {
  theme.value = theme.value === 'light' ? 'dark' : 'light';
  document.documentElement.setAttribute('data-theme', theme.value);
  window.electronAPI.setConfig('theme', theme.value);
}

/** 保存配置 */
function saveSettings(): void {
  window.electronAPI.setConfig('temperature', temperature.value);
  window.electronAPI.setConfig('floatBallOpacity', floatBallOpacity.value);
  window.electronAPI.setConfig('tools', selectedTools.value);
  emit('close');
}

/** 加载已有配置 */
async function loadConfig(): Promise<void> {
  try {
    const config = await window.electronAPI.getConfig();
    if (config.temperature !== undefined) temperature.value = config.temperature as number;
    if (config.floatBallOpacity !== undefined) floatBallOpacity.value = config.floatBallOpacity as number;
    if (config.theme !== undefined) {
      theme.value = config.theme as 'light' | 'dark';
      document.documentElement.setAttribute('data-theme', theme.value);
    }
    if (config.tools !== undefined) {
      selectedTools.value = config.tools as string[];
    }
  } catch {
    // 加载失败使用默认值
  }
}

onMounted(() => {
  if (props.visible) {
    loadConfig();
  }
});
</script>

<template>
  <div v-if="visible" class="settings-overlay" style="position: fixed; inset: 0; background: rgba(0,0,0,0.3); z-index: 10002" @click.self="emit('close')">
    <div class="settings-panel">
      <div class="settings-title">设置</div>

      <!-- 温度滑块 -->
      <div class="settings-group">
        <label class="settings-label">
          <span>温度 (Temperature)</span>
          <span>{{ temperature.toFixed(1) }}</span>
        </label>
        <input
          v-model.number="temperature"
          type="range"
          class="settings-slider"
          min="0"
          max="1"
          step="0.1"
        />
      </div>

      <!-- 悬浮球透明度 -->
      <div class="settings-group">
        <label class="settings-label">
          <span>悬浮球透明度</span>
          <span>{{ floatBallOpacity }}%</span>
        </label>
        <input
          v-model.number="floatBallOpacity"
          type="range"
          class="settings-slider"
          min="30"
          max="100"
          step="5"
        />
      </div>

      <!-- 工具选择（架构不变量 #6） -->
      <div class="settings-group">
        <label class="settings-label">可用工具</label>
        <div class="settings-checkbox-group">
          <span
            v-for="tool in availableTools"
            :key="tool.id"
            class="settings-chip"
            :class="{ selected: selectedTools.includes(tool.id) }"
            @click="toggleTool(tool.id)"
          >
            {{ tool.label }}
          </span>
        </div>
      </div>

      <!-- 主题切换 -->
      <div class="settings-group" style="flex-direction: row; justify-content: space-between; align-items: center">
        <label class="settings-label" style="margin-bottom: 0">深色模式</label>
        <button
          class="settings-toggle"
          :class="{ active: theme === 'dark' }"
          @click="toggleTheme"
        />
      </div>

      <!-- 按钮 -->
      <div class="settings-actions">
        <button class="settings-btn secondary" @click="emit('close')">取消</button>
        <button class="settings-btn primary" @click="saveSettings">保存</button>
      </div>
    </div>
  </div>
</template>
