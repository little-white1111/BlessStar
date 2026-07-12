/**
 * Store 层测试
 * 覆盖 componentStore、layerStore、uiStore 的核心操作
 * 注意：Zustand store 在测试环境中会跨测试保持状态，需在 beforeEach 中重置
 */

import { describe, it, expect, beforeEach } from 'vitest';
import { useComponentStore } from '../stores/componentStore';
import { useLayerStore } from '../stores/layerStore';
import { useUIStore } from '../stores/uiStore';

// ============================================================
// componentStore 测试
// ============================================================
describe('componentStore', () => {
  beforeEach(() => {
    // 重置 store 状态
    useComponentStore.setState({
      components: [],
      schemas: new Map(),
      selectedComponentId: null,
    });
  });

  describe('addComponent', () => {
    it('应返回包含 id / tagName / name 的对象', () => {
      const comp = useComponentStore.getState().addComponent('task-list', '我的任务列表');
      expect(comp).toHaveProperty('id');
      expect(comp.tagName).toBe('task-list');
      expect(comp.name).toBe('我的任务列表');
    });

    it('应将新组件添加到组件列表中', () => {
      useComponentStore.getState().addComponent('task-list', '任务1');
      useComponentStore.getState().addComponent('button', '按钮');
      expect(useComponentStore.getState().components).toHaveLength(2);
    });

    it('新组件的 zIndex 应等于当前组件数量', () => {
      useComponentStore.getState().addComponent('a', 'A');
      const comp2 = useComponentStore.getState().addComponent('b', 'B');
      expect(comp2.zIndex).toBe(1);
    });
  });

  describe('registerSchema', () => {
    it('应注册 schema 到 schemas Map', () => {
      const schema = {
        tagName: 'task-list',
        name: '任务列表',
        fields: [
          { key: 'title', label: '标题', type: 'string' as const, default: '' },
        ],
      };
      useComponentStore.getState().registerSchema(schema);
      const retrieved = useComponentStore.getState().getSchema('task-list');
      expect(retrieved).toEqual(schema);
    });

    it('getSchema 对不存在的 tagName 应返回 undefined', () => {
      expect(useComponentStore.getState().getSchema('nonexistent')).toBeUndefined();
    });
  });

  describe('removeComponent', () => {
    it('应移除指定组件', () => {
      const comp = useComponentStore.getState().addComponent('task-list', '待移除');
      expect(useComponentStore.getState().components).toHaveLength(1);

      useComponentStore.getState().removeComponent(comp.id);
      expect(useComponentStore.getState().components).toHaveLength(0);
    });

    it('移除已选中的组件时应清除 selectedComponentId', () => {
      const comp = useComponentStore.getState().addComponent('task-list', '选中');
      useComponentStore.getState().selectComponent(comp.id);
      expect(useComponentStore.getState().selectedComponentId).toBe(comp.id);

      useComponentStore.getState().removeComponent(comp.id);
      expect(useComponentStore.getState().selectedComponentId).toBeNull();
    });
  });

  describe('updateComponentProps', () => {
    it('应正确更新指定组件的 props', () => {
      const comp = useComponentStore.getState().addComponent('task-list', '测试');
      useComponentStore.getState().updateComponentProps(comp.id, { title: '新标题', fontSize: 16 });

      const updated = useComponentStore.getState().components.find((c) => c.id === comp.id);
      expect(updated?.props).toEqual({ title: '新标题', fontSize: 16 });
    });

    it('多次更新应合并 props', () => {
      const comp = useComponentStore.getState().addComponent('task-list', '测试');
      useComponentStore.getState().updateComponentProps(comp.id, { title: '标题' });
      useComponentStore.getState().updateComponentProps(comp.id, { fontSize: 18 });

      const updated = useComponentStore.getState().components.find((c) => c.id === comp.id);
      expect(updated?.props).toEqual({ title: '标题', fontSize: 18 });
    });
  });

  describe('selectComponent', () => {
    it('应更新 selectedComponentId', () => {
      const comp = useComponentStore.getState().addComponent('task-list', '选中');
      useComponentStore.getState().selectComponent(comp.id);
      expect(useComponentStore.getState().selectedComponentId).toBe(comp.id);
    });

    it('传入 null 应清除选中', () => {
      const comp = useComponentStore.getState().addComponent('task-list', '选中');
      useComponentStore.getState().selectComponent(comp.id);
      useComponentStore.getState().selectComponent(null);
      expect(useComponentStore.getState().selectedComponentId).toBeNull();
    });

    it('getSelectedComponent 应返回被选中的组件', () => {
      const comp = useComponentStore.getState().addComponent('task-list', '选中');
      useComponentStore.getState().selectComponent(comp.id);
      const selected = useComponentStore.getState().getSelectedComponent();
      expect(selected?.id).toBe(comp.id);
    });
  });
});

