/**
 * CoreCatchphraseManager（核心口头禅管理器）
 *
 * 架构不变量 D5: Core 口头禅纯配置驱动不落盘。
 * 从 ConfigReader 读取，完全不可变（immutable）。
 */

import type { ConfigReader } from '../../ports/config-reader';
import type { CoreCatchphraseItem, CatchphraseIntensity } from './types';

/** 核心口头禅的配置路径 */
const CORE_CATCHPHRASE_PATH = '/config/livedesign/catchphrase.core';

/** 默认核心口头禅（与 config-schema.yaml 默认值一致） */
const DEFAULT_CORE_CATCHPHRASES: CoreCatchphraseItem[] = [
  {
    emotion_type: 'encouragement',
    variants: { low: '加油', medium: '你一定可以的', high: '你一定可以做到的！' },
  },
  {
    emotion_type: 'calming',
    variants: { low: '没事的', medium: '没关系的~', high: '别担心，我一直在这里' },
  },
];

export class CoreCatchphraseManager {
  private items: CoreCatchphraseItem[] = [];
  private loaded = false;

  constructor(private configReader?: ConfigReader) {}

  /**
   * 加载核心口头禅配置
   * 优先从 ConfigReader 读取，失败时使用默认值。
   */
  async load(): Promise<void> {
    if (this.loaded) return;

    if (this.configReader) {
      try {
        const raw = await this.configReader.get(CORE_CATCHPHRASE_PATH);
        if (Array.isArray(raw)) {
          this.items = raw as CoreCatchphraseItem[];
        } else if (typeof raw === 'string') {
          this.items = JSON.parse(raw) as CoreCatchphraseItem[];
        }
      } catch {
        // 配置读取失败，使用默认值
      }
    }

    if (this.items.length === 0) {
      this.items = DEFAULT_CORE_CATCHPHRASES;
    }
    this.loaded = true;
  }

  /**
   * 根据情绪类型获取核心口头禅
   * @param emotionType 情绪类型（如 encouragement, calming）
   * @param intensity 强度档位
   */
  getCatchphrase(emotionType: string, intensity: CatchphraseIntensity): string | undefined {
    const item = this.items.find((i) => i.emotion_type === emotionType);
    return item?.variants[intensity];
  }

  /**
   * 计算匹配的强度档位
   * 基于 VAD 偏差幅度：偏差小→low，偏差中→medium，偏差大→high
   * @param deviation VAD 偏差幅度
   */
  resolveIntensityByDeviation(deviation: number): CatchphraseIntensity {
    if (deviation > 0.6) return 'high';
    if (deviation > 0.3) return 'medium';
    return 'low';
  }

  /** 获取所有加载的核心口头禅 */
  getAll(): CoreCatchphraseItem[] {
    return [...this.items];
  }

  /** 检查是否已加载 */
  isLoaded(): boolean {
    return this.loaded;
  }

  /** 重置（用于测试） */
  reset(): void {
    this.items = [];
    this.loaded = false;
  }
}
