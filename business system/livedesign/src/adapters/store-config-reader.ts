/**
 * StoreConfigReader — 桥接 ConfigReader 接口与 ElectronConfigStore
 *
 * 将 Port-Adapter 层的注册路径（如 "/config/livedesign/avatar/position_x"）
 * 映射到 ConfigStore 的点号分隔键（如 "avatar.position_x"）。
 *
 * 使用方式（在 main/index.ts 中）：
 *
 *   import { StoreConfigReader } from './adapters/store-config-reader';
 *   import { CachedReader } from './provider/cached-reader';
 *   import { provideBlessStarAdapters } from './provider';
 *
 *   const storeReader = new StoreConfigReader(configStore);
 *   const cachedReader = new CachedReader(storeReader, 30_000);
 *   const adapters = provideBlessStarAdapters(cachedReader);
 *
 * 架构不变量 #3：Adapter 构造接收 ConfigReader，依赖倒置
 * 架构不变量 #6：三阶段降级不受影响 — StoreConfigReader 返回 undefined 时
 *              适配器自动走缓存 → 硬编码默认值
 */

import { ConfigReader } from '../ports/config-reader';
import type { ConfigStore, ConfigValue } from '../shared/config-schema';

/** 业务系统标识，与 config-schema.yaml 中的 domain 保持一致 */
const BIZ_ID = 'livedesign';

/** 注册路径前缀，如 "/config/livedesign/" */
const REGISTRY_PREFIX = `/config/${BIZ_ID}/`;

export class StoreConfigReader implements ConfigReader {
  constructor(private store: ConfigStore) {}

  /**
   * 读取配置值。
   * 将注册路径映射为 ConfigStore 键名后读取。
   *
   * @param path 配置的完整注册路径（如 "/config/livedesign/avatar/position_x"）
   * @returns 配置值或 undefined（适配器三阶段降级会兜底）
   */
  async get(path: string): Promise<unknown> {
    try {
      const key = this.registryPathToKey(path);
      const val = this.store.get<ConfigValue>(key);
      return val;
    } catch {
      // 异常时返回 undefined，由 adapter 的三阶段降级兜底
      return undefined;
    }
  }

  /**
   * 将注册路径转换为 ConfigStore 键名。
   *
   * 转换规则：
   *   "/config/livedesign/avatar/position_x" → "avatar.position_x"
   *
   * 如果路径不以预期前缀开头，直接返回原路径（兼容直接访问）。
   *
   * @param path 注册路径
   * @returns ConfigStore 键名
   */
  private registryPathToKey(path: string): string {
    if (!path.startsWith(REGISTRY_PREFIX)) {
      // 非预期前缀直接返回，由 ConfigStore 自行处理
      return path;
    }
    // 去掉 "/config/livedesign/" 前缀，得到 "avatar/position_x"
    const withoutPrefix = path.slice(REGISTRY_PREFIX.length);
    // 将 "/" 替换为 "."，得到 "avatar.position_x"
    return withoutPrefix.replace(/\//g, '.');
  }
}
