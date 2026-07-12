/**
 * TaskList 组件入口
 * 使用 @livestyle/plugin-sdk 定义 Web Components
 * 遵循架构不变量 I3：组件必须通过 Web Components 标准接口加载
 * 遵循架构不变量 I8：组件 SDK 必须零依赖打包
 */

import { defineComponent } from '@livestyle/plugin-sdk';
import { taskListSchema } from './schema';
import { renderTaskList } from './render';

const { register, tagName } = defineComponent({
  tagName: 'task-list',
  config: taskListSchema,
  render: (props) => {
    return renderTaskList(props);
  },
  hooks: {
    onMount: () => {
      console.log('[TaskList] 组件已挂载');
    },
    onUnmount: () => {
      console.log('[TaskList] 组件已卸载');
    },
    onPropsUpdate: (oldProps, newProps) => {
      console.log('[TaskList] 属性更新:', oldProps, '→', newProps);
    },
  },
});

// 自动注册（通过 engine-bridge 加载时生效）
register();

export { tagName, register };
export default register;
