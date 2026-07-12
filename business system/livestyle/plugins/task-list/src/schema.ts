/**
 * TaskList 组件配置 Schema
 * 遵循架构不变量 I3：组件必须通过 Web Components 标准接口加载
 * 遵循架构不变量 I8：组件 SDK 必须零依赖打包
 */

import type { ComponentConfigSchema } from '@livestyle/plugin-sdk';

/**
 * 任务列表组件 Schema
 * 7 个配置字段，匹配 config-schema.yaml #16-22
 */
export const taskListSchema: ComponentConfigSchema = {
  name: '任务列表',
  version: '1.0.0',
  defaultProps: {
    title: '任务列表',
    backgroundColor: '#1a1a2e',
    fontSize: 14,
    textColor: '#ffffff',
    items: '任务1,任务2,任务3',
    showHeader: true,
    borderRadius: 8,
  },
  propsSchema: {
    title: {
      type: 'string',
      label: '标题',
      description: '组件标题文字',
      default: '任务列表',
    },
    backgroundColor: {
      type: 'color',
      label: '背景色',
      description: '组件背景颜色',
      default: '#1a1a2e',
    },
    fontSize: {
      type: 'number',
      label: '字体大小',
      description: '任务列表中文字的字号（px）',
      default: 14,
      min: 10,
      max: 48,
    },
    textColor: {
      type: 'color',
      label: '文字颜色',
      description: '任务列表中文字的颜色',
      default: '#ffffff',
    },
    items: {
      type: 'string',
      label: '任务列表',
      description: '逗号分隔的任务文字',
      default: '任务1,任务2,任务3',
    },
    showHeader: {
      type: 'boolean',
      label: '显示标题',
      description: '是否显示组件标题栏',
      default: true,
    },
    borderRadius: {
      type: 'number',
      label: '边框圆角',
      description: '组件整体边框圆角（px）',
      default: 8,
      min: 0,
      max: 20,
    },
  },
};
