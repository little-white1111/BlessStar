/**
 * 画布交互 Hook
 * 封装 fabric.js 画布的操作逻辑
 */

import { useEffect, useRef, useCallback } from 'react';
import { Canvas as FabricCanvas, Rect, Text, Group, Point } from 'fabric';
import { useEditorStore } from '../stores/editorStore';
import { useComponentStore } from '../stores/componentStore';
import type { CanvasComponent } from '../types/component';

/** Canvas Hook 返回值 */
interface UseCanvasReturn {
  canvasRef: React.RefObject<HTMLCanvasElement | null>;
  addComponentToCanvas: (component: CanvasComponent) => void;
  removeComponentFromCanvas: (id: string) => void;
  updateComponentOnCanvas: (id: string, props: Partial<CanvasComponent>) => void;
  zoomTo: (zoom: number) => void;
}

/**
 * 画布交互 Hook
 * 管理 fabric.Canvas 实例，处理拖拽、选中、缩放等交互
 */
export function useCanvas(): UseCanvasReturn {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const fabricRef = useRef<FabricCanvas | null>(null);
  const objectsRef = useRef<Map<string, Group>>(new Map());

  const { zoom, mode } = useEditorStore();
  const { components, selectComponent, updateComponent } = useComponentStore();

  // 初始化 fabric.js 画布
  useEffect(() => {
    if (!canvasRef.current || fabricRef.current) return;

    const canvas = new FabricCanvas(canvasRef.current, {
      width: 1920,
      height: 1080,
      backgroundColor: '#1a1a2e',
      selection: true,
      preserveObjectStacking: true,
    });

    fabricRef.current = canvas;

    // 选中事件
    canvas.on('selection:created', (e: any) => {
      if (e.selected?.[0]) {
        const obj = e.selected[0];
        selectComponent(obj.name || null);
      }
    });

    canvas.on('selection:updated', (e: any) => {
      if (e.selected?.[0]) {
        selectComponent(e.selected[0].name || null);
      }
    });

    canvas.on('selection:cleared', () => {
      selectComponent(null);
    });

    // 对象移动/缩放事件
    canvas.on('object:moved' as any, (e: any) => {
      const obj = e.target;
      if (obj && obj.name) {
        updateComponent(obj.name, {
          x: obj.left || 0,
          y: obj.top || 0,
        });
      }
    });

    canvas.on('object:scaled' as any, (e: any) => {
      const obj = e.target;
      if (obj && obj.name) {
        updateComponent(obj.name, {
          width: (obj.width || 400) * (obj.scaleX || 1),
          height: (obj.height || 300) * (obj.scaleY || 1),
        });
      }
    });

    // 鼠标滚轮缩放
    canvas.on('mouse:wheel', (opt: any) => {
      const delta = opt.e.deltaY;
      let newZoom = canvas.getZoom();
      newZoom *= 0.999 ** delta;
      newZoom = Math.max(0.1, Math.min(5, newZoom));
      canvas.zoomToPoint(new Point(opt.e.offsetX, opt.e.offsetY), newZoom);
      useEditorStore.getState().setZoom(newZoom);
      opt.e.preventDefault();
      opt.e.stopPropagation();
    });

    return () => {
      canvas.dispose();
      fabricRef.current = null;
    };
  }, []);

  // 同步缩放
  useEffect(() => {
    const canvas = fabricRef.current;
    if (!canvas) return;
    canvas.setZoom(zoom);
    canvas.renderAll();
  }, [zoom]);

  // 同步模式
  useEffect(() => {
    const canvas = fabricRef.current;
    if (!canvas) return;

    switch (mode) {
      case 'select':
        canvas.selection = true;
        canvas.defaultCursor = 'default';
        canvas.isDrawingMode = false;
        break;
      case 'hand':
        canvas.selection = false;
        canvas.defaultCursor = 'grab';
        canvas.isDrawingMode = false;
        break;
      default:
        canvas.selection = true;
        canvas.defaultCursor = 'default';
    }
  }, [mode]);

  // 同步组件到画布
  useEffect(() => {
    const canvas = fabricRef.current;
    if (!canvas) return;

    const currentIds = new Set(objectsRef.current.keys());
    const newIds = new Set(components.map((c) => c.id));

    // 删除不再存在的对象
    for (const id of currentIds) {
      if (!newIds.has(id)) {
        const obj = objectsRef.current.get(id);
        if (obj) {
          canvas.remove(obj);
        }
        objectsRef.current.delete(id);
      }
    }

    // 添加或更新对象
    for (const comp of components) {
      const existing = objectsRef.current.get(comp.id);

      if (existing) {
        existing.set({
          left: comp.x,
          top: comp.y,
          width: comp.width,
          height: comp.height,
          visible: comp.visible,
          selectable: !comp.locked,
          evented: !comp.locked,
        } as any);
        existing.setCoords();
      } else if (comp.visible) {
        const rect = new Rect({
          left: comp.x,
          top: comp.y,
          width: comp.width,
          height: comp.height,
          fill: 'rgba(255, 255, 255, 0.05)',
          stroke: '#4a9eff',
          strokeWidth: 2,
          strokeDashArray: [6, 3],
        });

        const label = new Text(comp.name, {
          left: comp.x + 8,
          top: comp.y + 8,
          fontSize: 12,
          fill: '#4a9eff',
          fontFamily: 'monospace',
        });

        const group = new Group([rect, label], {
          name: comp.id,
          selectable: !comp.locked,
          evented: !comp.locked,
        } as any);

        canvas.add(group);
        objectsRef.current.set(comp.id, group);
      }
    }

    canvas.renderAll();
  }, [components]);

  const addComponentToCanvas = useCallback((component: CanvasComponent) => {
    const canvas = fabricRef.current;
    if (!canvas) return;

    const rect = new Rect({
      left: component.x,
      top: component.y,
      width: component.width,
      height: component.height,
      fill: 'rgba(255, 255, 255, 0.05)',
      stroke: '#4a9eff',
      strokeWidth: 2,
      strokeDashArray: [6, 3],
    });

    const label = new Text(component.name, {
      left: component.x + 8,
      top: component.y + 8,
      fontSize: 12,
      fill: '#4a9eff',
      fontFamily: 'monospace',
    });

    const group = new Group([rect, label], {
      name: component.id,
      selectable: true,
    } as any);

    canvas.add(group);
    objectsRef.current.set(component.id, group);
    canvas.renderAll();
  }, []);

  const removeComponentFromCanvas = useCallback((id: string) => {
    const canvas = fabricRef.current;
    if (!canvas) return;

    const obj = objectsRef.current.get(id);
    if (obj) {
      canvas.remove(obj);
      objectsRef.current.delete(id);
      canvas.renderAll();
    }
  }, []);

  const updateComponentOnCanvas = useCallback(
    (id: string, props: Partial<CanvasComponent>) => {
      const canvas = fabricRef.current;
      if (!canvas) return;

      const obj = objectsRef.current.get(id);
      if (obj) {
        const changes: any = {};
        if (props.x !== undefined) changes.left = props.x;
        if (props.y !== undefined) changes.top = props.y;
        if (props.width !== undefined) changes.width = props.width;
        if (props.height !== undefined) changes.height = props.height;
        if (props.visible !== undefined) changes.visible = props.visible;
        if (props.locked !== undefined) {
          changes.selectable = !props.locked;
          changes.evented = !props.locked;
        }
        obj.set(changes);
        obj.setCoords();
        canvas.renderAll();
      }
    },
    [],
  );

  const zoomTo = useCallback((zoomLevel: number) => {
    const canvas = fabricRef.current;
    if (!canvas) return;
    const clamped = Math.max(0.1, Math.min(5, zoomLevel));
    canvas.setZoom(clamped);
    canvas.renderAll();
    useEditorStore.getState().setZoom(clamped);
  }, []);

  return {
    canvasRef,
    addComponentToCanvas,
    removeComponentFromCanvas,
    updateComponentOnCanvas,
    zoomTo,
  };
}
