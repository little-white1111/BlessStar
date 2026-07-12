/**
 * 配置 Schema 类型定义（对应 config-schema.yaml）
 * 用于运行时类型安全和配置校验
 */

/** 配置字段类型 */
export type ConfigFieldType = 'I32' | 'F64' | 'STR' | 'BOOL' | 'ARR';

/** 配置字段值类型 */
export type ConfigValue = number | string | boolean | unknown[];

/** 单个配置字段定义 */
export interface ConfigField {
  key: string;
  type: ConfigFieldType;
  default: ConfigValue;
  required: boolean;
  business_desc: string;
  contract?: {
    range?: [number, number];
    dependencies?: string[];
    slo_impact?: string;
    approval_required?: boolean;
    enum_values?: string[];
  };
  ui_meta?: {
    label: string;
    order: number;
    hidden: boolean;
    widget?: string;
    step?: number;
    placeholder?: string;
    description?: string;
  };
}

/** 完整配置 Schema */
export interface ConfigSchema {
  domain: string;
  version: string;
  fields: ConfigField[];
}

/**
 * 运行时配置存储接口（架构不变量 #5 — 所有数据仅存储在本地）
 * 用于在 config-store.ts 中实现的键值对持久化
 */
export interface ConfigStore {
  get<T extends ConfigValue>(key: string): T | undefined;
  set<T extends ConfigValue>(key: string, value: T): void;
  getAll(): Record<string, ConfigValue>;
  reset(key: string): void;
}

/**
 * 获取配置字段的默认值（按照 Schema 定义）
 */
export function getDefaultValue(field: ConfigField): ConfigValue {
  return field.default;
}

/**
 * 校验配置值是否在 contract 约束范围内
 */
export function validateConfigValue(field: ConfigField, value: ConfigValue): string | null {
  if (field.contract?.range && typeof value === 'number') {
    const [min, max] = field.contract.range;
    if (value < min || value > max) {
      return `${field.ui_meta?.label || field.key} 的值必须在 ${min}~${max} 之间`;
    }
  }
  if (field.contract?.enum_values && typeof value === 'string') {
    if (!field.contract.enum_values.includes(value)) {
      return `${field.ui_meta?.label || field.key} 的值必须是以下之一: ${field.contract.enum_values.join(', ')}`;
    }
  }
  return null;
}
