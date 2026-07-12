/**
 * 配置存储（架构不变量 #5 — 所有用户数据仅存储在本地）
 * 架构不变量 #10 — 悬浮球位置变更实时持久化
 *
 * 基于 electron-store 实现 ConfigStore 接口，
 * 提供同步的键值对持久化能力。
 */
import ElectronStore from 'electron-store';
import { ConfigStore, ConfigValue } from '../../shared/config-schema';
/** electron-store 的 schema 定义（用于类型约束） */
interface StoreSchema {
    [key: string]: ConfigValue | undefined;
}
declare class ElectronConfigStore implements ConfigStore {
    private store;
    constructor();
    /**
     * 默认配置
     */
    private getDefaultConfig;
    /**
     * 获取配置值
     * @param key 配置键（支持点号分隔，如 'ui.floatBall.x'）
     */
    get<T extends ConfigValue>(key: string): T | undefined;
    /**
     * 设置配置值
     * 架构不变量 #10：悬浮球位置调用此方法时实时持久化到磁盘
     * @param key 配置键
     * @param value 配置值
     */
    set<T extends ConfigValue>(key: string, value: T): void;
    /**
     * 获取所有配置
     */
    getAll(): Record<string, ConfigValue>;
    /**
     * 重置指定配置到默认值
     * @param key 配置键
     */
    reset(key: string): void;
    /**
     * 获取 electron-store 原始实例
     */
    getRawStore(): ElectronStore<StoreSchema>;
}
/** 全局单例 */
export declare const configStore: ElectronConfigStore;
export {};
//# sourceMappingURL=config-store.d.ts.map