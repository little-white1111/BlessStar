/**
 * Agent Orchestrator（代理编排器）
 *
 * 架构不变量 A5: 所有用户请求必须经过 Perceptor → Planner → Executor → Expresser
 * 架构不变量 A7: 输入已归一化为 InputEvent
 *
 * 当前实现覆盖管线骨架：
 *   Perceptor → Planner → [LLM Executor 由外部注入] → Expresser
 * 后续可扩展 Cognitive（记忆融合）和 Executor（工具执行）
 */

import type { InputEvent } from '../../shared/ipc-protocol';
import type { LlmMessage, ChatOptions } from '../llm-connector';
import type { ExpressionResult } from './expresser';
import type { PersonalityEngine } from '../personality/engine';
import { Perceptor } from './perceptor';
import { Planner } from './planner';
import { Expresser } from './expresser';
import { EmotionInferrer } from '../emotion-inferrer';

/** LLM 流式回调 */
export interface StreamCallbacks {
  onStream: (chunk: string) => void;
  onDone: (result: ExpressionResult) => void;
  onError: (error: Error) => void;
}

/** LLM 执行器接口 */
export interface LlmExecutor {
  chatStream(
    messages: LlmMessage[],
    options: ChatOptions,
    callbacks: StreamCallbacks,
  ): Promise<void>;
}

/** Agent Orchestrator */
export class AgentOrchestrator {
  readonly perceptor: Perceptor = new Perceptor();
  readonly planner: Planner = new Planner();
  readonly expresser: Expresser = new Expresser();
  readonly emotionInferrer: EmotionInferrer = new EmotionInferrer();

  /**
   * 执行完整管线：Perceptor → PersonalityEngine → Planner → LLM → Expresser
   *
   * @param input  归一化输入事件（架构不变量 A7）
   * @param pe     人格引擎实例（架构不变量 A1）
   * @param config 管线配置
   * @param exec   LLM 执行器
   */
  async execute(
    input: InputEvent,
    pe: PersonalityEngine,
    config: {
      systemPrompt: string;
      conversationHistory: LlmMessage[];
      tools?: ChatOptions['tools'];
      temperature?: number;
    },
    exec: LlmExecutor,
  ): Promise<void> {
    // Step 1: Perceptor — 感知分析 （架构不变量 A5）
    const perception = this.perceptor.perceive(input);

    // Step 2: 触发 PersonalityEngine（架构不变量 A1）
    pe.applyEvent({ type: perception.userEmotion, intensity: perception.intensity });
    const currentVAD = pe.getAffective();

    // Step 3: Planner — 构建 LLM 上下文
    const plan = this.planner.plan({
      systemPrompt: config.systemPrompt,
      conversationHistory: config.conversationHistory,
      userMessage: input.payload,
      currentVAD,
      tools: config.tools,
      temperature: config.temperature,
    });

    // Step 4: Executor（由外部注入）
    await exec.chatStream(plan.messages, plan.options, {
      onStream: (chunk: string) => {
        // 转发流式块（由 LLM 执行器管理）
      },
      onDone: (fullResponse: string) => {
        // Step 5: EmotionInferrer
        const inferred = this.emotionInferrer.infer(fullResponse);

        // Step 6: 经 PersonalityEngine 过滤（架构不变量 A1）
        pe.applyEvent({
          type: 'llm_response',
          emotion: inferred.emotionUpdate.emotion,
          intensity: inferred.emotionUpdate.intensity ?? 0.5,
        });
        const filteredVAD = pe.getAffective();

        // Step 7: Expresser — 组装输出
        // 注：onDone 需要类型改为接受完整文本，当前由调用方自行组装
      },
      onError: (error: Error) => {
        // 错误由调用方处理
      },
    });
  }
}
