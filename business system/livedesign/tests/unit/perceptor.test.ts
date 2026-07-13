/**
 * Perceptor 感知器单元测试
 *
 * 架构不变量 A11: LLM 感知降级不可丢失能力
 * 架构不变量 A5: 所有用户请求必须经过 Perceptor 管线
 * 架构不变量 A7: 输入已归一化为 InputEvent
 */
import { describe, it, expect, vi } from 'vitest';
import { Perceptor } from '../../src/llm-service/agent/perceptor';
import type { InputEvent } from '../../src/shared/ipc-protocol';

function makeInput(payload: string): InputEvent {
  return {
    type: 'text',
    payload,
    timestamp: Date.now(),
  };
}

describe('Perceptor - perceive（已有功能回归）', () => {
  it('中性输入应返回 llm_response', () => {
    const p = new Perceptor();
    const result = p.perceive(makeInput('今天天气不错'));
    expect(result.userEmotion).toBe('llm_response');
    expect(result.intensity).toBe(0.3);
  });

  it('难过关键词应映射到 user_sad', () => {
    const p = new Perceptor();
    const result = p.perceive(makeInput('我好难过啊'));
    expect(result.userEmotion).toBe('user_sad');
  });

  it('开心关键词应映射到 user_happy', () => {
    const p = new Perceptor();
    const result = p.perceive(makeInput('哈哈太开心了'));
    expect(result.userEmotion).toBe('user_happy');
  });

  it('工具关键词应检测 requiresTools', () => {
    const p = new Perceptor();
    const result = p.perceive(makeInput('帮我搜索一下今天的新闻'));
    expect(result.requiresTools).toBe(true);
    expect(result.keywords).toContain('requires_tools');
  });
});

describe('Perceptor - llmPerceive（架构不变量 A11）', () => {
  it('无 llmFn 应自动降级到 perceive()', async () => {
    const input = makeInput('真的很难过很伤心');
    const result = await Perceptor.llmPerceive(input);
    // 降级返回 PerceptionResult 类型
    expect(result).toHaveProperty('userEmotion');
    expect(result).toHaveProperty('intensity');
    if ('userEmotion' in result) {
      expect(result.userEmotion).toBe('user_sad');
    }
  });

  it('LLM 返回有效 JSON 应返回 VADWithConfidence', async () => {
    const input = makeInput('I feel great today!');
    const mockLLM = vi.fn().mockResolvedValue(
      JSON.stringify({ emotion: 'happy', intensity: 0.8, valence: 0.9, arousal: 0.7, dominance: 0.6 })
    );
    const result = await Perceptor.llmPerceive(input, mockLLM);
    expect(result).toHaveProperty('vad');
    expect(result).toHaveProperty('confidence');
    if ('vad' in result) {
      expect(result.vad.valence).toBe(0.9);
      expect(result.vad.arousal).toBe(0.7);
      expect(result.vad.dominance).toBe(0.6);
      expect(result.confidence).toBe(0.8);
    }
  });

  it('VAD 值应被 clamp 到 [0,1]', async () => {
    const input = makeInput('test');
    const mockLLM = vi.fn().mockResolvedValue(
      JSON.stringify({ emotion: 'angry', intensity: 1.5, valence: 2.0, arousal: -0.5, dominance: 0.5 })
    );
    const result = await Perceptor.llmPerceive(input, mockLLM);
    if ('vad' in result) {
      expect(result.vad.valence).toBe(1.0);
      expect(result.vad.arousal).toBe(0.0);
      expect(result.confidence).toBe(1.0);
    }
  });

  it('LLM 超时应自动降级到 perceive()', async () => {
    const input = makeInput('我好伤心');
    const mockLLM = vi.fn().mockImplementation(
      () => new Promise((resolve) => setTimeout(() => resolve('{}'), 10000))
    );
    const result = await Perceptor.llmPerceive(input, mockLLM, 100);
    // 超时降级为 PerceptionResult
    expect(result).toHaveProperty('userEmotion');
    if ('userEmotion' in result) {
      expect(result.userEmotion).toBe('user_sad');
    }
  });

  it('LLM 返回无效 JSON 应降级到 perceive()', async () => {
    const input = makeInput('hello');
    const mockLLM = vi.fn().mockResolvedValue('not valid json');
    const result = await Perceptor.llmPerceive(input, mockLLM);
    expect(result).toHaveProperty('userEmotion');
    if ('userEmotion' in result) {
      expect(result.userEmotion).toBe('llm_response');
    }
  });

  it('LLM 返回缺失必需字段应降级', async () => {
    const input = makeInput('test');
    const mockLLM = vi.fn().mockResolvedValue(
      JSON.stringify({ emotion: 'happy', intensity: 0.5 })
      // 缺少 valence/arousal/dominance
    );
    const result = await Perceptor.llmPerceive(input, mockLLM);
    expect(result).toHaveProperty('userEmotion');
  });

  it('LLM 抛出异常应降级到 perceive()', async () => {
    const input = makeInput('真的好伤心好难过');
    const mockLLM = vi.fn().mockRejectedValue(new Error('LLM service unavailable'));
    const result = await Perceptor.llmPerceive(input, mockLLM);
    expect(result).toHaveProperty('userEmotion');
    if ('userEmotion' in result) {
      expect(result.userEmotion).toBe('user_sad');
    }
  });

  it('llmPerceive 混合情绪字段应正确传递', async () => {
    const input = makeInput('feeling mixed');
    const mockLLM = vi.fn().mockResolvedValue(
      JSON.stringify({
        emotion: 'sad',
        intensity: 0.6,
        valence: 0.2,
        arousal: 0.3,
        dominance: 0.3,
      })
    );
    const result = await Perceptor.llmPerceive(input, mockLLM);
    if ('vad' in result) {
      expect(result.mixedEmotions).toBeDefined();
      expect(result.mixedEmotions![0].emotion).toBe('sad');
    }
  });
});
