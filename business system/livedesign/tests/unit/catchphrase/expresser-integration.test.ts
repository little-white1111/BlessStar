/**
 * Expresser 集成测试 — VAD → EmotionTrigger → Selector → Expresser 全链路
 *
 * 架构不变量 D1: 一致性检查
 * 架构不变量 D3: 频率限制
 * 架构不变量 D6: 语气强度调制
 */

import { describe, it, expect, beforeEach } from 'vitest';
import { CoreCatchphraseManager } from '../../../src/llm-service/catchphrase/core-manager';
import { RelationalCatchphraseManager } from '../../../src/llm-service/catchphrase/relational-manager';
import { EmotionTriggerMatcher } from '../../../src/llm-service/catchphrase/emotion-trigger';
import { ScenarioTriggerMatcher } from '../../../src/llm-service/catchphrase/scenario-trigger';
import { FrequencyGovernor } from '../../../src/llm-service/catchphrase/frequency-governor';
import { CatchphraseSelector } from '../../../src/llm-service/catchphrase/selector';
import { Expresser } from '../../../src/llm-service/agent/expresser';
import { EmotionInferrer } from '../../../src/llm-service/emotion-inferrer';
import type { CatchphraseSelectionConfig } from '../../../src/llm-service/catchphrase/types';

describe('Expresser 全链路集成', () => {
  let coreManager: CoreCatchphraseManager;
  let relationalManager: RelationalCatchphraseManager;
  let emotionTrigger: EmotionTriggerMatcher;
  let scenarioTrigger: ScenarioTriggerMatcher;
  let frequencyGovernor: FrequencyGovernor;
  let selector: CatchphraseSelector;
  let expresser: Expresser;
  let inferrer: EmotionInferrer;

  beforeEach(async () => {
    coreManager = new CoreCatchphraseManager();
    await coreManager.load();
    relationalManager = new RelationalCatchphraseManager(3, 0.7);
    emotionTrigger = new EmotionTriggerMatcher(0.2, 0.6);
    scenarioTrigger = new ScenarioTriggerMatcher();
    frequencyGovernor = new FrequencyGovernor(1);
    selector = new CatchphraseSelector(coreManager, relationalManager, emotionTrigger, scenarioTrigger, frequencyGovernor);
    expresser = new Expresser();
    inferrer = new EmotionInferrer();
  });

  it('VAD 偏差触发 → Selector 匹配 → Expresser 注入口头禅', () => {
    // Step 1: EmotionTrigger 上下文 → Selector 选择
    const config: CatchphraseSelectionConfig = {
      currentVAD: { valence: 0.1, arousal: 0.5, dominance: 0.5 },
      baselineVAD: { valence: 0.5, arousal: 0.5, dominance: 0.5 },
      scenarioTags: [],
      adaptiveEncouragement: 0.5,
      sessionId: 'integration-test-1',
    };
    const match = selector.select(config);
    expect(match).toBeDefined();
    expect(match!.source).toBe('core');

    // Step 2: Expresser 注入口头禅
    const inference = inferrer.infer('我没事');
    const filteredVAD = { valence: 0.5, arousal: 0.5, dominance: 0.5 };
    const result = expresser.express(inference, filteredVAD, match);

    // 口头禅应该被注入到文本中
    expect(result.text).toContain(match!.text);
    expect(result.catchphrase).toBeDefined();
    expect(result.catchphrase!.text).toBe(match!.text);
  });

  it('Expresser 不重复注入已含口头禅的文本', () => {
    const inference = inferrer.infer('加油！你一定可以的！');
    const match = {
      text: '你一定可以的',
      source: 'core' as const,
      intensity: 'medium' as const,
      confidence: 0.8,
      emotionType: 'encouragement',
    };
    const filteredVAD = { valence: 0.5, arousal: 0.5, dominance: 0.5 };
    const result = expresser.express(inference, filteredVAD, match);

    // 文本包含口头禅时不应重复注入
    expect(result.text).toBe('加油！你一定可以的！');
  });

  it('无口头禅匹配时 Expresser 应输出原始文本', () => {
    const inference = inferrer.infer('你好，今天天气不错');
    const filteredVAD = { valence: 0.5, arousal: 0.5, dominance: 0.5 };
    const result = expresser.express(inference, filteredVAD);

    expect(result.text).toBe('你好，今天天气不错');
    expect(result.catchphrase).toBeUndefined();
  });

  it('全链路：频率限制应阻止超限输出（架构不变量 D3）', () => {
    // 第一次选择
    const config1: CatchphraseSelectionConfig = {
      currentVAD: { valence: 0.1, arousal: 0.5, dominance: 0.5 },
      baselineVAD: { valence: 0.5, arousal: 0.5, dominance: 0.5 },
      scenarioTags: [],
      adaptiveEncouragement: 0.5,
      sessionId: 'integration-test-2',
    };
    const match1 = selector.select(config1);
    expect(match1).toBeDefined();

    // 记录输出
    frequencyGovernor.recordOutput('integration-test-2');

    // 第二次选择（同一 session），应被频率限制阻挡
    const match2 = selector.select(config1);
    expect(match2).toBeUndefined();
  });
});
