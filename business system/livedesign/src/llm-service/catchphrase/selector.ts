/**
 * CatchphraseSelector（口头禅选择器）
 *
 * 编排匹配器 + 优先级裁决。
 * 架构不变量 D1: 所有口头禅必须通过 ConsistencyChecker 验证。
 * 架构不变量 D6: 口头禅语气强度受 adaptive persona 当前值调制。
 */

import { CoreCatchphraseManager } from './core-manager';
import { RelationalCatchphraseManager } from './relational-manager';
import { EmotionTriggerMatcher } from './emotion-trigger';
import { ScenarioTriggerMatcher } from './scenario-trigger';
import { FrequencyGovernor } from './frequency-governor';
import type {
  CatchphraseMatch,
  CatchphraseIntensity,
  CatchphraseSelectionConfig,
} from './types';
import type { RelationalCatchphrase } from './types';

/** 一致性校验器接口 */
export interface ConsistencyChecker {
  /**
   * 验证口头禅是否与 core persona 一致
   * @param text 口头禅文本
   * @returns 是否通过一致性检查
   */
  checkConsistency(text: string): boolean;
}

/**
 * 默认一致性校验器（始终通过）
 * 当 persona 模块的 ConsistencyChecker 未就绪时使用。
 */
export class DefaultConsistencyChecker implements ConsistencyChecker {
  checkConsistency(_text: string): boolean {
    return true;
  }
}

export class CatchphraseSelector {
  private coreManager: CoreCatchphraseManager;
  private relationalManager: RelationalCatchphraseManager;
  private emotionTrigger: EmotionTriggerMatcher;
  private scenarioTrigger: ScenarioTriggerMatcher;
  private frequencyGovernor: FrequencyGovernor;
  private consistencyChecker: ConsistencyChecker;

  constructor(
    coreManager: CoreCatchphraseManager,
    relationalManager: RelationalCatchphraseManager,
    emotionTrigger: EmotionTriggerMatcher,
    scenarioTrigger: ScenarioTriggerMatcher,
    frequencyGovernor: FrequencyGovernor,
    consistencyChecker?: ConsistencyChecker,
  ) {
    this.coreManager = coreManager;
    this.relationalManager = relationalManager;
    this.emotionTrigger = emotionTrigger;
    this.scenarioTrigger = scenarioTrigger;
    this.frequencyGovernor = frequencyGovernor;
    this.consistencyChecker = consistencyChecker ?? new DefaultConsistencyChecker();
  }

  /**
   * 设置一致性校验器
   */
  setConsistencyChecker(checker: ConsistencyChecker): void {
    this.consistencyChecker = checker;
  }