// ============================================================
// layerStore 测试
// ============================================================
describe('layerStore', () => {
  beforeEach(() => {
    useLayerStore.setState({ layers: [] });
  });

  describe('addLayer', () => {
    it('应添加新图层到列表', () => {
      useLayerStore.getState().addLayer('comp1', '图层1');
      expect(useLayerStore.getState().layers).toHaveLength(1);
      expect(useLayerStore.getState().layers[0].name).toBe('图层1');
    });

    it('新图层的 zIndex 应等于当前图层数量', () => {
      useLayerStore.getState().addLayer('comp1', '图层A');
      useLayerStore.getState().addLayer('comp2', '图层B');
      expect(useLayerStore.getState().layers[1].zIndex).toBe(1);
    });
  });

  describe('reorderLayers', () => {
    it('应重新排列图层并更新 zIndex', () => {
      useLayerStore.getState().addLayer('comp1', '第一个');
      useLayerStore.getState().addLayer('comp2', '第二个');
      useLayerStore.getState().addLayer('comp3', '第三个');

      // 将第二个移到最前面
      useLayerStore.getState().reorderLayers(1, 0);

      const layers = useLayerStore.getState().layers;
      expect(layers[0].id).toBe('comp2');
      expect(layers[0].zIndex).toBe(0);
      expect(layers[1].id).toBe('comp1');
      expect(layers[1].zIndex).toBe(1);
      expect(layers[2].id).toBe('comp3');
      expect(layers[2].zIndex).toBe(2);
    });
  });

  describe('toggleLayerVisibility', () => {
    it('应翻转指定图层的 visible 状态', () => {
      useLayerStore.getState().addLayer('comp1', '图层1');
      expect(useLayerStore.getState().layers[0].visible).toBe(true);

      useLayerStore.getState().toggleLayerVisibility('comp1');
      expect(useLayerStore.getState().layers[0].visible).toBe(false);

      useLayerStore.getState().toggleLayerVisibility('comp1');
      expect(useLayerStore.getState().layers[0].visible).toBe(true);
    });
  });

  describe('getOrderedLayers', () => {
    it('应按 zIndex 升序返回图层', () => {
      useLayerStore.getState().addLayer('comp1', '图层1'); // zIndex: 0
      useLayerStore.getState().addLayer('comp2', '图层2'); // zIndex: 1
      useLayerStore.getState().addLayer('comp3', '图层3'); // zIndex: 2

      // 把最后一个移到最前面
      useLayerStore.getState().reorderLayers(2, 0);

      const ordered = useLayerStore.getState().getOrderedLayers();
      expect(ordered.map((l) => l.id)).toEqual(['comp3', 'comp1', 'comp2']);
      expect(ordered[0].zIndex).toBe(0);
      expect(ordered[1].zIndex).toBe(1);
      expect(ordered[2].zIndex).toBe(2);
    });
  });
});

// ============================================================
// uiStore 测试
// ============================================================
describe('uiStore', () => {
  beforeEach(() => {
    useUIStore.setState({
      showLayerPanel: true,
      showComponentPalette: true,
      showPropsPanel: true,
      panelSizes: { layerPanel: 240, componentPalette: 240, propsPanel: 300 },
    });
  });

  describe('toggleLayerPanel', () => {
    it('应翻转 showLayerPanel', () => {
      expect(useUIStore.getState().showLayerPanel).toBe(true);
      useUIStore.getState().toggleLayerPanel();
      expect(useUIStore.getState().showLayerPanel).toBe(false);
      useUIStore.getState().toggleLayerPanel();
      expect(useUIStore.getState().showLayerPanel).toBe(true);
    });
  });
});
