/**
 * 插件 SDK 配置类型
 * 供组件开发者定义组件的配置 Schema
 */

/** 配置字段类型 */
export type ConfigFieldType =
  | 'string'
  | 'number'
  | 'boolean'
  | 'color'
  | 'select'
  | 'slider'
  | 'image'
  | 'font';

/** 配置字段定义 */
export interface ConfigFieldDefinition {
  key: string;
  type: ConfigFieldType;
  label: string;
  default: unknown;
  description?: string;
  required?: boolean;
  options?: { label: string; value: string }[];
  min?: number;
  max?: number;
  step?: number;
  placeholder?: string;
}

/** 组件配置 Schema（给属性面板用的元数据） */
export interface ComponentConfigSchema {
  fields: ConfigFieldDefinition[];
  groups?: ConfigFieldGroup[];
}

/** 配置字段分组 */
export interface ConfigFieldGroup {
  name: string;
  label: string;
  fields: string[];
  collapsed?: boolean;
}
