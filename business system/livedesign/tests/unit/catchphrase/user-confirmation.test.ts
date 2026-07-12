/**
 * UserConfirmationService 单元测试
 *
 * 架构不变量 D2: Relational 口头禅晋升必须经用户确认。
 * IPC 弹窗确认后才写入活跃表，确认前仅存 candidate 表。
 */

import { describe, it, expect, beforeEach, vi } from 'vitest';
import { RelationalCatchphraseManager } from '../../../src/llm-service/catchphrase/relational-manager';
import { UserConfirmationService } from '../../../src/llm-service/catchphrase/user-confirmation';
import type { ConfirmationResult } from '../../../src/llm-service/catchphrase/user-confirmation';

describe('UserConfirmationService（架构不变量 D2）', () => {
  let mgr: RelationalCatchphraseManager;
  let service: UserConfirmationService;

  beforeEach(() => {
    mgr = new RelationalCatchphraseManager(3, 0.7);
    service = new UserConfirmationService(mgr);
  });

  it('不满足条件时 requestPromotion 应返回 rejected', async () => {
    const cp = mgr.recordUsage('加油');
    const result = await service.requestPromotion(cp.id);
    expect(result).toBe('rejected');
  });

  it('无回调时满足条件应自动确认', async () => {
    const cp = mgr.recordUsage('加油');
    mgr.recordUsage('加油');
    mgr.recordUsage('加油');
    mgr.submitFeedback(cp.id, 0.8);
    const result = await service.requestPromotion(cp.id);
    expect(result).toBe('confirmed');
    const updated = mgr.findByText('加油')!;
    expect(updated.status).toBe('active');
  });

  it('用户确认后应晋升为 active', async () => {
    const cp = mgr.recordUsage('加油');
    mgr.recordUsage('加油');
    mgr.recordUsage('加油');
    mgr.submitFeedback(cp.id, 0.8);

    service.registerConfirmCallback(async () => 'confirmed' as ConfirmationResult);
    const result = await service.requestPromotion(cp.id);
    expect(result).toBe('confirmed');
    const updated = mgr.findByText('加油')!;
    expect(updated.status).toBe('active');
  });

  it('用户拒绝后应重置计数', async () => {
    const cp = mgr.recordUsage('加油');
    mgr.recordUsage('加油');
    mgr.recordUsage('加油');
    mgr.submitFeedback(cp.id, 0.8);

    service.registerConfirmCallback(async () => 'rejected' as ConfirmationResult);
    const result = await service.requestPromotion(cp.id);
    expect(result).toBe('rejected');
    const updated = mgr.findByText('加油')!;
    expect(updated.usageCount).toBe(0);
  });

  it('超时等同于拒绝', async () => {
    const cp = mgr.recordUsage('加油');
    mgr.recordUsage('加油');
    mgr.recordUsage('加油');
    mgr.submitFeedback(cp.id, 0.8);

    service.registerConfirmCallback(async () => 'timeout' as ConfirmationResult);
    const result = await service.requestPromotion(cp.id);
    expect(result).toBe('timeout');
    const updated = mgr.findByText('加油')!;
    expect(updated.status).toBe('candidate');
  });

  it('已激活的口头禅应直接返回 confirmed', async () => {
    const cp = mgr.recordUsage('加油');
    mgr.recordUsage('加油');
    mgr.recordUsage('加油');
    mgr.submitFeedback(cp.id, 0.8);
    mgr.confirmPromotion(cp.id);

    const result = await service.requestPromotion(cp.id);
    expect(result).toBe('confirmed');
  });

  it('不存在的 ID 应返回 rejected', async () => {
    const result = await service.requestPromotion('nonexistent');
    expect(result).toBe('rejected');
  });
});
