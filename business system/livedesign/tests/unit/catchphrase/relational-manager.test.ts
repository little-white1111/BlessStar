/**
 * RelationalCatchphraseManager 单元测试
 *
 * 架构不变量 D2: Relational 口头禅晋升必须经用户确认。
 * 架构不变量 D5: Relational 计数/评分配持久化。
 */

import { describe, it, expect, beforeEach } from 'vitest';
import { RelationalCatchphraseManager } from '../../../src/llm-service/catchphrase/relational-manager';

describe('RelationalCatchphraseManager - 基本操作', () => {
  let mgr: RelationalCatchphraseManager;

  beforeEach(() => {
    mgr = new RelationalCatchphraseManager(3, 0.7);
  });

  it('初次记录使用应创建新口头禅', () => {
    const cp = mgr.recordUsage('加油');
    expect(cp.text).toBe('加油');
    expect(cp.usageCount).toBe(1);
    expect(cp.status).toBe('candidate');
    expect(cp.averageScore).toBe(0);
  });

  it('重复记录同一文本应递增计数', () => {
    mgr.recordUsage('加油');
    const cp = mgr.recordUsage('加油');
    expect(cp.usageCount).toBe(2);
  });

  it('submitFeedback 应正确计算平均分', () => {
    mgr.recordUsage('加油');
    const cp = mgr.findByText('加油')!;
    mgr.submitFeedback(cp.id, 0.8);
    mgr.submitFeedback(cp.id, 1.0);
    const updated = mgr.findByText('加油')!;
    expect(updated.feedbackCount).toBe(2);
    expect(updated.averageScore).toBe(0.9);
  });

  it('submitFeedback 应钳制评分范围', () => {
    mgr.recordUsage('加油');
    const cp = mgr.findByText('加油')!;
    mgr.submitFeedback(cp.id, 2.0);
    const updated = mgr.findByText('加油')!;
    expect(updated.averageScore).toBe(1.0);
  });

  it('findById 和 findByText 应正确查找', () => {
    const cp = mgr.recordUsage('测试');
    expect(mgr.findById(cp.id)).toBeDefined();
    expect(mgr.findByText('测试')).toBeDefined();
    expect(mgr.findByText('不存在')).toBeUndefined();
  });
});

describe('RelationalCatchphraseManager - 晋升流程（架构不变量 D2）', () => {
  let mgr: RelationalCatchphraseManager;

  beforeEach(() => {
    mgr = new RelationalCatchphraseManager(3, 0.7);
  });

  it('不满条件时 checkPromotionEligibility 应返回 false', () => {
    const cp = mgr.recordUsage('加油');
    expect(mgr.checkPromotionEligibility(cp.id)).toBe(false);
  });

  it('使用次数达标但反馈分不足应返回 false', () => {
    const cp = mgr.recordUsage('加油');
    mgr.recordUsage('加油');
    mgr.recordUsage('加油');
    expect(mgr.checkPromotionEligibility(cp.id)).toBe(false);
  });

  it('使用次数和反馈分均达标应返回 true', () => {
    const cp = mgr.recordUsage('加油');
    for (let i = 0; i < 2; i++) mgr.recordUsage('加油');
    mgr.submitFeedback(cp.id, 0.8);
    mgr.submitFeedback(cp.id, 0.9);
    const updated = mgr.findByText('加油')!;
    expect(updated.usageCount).toBe(3);
    expect(updated.averageScore).toBeGreaterThanOrEqual(0.7);
    expect(mgr.checkPromotionEligibility(cp.id)).toBe(true);
  });

  it('confirmPromotion 应将 status 改为 active', () => {
    const cp = mgr.recordUsage('加油');
    for (let i = 0; i < 2; i++) mgr.recordUsage('加油');
    mgr.submitFeedback(cp.id, 1.0);
    mgr.submitFeedback(cp.id, 0.8);
    const promoted = mgr.confirmPromotion(cp.id);
    expect(promoted).toBeDefined();
    expect(promoted!.status).toBe('active');
  });

  it('rejectPromotion 应重置计数', () => {
    const cp = mgr.recordUsage('加油');
    for (let i = 0; i < 2; i++) mgr.recordUsage('加油');
    mgr.submitFeedback(cp.id, 0.8);
    mgr.rejectPromotion(cp.id);
    const rejected = mgr.findByText('加油')!;
    expect(rejected.usageCount).toBe(0);
    expect(rejected.averageScore).toBe(0);
  });

  it('getActive 应只返回 active 状态的口头禅', () => {
    mgr.recordUsage('候选');
    const cp = mgr.recordUsage('活跃');
    for (let i = 0; i < 2; i++) mgr.recordUsage('活跃');
    mgr.submitFeedback(cp.id, 0.8);
    mgr.confirmPromotion(cp.id);
    const active = mgr.getActive();
    expect(active).toHaveLength(1);
    expect(active[0].text).toBe('活跃');
  });

  it('getCandidates 应只返回 candidate 状态的口头禅', () => {
    mgr.recordUsage('候选1');
    mgr.recordUsage('候选2');
    const candidates = mgr.getCandidates();
    expect(candidates).toHaveLength(2);
  });
});

describe('RelationalCatchphraseManager - 场景标签', () => {
  let mgr: RelationalCatchphraseManager;

  beforeEach(() => {
    mgr = new RelationalCatchphraseManager(3, 0.7);
  });

  it('recordUsage 应记录场景标签', () => {
    const cp = mgr.recordUsage('早上好', 'morning');
    expect(cp.scenarioTag).toBe('morning');
  });

  it('findByScenarioTag 应返回匹配的口头禅', () => {
    mgr.recordUsage('早上好', 'morning');
    const cp1 = mgr.recordUsage('晚安', 'night');
    for (let i = 0; i < 2; i++) mgr.recordUsage('晚安');
    mgr.submitFeedback(cp1.id, 0.8);
    mgr.confirmPromotion(cp1.id);
    const morning = mgr.findByScenarioTag('morning');
    expect(morning).toHaveLength(0); // 未晋升
    const night = mgr.findByScenarioTag('night');
    expect(night).toHaveLength(1);
  });
});
