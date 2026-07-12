/**
 * CatchphraseSelector 单元测试
 *
 * 架构不变量 D1: 所有口头禅必须通过 ConsistencyChecker 验证。
 * 架构不变量 D6: 口头禅语气强度受 adaptive persona 当前值调制。
 */

import { describe, it, expect, beforeEach } from 'vitest';
import { CoreCatchphraseManager } from '../../../src/llm-service/catchphrase/core-manager';
import { RelationalCatchphraseManager } from '../../../src/llm-service/catchphrase/relational-manager';
import { EmotionTriggerMatcher } from '../../../src/llm-service/catchphrase/emotion-trigger';
import { ScenarioTriggerMatcher } from '../../../src/llm-service/catchphrase/scenario-trigger';
import { FrequencyGovernor } from '../../../src/llm-service/catchphrase/frequency-governor';
import { CatchphraseSelector, DefaultConsistencyChecker } from '../../../src/llm-service/catchphrase/selector';
import type { ConsistencyChecker } from '../../../src/llm-service/catchphrase/selector';

describe('CatchphraseSelector - 基本', () => {
  let coreManager: CoreCatchphraseManager;
  let relationalManager: RelationalCatchphraseManager;
  let emotionTrigger: EmotionTriggerMatcher;
  let scenarioTrigger: ScenarioTriggerMatcher;
  let frequencyGovernor: FrequencyGovernor;
  let selector: CatchphraseSelector;

  beforeEach(async () => {
    coreManager = new CoreCatchphraseManager();
    await coreManager.load();
    relationalManager = new RelationalCatchphraseManager(3, 0.7);
    emotionTrigger = new EmotionTriggerMatcher(0.2, 0.6);
    scenarioTrigger = new ScenarioTriggerMatcher();
    frequencyGovernor = new FrequencyGovernor(1);
    selector = new CatchphraseSelector(coreManager, relationalManager, emotionTrigger, scenarioTrigger, frequencyGovernor);
  });

  it('超出频率限制应返回 undefined', () => {
    frequencyGovernor.recordOutput('session-1');
    const result = selector.select({
      currentVAD: { valence: 0.1, arousal: 0.5, dominance: 0.5 },
      baselineVAD: { valence: 0.5, arousal: 0.5, dominance: 0.5 },
      scenarioTags: [],
      adaptiveEncouragement: 0.5,
      sessionId: 'session-1',
    });
    expect(result).toBeUndefined();
  });

  it('Emotion 匹配成功时应返回核心口头禅', () => {
    const result = selector.select({
      currentVAD: { valence: 0.1, arousal: 0.5, dominance: 0.5 },
      baselineVAD: { valence: 0.5, arousal: 0.5, dominance: 0.5 },
      scenarioTags: [],
      adaptiveEncouragement: 0.5,
      sessionId: 'session-2',
    });
    expect(result).toBeDefined();
    expect(result!.source).toBe('core');
    expect(result!.emotionType).toBe('encouragement');
  });
});

describe('CatchphraseSelector - 一致性检查（架构不变量 D1）', () => {
  let coreManager: CoreCatchphraseManager;
  let relationalManager: RelationalCatchphraseManager;
  let emotionTrigger: EmotionTriggerMatcher;
  let scenarioTrigger: ScenarioTriggerMatcher;
  let frequencyGovernor: FrequencyGovernor;
  let selector: CatchphraseSelector;

  beforeEach(async () => {
    coreManager = new CoreCatchphraseManager();
    await coreManager.load();
    relationalManager = new RelationalCatchphraseManager(3, 0.7);
    emotionTrigger = new EmotionTriggerMatcher(0.2, 0.6);
    scenarioTrigger = new ScenarioTriggerMatcher();
    frequencyGovernor = new FrequencyGovernor(1);
  });

  it('一致性校验通过时应返回口头禅', () => {
    const checker: ConsistencyChecker = { checkConsistency: () => true };
    selector = new CatchphraseSelector(coreManager, relationalManager, emotionTrigger, scenarioTrigger, frequencyGovernor, checker);
    const result = selector.select({
      currentVAD: { valence: 0.1, arousal: 0.5, dominance: 0.5 },
      baselineVAD: { valence: 0.5, arousal: 0.5, dominance: 0.5 },
      scenarioTags: [],
      adaptiveEncouragement: 0.5,
      sessionId: 'session-1',
    });
    expect(result).toBeDefined();
  });

  it('一致性校验失败时应返回 undefined', () => {
    const checker: ConsistencyChecker = { checkConsistency: () => false };
    selector = new CatchphraseSelector(coreManager, relationalManager, emotionTrigger, scenarioTrigger, frequencyGovernor, checker);
    const result = selector.select({
      currentVAD: { valence: 0.1, arousal: 0.5, dominance: 0.5 },
      baselineVAD: { valence: 0.5, arousal: 0.5, dominance: 0.5 },
      scenarioTags: [],
      adaptiveEncouragement: 0.5,
      sessionId: 'session-2',
    });
    expect(result).toBeUndefined();
  });

  it('setConsistencyChecker 应能动态更换校验器', () => {
    selector = new CatchphraseSelector(coreManager, relationalManager, emotionTrigger, scenarioTrigger, frequencyGovernor);
    const failChecker: ConsistencyChecker = { checkConsistency: () => false };
    selector.setConsistencyChecker(failChecker);
    const result = selector.select({
      currentVAD: { valence: 0.1, arousal: 0.5, dominance: 0.5 },
      baselineVAD: { valence: 0.5, arousal: 0.5, dominance: 0.5 },
      scenarioTags: [],
      adaptiveEncouragement: 0.5,
      sessionId: 'session-3',
    });
    expect(result).toBeUndefined();
  });
});

describe('CatchphraseSelector - 语气强度调制（架构不变量 D6）', () => {
  let coreManager: CoreCatchphraseManager;
  let relationalManager: RelationalCatchphraseManager;
  let emotionTrigger: EmotionTriggerMatcher;
  let scenarioTrigger: ScenarioTriggerMatcher;
  let frequencyGovernor: FrequencyGovernor;
  let selector: CatchphraseSelector;

  beforeEach(async () => {
    coreManager = new CoreCatchphraseManager();
    await coreManager.load();
    relationalManager = new RelationalCatchphraseManager(3, 0.7);
    emotionTrigger = new EmotionTriggerMatcher(0.2, 0.6);
    scenarioTrigger = new ScenarioTriggerMatcher();
    frequencyGovernor = new FrequencyGovernor(1);
    selector = new CatchphraseSelector(coreManager, relationalManager, emotionTrigger, scenarioTrigger, frequencyGovernor);
  });

  it('高 adaptiveEncouragement 应提升强度', () => {
    const result = selector.select({
      currentVAD: { valence: 0.1, arousal: 0.5, dominance: 0.5 },
      baselineVAD: { valence: 0.5, arousal: 0.5, dominance: 0.5 },
      scenarioTags: [],
      adaptiveEncouragement: 0.9,
      sessionId: 'session-high',
    });
    expect(result).toBeDefined();
    // high adaptiveEncouragement(0.9) → gain = 1.0+(0.9-0.5)*0.4 = 1.16
    // low 可能被提升到 medium
    expect(result!.intensity === 'medium' || result!.intensity === 'high').toBe(true);
  });
});
