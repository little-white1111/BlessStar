/**
 * 画布主组件
 * 基于 fabric.js 实现拖拽、缩放、选中
 * 遵循 I9：图层管理与渲染顺序解耦
 */

import React, { useRef, useEffect } from 'react';
import { useCanvas } from '../../hooks/useCanvas';
import { useEditorStore } from '../../stores/editorStore';
import { useComponentStore } from '../../stores/componentStore';
import { useLayerStore } from '../../stores/layerStore';
import { EditorMode } from '../../types/editor';

export const Canvas: React.FC = () => {
  const canvasContainerRef = useRef<HTMLDivElement>(null);
  const { canvasRef, zoomTo } = useCanvas();
  const { zoom, mode } = useEditorStore();
  const selectedComponentId = useComponentStore((s) => s.selectedComponentId);

  // 处理画布点击取消选中
  const handleContainerClick = (e: React.MouseEvent) => {
    if ((e.target as HTMLElement).closest('.canvas-wrapper')) return;
    useComponentStore.getState().selectComponent(null);
  };

  // 键盘快捷键
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      const componentStore = useComponentStore.getState();
      const layerStore = useLayerStore.getState();
      const editorStore = useEditorStore.getState();

      switch (e.key) {
        case 'Delete':
        case 'Backspace':
          if (componentStore.selectedComponentId) {
            componentStore.removeComponent(componentStore.selectedComponentId);
            layerStore.removeLayer(componentStore.selectedComponentId);
          }
          break;
        case 'z':
        case 'Z':
          if (e.ctrlKey || e.metaKey) {
            e.preventDefault();
            if (e.shiftKey) {
              editorStore.redo();
            } else {
              editorStore.undo();
            }
          }
          break;
        case 'Escape':
          componentStore.selectComponent(null);
          break;
        case ' ':
          if (!e.repeat) {
            editorStore.setMode(
              editorStore.mode === EditorMode.HAND ? EditorMode.SELECT : EditorMode.HAND,
            );
          }
          break;
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, []);

  return (
    <div
      ref={canvasContainerRef}
      className="canvas-container"
      style={{
        flex: 1,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        background: '#111827',
      }}
      onClick={handleContainerClick}
    >
      {/* 工具栏 */}
      <div style={toolbarStyle}>
        <span style={{ color: '#9ca3af', fontSize: 12 }}>缩放:</span>
        <button onClick={() => zoomTo(zoom - 0.1)} style={btnStyle}>-</button>
        <span style={{ color: '#e5e7eb', fontSize: 13, minWidth: 50, textAlign: 'center' }}>
          {Math.round(zoom * 100)}%
        </span>
        <button onClick={() => zoomTo(zoom + 0.1)} style={btnStyle}>+</button>
        <button onClick={() => zoomTo(1)} style={btnStyle}>100%</button>
        <button onClick={() => zoomTo(-1)} style={btnStyle}>适应</button>

        <div style={{ width: 1, height: 20, background: '#374151', margin: '0 8px' }} />

        <button
          onClick={() => useEditorStore.getState().setMode(
            mode === EditorMode.HAND ? EditorMode.SELECT : EditorMode.HAND,
          )}
          style={{
            ...btnStyle,
            background: mode === EditorMode.HAND ? '#374151' : 'transparent',
          }}
        >
          {mode === EditorMode.HAND ? '🖐 抓手' : '🖱 选择'}
        </button>
      </div>

      {/* 画布区域 */}
      <div style={canvasAreaStyle}>
        <div className="canvas-wrapper" style={canvasWrapperStyle}>
          <canvas ref={canvasRef as React.Ref<HTMLCanvasElement>} width={1920} height={1080} />
        </div>
      </div>

      {/* 状态栏 */}
      <div style={statusBarStyle}>
        <span>模式: {mode === EditorMode.SELECT ? '选择' : '抓手'}</span>
        <span>选中: {selectedComponentId || '无'}</span>
        <span>1920 × 1080</span>
      </div>
    </div>
  );
};

const toolbarStyle: React.CSSProperties = {
  display: 'flex', alignItems: 'center', gap: 8,
  padding: '8px 16px', background: '#1f2937',
  borderBottom: '1px solid #374151',
};

const canvasAreaStyle: React.CSSProperties = {
  flex: 1, display: 'flex', justifyContent: 'center',
  alignItems: 'center', overflow: 'auto', padding: 24,
};

const canvasWrapperStyle: React.CSSProperties = {
  boxShadow: '0 4px 24px rgba(0,0,0,0.4)',
  borderRadius: 4, overflow: 'hidden',
};

const statusBarStyle: React.CSSProperties = {
  display: 'flex', justifyContent: 'space-between',
  padding: '4px 16px', background: '#1f2937',
  borderTop: '1px solid #374151',
  fontSize: 12, color: '#6b7280',
};

const btnStyle: React.CSSProperties = {
  padding: '4px 10px', background: 'transparent',
  color: '#d1d5db', border: '1px solid #374151',
  borderRadius: 4, cursor: 'pointer', fontSize: 12,
};
