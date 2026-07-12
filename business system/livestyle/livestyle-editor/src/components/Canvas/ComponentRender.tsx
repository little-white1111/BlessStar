/**
 * 组件渲染容器
 * 内嵌 Web Components 实例的画布渲染层
 * 遵循架构不变量 I3：组件通过 Web Components 标准接口加载
 */

import React, { useRef, useEffect } from 'react';
import type { CanvasComponent } from '../../types/component';

interface ComponentRenderProps {
  component: CanvasComponent;
}

/**
 * 组件渲染容器
 * 将组件 ID 映射到对应的 Web Components 实例
 * Phase 1 实现：渲染占位符 + 样式轮廓
 * Phase 2 实现：动态加载 Web Components 并传入配置
 */
export const ComponentRender: React.FC<ComponentRenderProps> = React.memo(
  ({ component }) => {
    const containerRef = useRef<HTMLDivElement>(null);

    useEffect(() => {
      if (!containerRef.current) return;
      const container = containerRef.current;

      // 尝试创建 Web Components 实例
      const tagName = component.tagName;
      if (customElements.get(tagName)) {
        // 组件已注册，创建实例
        const element = document.createElement(tagName);
        container.innerHTML = '';
        container.appendChild(element);
      } else {
        // 组件未注册 → 显示占位符
        container.innerHTML = `
          <div style="
            width:100%;height:100%;
            display:flex;align-items:center;justify-content:center;
            background:rgba(74,158,255,0.05);
            border:1px dashed rgba(74,158,255,0.3);
            border-radius:4px;
            color:#6b7280;
            font-size:12px;
            font-family:monospace;
          ">
            ${component.name}
            <br/>
            <span style="font-size:10px;color:#4b5563;">(${tagName})</span>
          </div>
        `;
      }

      return () => {
        container.innerHTML = '';
      };
    }, [component.tagName, component.id]);

    return (
      <div
        ref={containerRef}
        style={{
          width: '100%',
          height: '100%',
          pointerEvents: component.locked ? 'none' : 'auto',
          opacity: component.visible ? 1 : 0,
        }}
      />
    );
  },
);

ComponentRender.displayName = 'ComponentRender';
