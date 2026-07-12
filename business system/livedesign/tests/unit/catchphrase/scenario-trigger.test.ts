/**
 * ScenarioTriggerMatcher 单元测试
 */

import { describe, it, expect } from 'vitest';
import { ScenarioTriggerMatcher } from '../../../src/llm-service/catchphrase/scenario-trigger';
import type { RelationalCatchphrase } from '../../../src/llm-service/catchphrase/types';

function makeActiveCP(text: string, scenarioTag?: string): RelationalCatchphrase {
  return {
    id: `test-${text}`,
    text,
    usageCount: 5,
    totalFeedbackScore: 4,
    feedbackCount: 5,
    averageScore: 0.8,
    status: 'active',
    scenarioTag,
    createdAt: Date.now(),
  };
}

describe('ScenarioTriggerMatcher', () => {
  const matcher = new ScenarioTriggerMatcher();

  it('空标签应返回空数组', () => {
    const result = matcher.match([], [makeActiveCP('你好', 'greeting')]);
    expect(result).toHaveLength(0);
  });

  it('空口头禅列表应返回空数组', () => {
    const result = matcher.match(['greeting'], []);
    expect(result).toHaveLength(0);
  });

  it('精确匹配应返回置信度 1.0', () => {
    const cps = [makeActiveCP('你好', 'greeting')];
    const result = matcher.match(['greeting'], cps);
    expect(result).toHaveLength(1);
    expect(result[0].confidence).toBe(1.0);
    expect(result[0].catchphrase.text).toBe('你好');
  });

  it('模糊匹配应返回置信度 0.6', () => {
    const cps = [makeActiveCP('加油', 'encourage')];
    const result = matcher.match(['encouragement'], cps);
    expect(result).toHaveLength(1);
    expect(result[0].confidence).toBe(0.6);
  });

  it('多个匹配应按置信度降序排列', () => {
    const cps = [
      makeActiveCP('你好', 'greeting'),
      makeActiveCP('加油', 'encourage'),
    ];
    const result = matcher.match(['greeting', 'encouragement'], cps);
    expect(result).toHaveLength(2);
    expect(result[0].confidence).toBeGreaterThanOrEqual(result[1].confidence);
  });

  it('无 scenarioTag 的口头禅不应匹配', () => {
    const cps = [makeActiveCP('通用口头禅')];
    const result = matcher.match(['greeting'], cps);
    expect(result).toHaveLength(0);
  });
});
