/**
 * Planner（规划器）— Agent 管线编排层
 *
 * 架构不变量 A5: 管线中 Planner 负责构建 LLM 调用上下文
 * 架构不变量 A1: Planner 从 PersonalityEngine 获取 VAD 注入到 system prompt
 *
 * 职责：
 *   1. 从 PersonalityEngine 获取当前 VAD 状态
 *   2. 将 VAD 情感上下文注入 system prompt
 *   3. 构建包含记忆上下文的 messages 列表
 *   4. 决定是否附加工具定义
 */

import type { LlmMessage, ChatOptions } from '../llm-connector';
import type { AffectiveState } from '../personality/affective';

/** Planner 配置 */
export interface PlannerConfig {
  systemPrompt: string;
  conversationHistory: LlmMessage[];
  userMessage: string;
  currentVAD?: AffectiveState;
  tools?: ChatOptions['tools'];
  temperature?: number;
}

/** Planner 结果 */
export interface PlanResult {
  messages: LlmMessage[];
  options: ChatOptions;
}

/**
 * 将 VAD 状态注入 system prompt 的附加指令
 */
function buildVADContext(vad: AffectiveState): string {
  const valenceDesc = vad.valence > 0.6 ? '积极正面' : vad.valence < 0.4 ? '消极负面' : '中性';
  const arousalDesc = vad.arousal > 0.6 ? '兴奋活跃' : vad.arousal < 0.4 ? '平静低沉' : '适度';
  const dominanceDesc = vad.dominance > 0.6 ? '自信主导' : vad.dominance < 0.4 ? '顺从被动' : '平衡';
  return [
    ``,
    `## 当前情感状态`,
    `- 效价 (Valence): ${(vad.valence * 100).toFixed(0)}% — ${valenceDesc}`,
    `- 唤醒度 (Arousal): ${(vad.arousal * 100).toFixed(0)}% — ${arousalDesc}`,
    `- 支配度 (Dominance): ${(vad.dominance * 100).toFixed(0)}% — ${dominanceDesc}`,
    `请根据上述情感状态调整回复的语气和风格。`,
  ].join('\n');
}

/** Planner */
export class Planner {
  /**
   * 规划 LLM 调用
   * @param config 规划配置
   */
  plan(config: PlannerConfig): PlanResult {
    let systemPrompt = config.systemPrompt;

    // 架构不变量 A1: 注入 VAD 情感上下文
    if (config.currentVAD) {
      systemPrompt += buildVADContext(config.currentVAD);
    }

    const messages: LlmMessage[] = [
      { role: 'system', content: systemPrompt },
      ...config.conversationHistory,
      { role: 'user', content: config.userMessage },
    ];

    return {
      messages,
      options: {
        temperature: config.temperature ?? 0.7,
        tools: config.tools,
      },
    };
  }
}