  /**
   * 选择最佳口头禅
   * 架构不变量 D1: 输出前调用一致性检查。
   * 架构不变量 D3: FrequencyGovernor 每轮最多 1 次。
   * 架构不变量 D6: 语气强度受 adaptive persona 调制。
   *
   * @param config 选择配置
   * @returns 匹配结果（若无合适口头禅返回 undefined）
   */
  select(config: CatchphraseSelectionConfig): CatchphraseMatch | undefined {
    // 架构不变量 D3: 检查频率限制
    if (!this.frequencyGovernor.canOutput(config.sessionId)) {
      return undefined;
    }

    // Step 1: EmotionTrigger 匹配
    const emotionContext = this.emotionTrigger.computeContext(config.currentVAD, config.baselineVAD);
    const emotionMatch = this.tryEmotionMatch(emotionContext);

    // Step 2: ScenarioTrigger 匹配
    const activeRelational = this.relationalManager.getActive();
    const scenarioMatches = this.scenarioTrigger.match(config.scenarioTags, activeRelational);

    // Step 3: 裁决 — Emotion 匹配优先
    let bestMatch: CatchphraseMatch | undefined;

    if (emotionMatch) {
      bestMatch = emotionMatch;
    }

    // 如果没有 Emotion 匹配，尝试 Scenario 匹配
    if (!bestMatch && scenarioMatches.length > 0) {
      const sm = scenarioMatches[0];
      const text = sm.catchphrase.text;
      // 架构不变量 D1: 一致性检查
      if (this.consistencyChecker.checkConsistency(text)) {
        bestMatch = {
          text,
          source: 'relational',
          intensity: 'medium',
          confidence: sm.confidence,
          relationalId: sm.catchphrase.id,
        };
      }
    }

    // 如果没有匹配，尝试随机取一个活跃的 Relational 口头禅
    if (!bestMatch && activeRelational.length > 0) {
      const randomIdx = Math.floor(Math.random() * activeRelational.length);
      const randomCP = activeRelational[randomIdx];
      const text = randomCP.text;
      if (this.consistencyChecker.checkConsistency(text)) {
        bestMatch = {
          text,
          source: 'relational',
          intensity: 'medium',
          confidence: 0.3,
          relationalId: randomCP.id,
        };
      }
    }

    // 架构不变量 D1: Consistency Check（最终把关）
    if (bestMatch && !this.consistencyChecker.checkConsistency(bestMatch.text)) {
      return undefined;
    }

    // 架构不变量 D6: 语气强度调制
    if (bestMatch) {
      bestMatch = this.applyToneModulation(bestMatch, config.adaptiveEncouragement);
    }

    return bestMatch;
  }

  /**
   * 尝试 Emotion 匹配
   */
  private tryEmotionMatch(emotionContext: ReturnType<EmotionTriggerMatcher['computeContext']>): CatchphraseMatch | undefined {
    if (!this.emotionTrigger.isTriggered(emotionContext)) {
      return undefined;
    }

    const emotionType = this.emotionTrigger.matchEmotionType(emotionContext);
    if (!emotionType) return undefined;

    const intensity = this.coreManager.resolveIntensityByDeviation(emotionContext.deviation);
    const text = this.coreManager.getCatchphrase(emotionType, intensity);
    if (!text) return undefined;

    // 架构不变量 D1: 一致性检查
    if (!this.consistencyChecker.checkConsistency(text)) {
      return undefined;
    }

    return {
      text,
      source: 'core',
      intensity,
      confidence: 0.8,
      emotionType,
    };
  }

  /**
   * 架构不变量 D6: 语气强度受 adaptive persona 当前值调制。
   * 语气增益因子 = 1.0 + (adaptive_value - 0.5) × 0.4
   */
  private applyToneModulation(
    match: CatchphraseMatch,
    adaptiveEncouragement: number,
  ): CatchphraseMatch {
    // 计算语气增益因子
    const gain = 1.0 + (adaptiveEncouragement - 0.5) * 0.4;

    // 根据增益调整强度档位
    let adjustedIntensity: CatchphraseIntensity = match.intensity;
    if (gain > 1.15 && match.intensity === 'low') {
      adjustedIntensity = 'medium';
    } else if (gain > 1.25 && match.intensity === 'medium') {
      adjustedIntensity = 'high';
    } else if (gain < 0.85 && match.intensity === 'high') {
      adjustedIntensity = 'medium';
    } else if (gain < 0.75 && match.intensity === 'medium') {
      adjustedIntensity = 'low';
    }

    return {
      ...match,
      intensity: adjustedIntensity,
      confidence: match.confidence * Math.min(gain, 1.3),
    };
  }

  /** 获取各组件引用 */
  getCoreManager(): CoreCatchphraseManager { return this.coreManager; }
  getRelationalManager(): RelationalCatchphraseManager { return this.relationalManager; }
  getEmotionTrigger(): EmotionTriggerMatcher { return this.emotionTrigger; }
  getScenarioTrigger(): ScenarioTriggerMatcher { return this.scenarioTrigger; }
  getFrequencyGovernor(): FrequencyGovernor { return this.frequencyGovernor; }
}
