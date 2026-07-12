/**
 * 此文件由 @blessstar/codegen-ts 自动生成
 * 源文件: ..\..\config-schema.yaml
 * 生成时间: 2026-07-11
 * 请勿手动编辑 — 下次运行 codegen 时将覆盖
 */

import type { LivestyleConfig } from './livestyle-config.types';

/**
 * ConfigPort — 配置端口接口
 * 遵循 I2：通过 Port 抽象解耦配置来源
 */
export interface ConfigPort {
  /**
   * 获取指定 key 的配置值
   * @param key - 配置键路径，如 "canvas.grid.size"
   */
  get<K extends keyof LivestyleConfig>(key: K): LivestyleConfig[K];

  /**
   * 批量获取配置快照
   */
  getAll(): LivestyleConfig;

  /**
   * 设置配置值
   * @param key - 配置键路径
   * @param value - 配置值
   */
  set<K extends keyof LivestyleConfig>(key: K, value: LivestyleConfig[K]): void;

  /**
   * 订阅配置变更
   * @param key - 配置键路径（可选，不传则监听所有变更）
   * @param listener - 变更回调
   * @returns 取消订阅函数
   */
  subscribe<K extends keyof LivestyleConfig>(
    key: K | '*',
    listener: (newValue: LivestyleConfig[K], oldValue: LivestyleConfig[K]) => void,
  ): () => void;
}
