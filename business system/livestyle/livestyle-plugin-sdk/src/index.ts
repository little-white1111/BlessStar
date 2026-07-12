/**
 * livestyle-plugin-sdk 入口
 * 组件插件 SDK — 零依赖，仅 TypeScript + Web API
 * 第三方组件开发者通过此 SDK 定义和注册组件
 */

export { defineComponent } from './component/defineComponent';

export type {
  PropDefinition,
  PropsSchema,
  ComponentConfig,
  ComponentLifecycleHooks,
  ComponentRenderFunction,
  DefineComponentOptions,
  ComponentRegistration,
} from './types/component';
