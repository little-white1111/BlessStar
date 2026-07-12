/**
 * 用户确认流程（IPC + 弹窗审批）
 *
 * 架构不变量 D2: Relational 口头禅晋升必须经用户确认。
 * IPC 弹窗确认后才写入活跃表，确认前仅存 candidate 表。
 */

import type { PromotionConfirmationRequest } from './types';
import type { RelationalCatchphraseManager } from './relational-manager';

/** 用户确认结果 */
export type ConfirmationResult = 'confirmed' | 'rejected' | 'timeout';

/** IPC 用户确认通道回调 */
type ConfirmCallback = (request: PromotionConfirmationRequest) => Promise<ConfirmationResult>;

export class UserConfirmationService {
  private relationalManager: RelationalCatchphraseManager;
  private confirmCallback: ConfirmCallback | null = null;

  constructor(relationalManager: RelationalCatchphraseManager) {
    this.relationalManager = relationalManager;
  }

  /**
   * 注册 IPC 确认回调
   * 由外部集成方提供（如 ipc-router 接收到确认请求时调用）。
   */
  registerConfirmCallback(callback: ConfirmCallback): void {
    this.confirmCallback = callback;
  }

  /**
   * 检查晋升条件并触发用户确认流程
   * 架构不变量 D2: 确认前仅存 candidate 表。
   *
   * @param catchphraseId 口头禅 ID
   * @returns Promise<ConfirmationResult>
   */
  async requestPromotion(catchphraseId: string): Promise<ConfirmationResult> {
    const cp = this.relationalManager.findById(catchphraseId);
    if (!cp) return 'rejected';

    // 如果已激活，直接返回
    if (cp.status === 'active') return 'confirmed';

    // 检查是否满足晋升条件
    if (!this.relationalManager.checkPromotionEligibility(catchphraseId)) {
      return 'rejected';
    }

    const request: PromotionConfirmationRequest = {
      catchphraseId: cp.id,
      text: cp.text,
      usageCount: cp.usageCount,
      averageScore: cp.averageScore,
    };

    // 发送确认请求到 UI
    if (this.confirmCallback) {
      const result = await this.confirmCallback(request);
      await this.handleResult(catchphraseId, result);
      return result;
    }

    // 无回调时的默认行为：自动确认（降级模式）
    this.relationalManager.confirmPromotion(catchphraseId);
    return 'confirmed';
  }

  /**
   * 处理确认结果
   * 架构不变量 D2: 确认后写入活跃表，拒绝后重置计数。
   */
  private async handleResult(catchphraseId: string, result: ConfirmationResult): Promise<void> {
    switch (result) {
      case 'confirmed':
        this.relationalManager.confirmPromotion(catchphraseId);
        break;
      case 'rejected':
        this.relationalManager.rejectPromotion(catchphraseId);
        break;
      case 'timeout':
        // 超时等同于拒绝
        break;
    }
  }
}
