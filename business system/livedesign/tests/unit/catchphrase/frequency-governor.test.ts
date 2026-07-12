/**
 * FrequencyGovernor 单元测试
 *
 * 架构不变量 D3: FrequencyGovernor 强制每轮对话最多 1 次口头禅。
 */

import { describe, it, expect, beforeEach } from 'vitest';
import { FrequencyGovernor } from '../../../src/llm-service/catchphrase/frequency-governor';

describe('FrequencyGovernor（架构不变量 D3）', () => {
  let governor: FrequencyGovernor;

  beforeEach(() => {
    governor = new FrequencyGovernor(1);
  });

  it('初始状态应允许输出', () => {
    expect(governor.canOutput('session-1')).toBe(true);
  });

  it('记录一次输出后不应再允许输出', () => {
    governor.recordOutput('session-1');
    expect(governor.canOutput('session-1')).toBe(false);
  });

  it('不同 session 的计数器应独立', () => {
    governor.recordOutput('session-1');
    expect(governor.canOutput('session-1')).toBe(false);
    expect(governor.canOutput('session-2')).toBe(true);
  });

  it('resetSession 后应重新允许输出', () => {
    governor.recordOutput('session-1');
    expect(governor.canOutput('session-1')).toBe(false);
    governor.resetSession('session-1');
    expect(governor.canOutput('session-1')).toBe(true);
  });

  it('getOutputCount 应返回正确计数', () => {
    expect(governor.getOutputCount('session-1')).toBe(0);
    governor.recordOutput('session-1');
    expect(governor.getOutputCount('session-1')).toBe(1);
  });

  it('maxPerRound=2 时应允许 2 次输出', () => {
    const g2 = new FrequencyGovernor(2);
    expect(g2.canOutput('session-1')).toBe(true);
    g2.recordOutput('session-1');
    expect(g2.canOutput('session-1')).toBe(true);
    g2.recordOutput('session-1');
    expect(g2.canOutput('session-1')).toBe(false);
  });

  it('updateConfig 应动态更新最大次数', () => {
    governor.updateConfig(2);
    governor.recordOutput('session-1');
    expect(governor.canOutput('session-1')).toBe(true);
    governor.recordOutput('session-1');
    expect(governor.canOutput('session-1')).toBe(false);
  });

  it('clearAll 应清理所有 session', () => {
    governor.recordOutput('session-1');
    governor.recordOutput('session-2');
    governor.clearAll();
    expect(governor.canOutput('session-1')).toBe(true);
    expect(governor.canOutput('session-2')).toBe(true);
  });
});
