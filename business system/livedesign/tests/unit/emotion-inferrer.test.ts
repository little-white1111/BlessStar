/**
 * 情绪推断引擎单元测试
 * 测试 EmotionInferrer 的两种解析模式和默认行为
 */
import { describe, it, expect } from 'vitest';
import { EmotionInferrer } from '../../src/llm-service/emotion-inferrer';

describe('EmotionInferrer', () => {
  describe('结构化 JSON 模式', () => {
    it('应正确解析包含 text、emotion 和 action 的 JSON 块', () => {
      const inferrer = new EmotionInferrer();
      const response = JSON.stringify({
        text: '你好呀，今天过得怎么样？',
        emotion: 'happy',
        action: 'wave',
      });

      const result = inferrer.infer(response);

      expect(result.text).toBe('你好呀，今天过得怎么样？');
      expect(result.emotionUpdate.emotion).toBe('happy');
      expect(result.emotionUpdate.action).toBe('wave');
      expect(result.emotionUpdate.intensity).toBe(0.5);
    });

    it('未指定 action 时应根据 emotion 映射默认动作', () => {
      const inferrer = new EmotionInferrer();
      const response = JSON.stringify({
        text: '这真是太令人惊讶了！',
        emotion: 'surprised',
      });

      const result = inferrer.infer(response);

      expect(result.emotionUpdate.emotion).toBe('surprised');
      expect(result.emotionUpdate.action).toBe('cheer');
    });

    it('未指定 emotion 时应默认 calm', () => {
      const inferrer = new EmotionInferrer();
      const response = JSON.stringify({
        text: '好的，我知道了。',
      });

      const result = inferrer.infer(response);

      expect(result.emotionUpdate.emotion).toBe('calm');
      expect(result.emotionUpdate.action).toBe('idle');
    });

    it('JSON 块中缺少 text 字段时应回退到标签模式', () => {
      const inferrer = new EmotionInferrer();
      const response = JSON.stringify({
        emotion: 'happy',
        action: 'wave',
      });

      // 没有 text 字段，tryParseJsonMode 返回 null，回退到标签模式
      const result = inferrer.infer(response);

      // 注：JSON.stringify 的结果会被 JSON_BLOCK_REGEX 匹配，
      // 但因为缺少 text，tryParseJsonMode 返回 null
      // 所以会进入 parseTagMode，此时没有标签，返回默认值
      expect(result.emotionUpdate.emotion).toBe('calm');
      expect(result.emotionUpdate.action).toBe('idle');
    });

    it('intensity 应被限制在 0.0 ~ 1.0 范围内', () => {
      const inferrer = new EmotionInferrer();
      const response = JSON.stringify({
        text: '太棒了！',
        emotion: 'excited',
        intensity: 2.5,
      });

      const result = inferrer.infer(response);

      expect(result.emotionUpdate.intensity).toBe(1.0);
    });

    it('intensity 为负值时应被限制为 0.0', () => {
      const inferrer = new EmotionInferrer();
      const response = JSON.stringify({
        text: '嗯...',
        emotion: 'calm',
        intensity: -0.5,
      });

      const result = inferrer.infer(response);

      expect(result.emotionUpdate.intensity).toBe(0.0);
    });
  });

  describe('标签内嵌模式', () => {
    it('应从文本中提取 [emotion:xxx] 标签', () => {
      const inferrer = new EmotionInferrer();
      const response = '你好呀 [emotion:happy] 今天真开心';

      const result = inferrer.infer(response);

      // 标签被移除后，前后空格保留，trim 只去除首尾
      expect(result.text).toBe('你好呀  今天真开心');
      expect(result.emotionUpdate.emotion).toBe('happy');
    });

    it('应从文本中提取 [action:xxx] 标签', () => {
      const inferrer = new EmotionInferrer();
      const response = '你好 [action:wave] 欢迎！';

      const result = inferrer.infer(response);

      expect(result.text).toBe('你好  欢迎！');
      expect(result.emotionUpdate.action).toBe('wave');
      expect(result.emotionUpdate.emotion).toBe('calm'); // 无 emotion 标签时默认 calm
    });

    it('应同时提取 emotion 和 action 标签', () => {
      const inferrer = new EmotionInferrer();
      const response = '[emotion:angry][action:shake] 你怎麼能這樣做！';

      const result = inferrer.infer(response);

      expect(result.text).toBe('你怎麼能這樣做！');
      expect(result.emotionUpdate.emotion).toBe('angry');
      expect(result.emotionUpdate.action).toBe('shake');
    });

    it('未指定 action 时映射默认动作', () => {
      const inferrer = new EmotionInferrer();
      const response = '我好難過 [emotion:sad]';

      const result = inferrer.infer(response);

      expect(result.text).toBe('我好難過');
      expect(result.emotionUpdate.emotion).toBe('sad');
      expect(result.emotionUpdate.action).toBe('sigh');
    });

    it('应支持中文情绪标签', () => {
      const inferrer = new EmotionInferrer();
      const response = '好開心啊 [emotion:高兴]';

      const result = inferrer.infer(response);

      expect(result.emotionUpdate.emotion).toBe('高兴');
      expect(result.emotionUpdate.action).toBe('smile');
    });
  });

  describe('缺失标签时的默认行为', () => {
    it('没有任何标签时应返回默认情绪', () => {
      const inferrer = new EmotionInferrer();
      const response = '今天天气真不错。';

      const result = inferrer.infer(response);

      expect(result.text).toBe('今天天气真不错。');
      expect(result.emotionUpdate.emotion).toBe('calm');
      expect(result.emotionUpdate.action).toBe('idle');
      expect(result.emotionUpdate.intensity).toBe(0.5);
    });

    it('空字符串应返回默认情绪', () => {
      const inferrer = new EmotionInferrer();
      const result = inferrer.infer('');

      expect(result.text).toBe('');
      expect(result.emotionUpdate.emotion).toBe('calm');
      expect(result.emotionUpdate.action).toBe('idle');
    });
  });

  describe('自定义情绪-动作映射', () => {
    it('应使用自定义映射覆盖默认映射', () => {
      const inferrer = new EmotionInferrer({
        happy: 'dance',
        custom: 'nod',
      });

      const response = '[emotion:happy] 太棒了！';
      const result = inferrer.infer(response);

      expect(result.emotionUpdate.action).toBe('dance');
    });

    it('自定义映射中不存在的 emotion 应回退为 idle', () => {
      const inferrer = new EmotionInferrer({
        happy: 'dance',
      });

      const response = '[emotion:unknown_emotion] 你好';
      const result = inferrer.infer(response);

      expect(result.emotionUpdate.action).toBe('idle');
    });
  });

  describe('updateEmotionActionMap 方法', () => {
    it('应能在运行时更新映射', () => {
      const inferrer = new EmotionInferrer();
      inferrer.updateEmotionActionMap({ happy: 'jump' });

      const response = '[emotion:happy] 耶！';
      const result = inferrer.infer(response);

      expect(result.emotionUpdate.action).toBe('jump');
    });
  });

  describe('resetToDefaultMap 方法', () => {
    it('应重置为默认映射', () => {
      const inferrer = new EmotionInferrer({ happy: 'dance' });

      // 验证自定义映射生效
      let result = inferrer.infer('[emotion:happy] hi');
      expect(result.emotionUpdate.action).toBe('dance');

      // 重置
      inferrer.resetToDefaultMap();

      // 验证恢复默认
      result = inferrer.infer('[emotion:happy] hi');
      expect(result.emotionUpdate.action).toBe('smile');
    });
  });

  describe('VAD 输出（架构不变量 A1）', () => {
    it('JSON 模式应输出 VAD 三轴值', () => {
      const inferrer = new EmotionInferrer();
      const response = JSON.stringify({
        text: '我好开心！',
        emotion: 'happy',
        action: 'smile',
      });

      const result = inferrer.infer(response);
      expect(result.emotionUpdate.valence).toBeDefined();
      expect(result.emotionUpdate.arousal).toBeDefined();
      expect(result.emotionUpdate.dominance).toBeDefined();
      // happy 的 valence 应大于中性
      expect(result.emotionUpdate.valence!).toBeGreaterThan(0.5);
    });

    it('标签模式应输出 VAD 三轴值', () => {
      const inferrer = new EmotionInferrer();
      const result = inferrer.infer('[emotion:sad] 我好难过');

      expect(result.emotionUpdate.valence).toBeDefined();
      expect(result.emotionUpdate.arousal).toBeDefined();
      expect(result.emotionUpdate.dominance).toBeDefined();
      // sad 的 valence 应小于中性
      expect(result.emotionUpdate.valence!).toBeLessThan(0.5);
    });

    it('buildVADPayload 应从 emotion 构建正确的 VAD', () => {
      const inferrer = new EmotionInferrer();
      const payload = inferrer.buildVADPayload('happy', 'smile', 0.8);

      expect(payload.emotion).toBe('happy');
      expect(payload.action).toBe('smile');
      expect(payload.intensity).toBe(0.8);
      expect(payload.valence).toBeGreaterThan(0.5);
      expect(payload.arousal).toBeGreaterThan(0.5);
    });

    it('buildPayloadFromVAD 应从 VAD 状态构建 payload', () => {
      const inferrer = new EmotionInferrer();
      const payload = inferrer.buildPayloadFromVAD(
        { valence: 0.9, arousal: 0.75, dominance: 0.55 },
        0.6
      );

      expect(payload.emotion).toBe('joy');
      expect(payload.intensity).toBe(0.6);
      expect(payload.valence).toBe(0.9);
      expect(payload.arousal).toBe(0.75);
      expect(payload.dominance).toBe(0.55);
    });
  });

  describe('JSON 模式显式 VAD 字段', () => {
    it('应优先使用 JSON 中显式指定的 VAD 值', () => {
      const inferrer = new EmotionInferrer();
      const response = JSON.stringify({
        text: '自定义情感',
        emotion: 'happy',
        action: 'wave',
        valence: 0.1,   // 显式指定低 valence
        arousal: 0.9,
        dominance: 0.2,
      });

      const result = inferrer.infer(response);
      // happy 的默认 valence 是 0.85，但显式指定了 0.1 应优先使用
      expect(result.emotionUpdate.valence).toBe(0.1);
      expect(result.emotionUpdate.arousal).toBe(0.9);
      expect(result.emotionUpdate.dominance).toBe(0.2);
    });
  });
});
