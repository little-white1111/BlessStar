import React, { useEffect } from 'react';
import { Canvas } from './components/Canvas/Canvas';
import { useComponentStore, type ComponentSchemaField } from './stores/componentStore';
import { useLayerStore } from './stores/layerStore';
import { useUIStore } from './stores/uiStore';

/** 示例组件注册表（Phase 2 将从文件系统发现） */
const BUILTIN_PALETTE = [
  { tagName: 'task-list', name: '任务列表', category: '展示类', description: '显示任务事项列表' },
  { tagName: 'danmaku-list', name: '弹幕列表', category: '展示类', description: '显示直播间弹幕' },
  { tagName: 'live-bg', name: '直播间背景', category: '背景类', description: '自定义背景样式' },
  { tagName: 'music-lyrics', name: '音乐歌词', category: '展示类', description: '显示当前播放歌词' },
  { tagName: 'donation-effect', name: '打赏动画', category: '特效类', description: '打赏时的动画效果' },
];

// ========== 组件面板 ==========

const ComponentPalette: React.FC = () => {
  const { showComponentPalette, panelSizes } = useUIStore();
  const addComponent = useComponentStore((s) => s.addComponent);
  const addLayer = useLayerStore((s) => s.addLayer);

  if (!showComponentPalette) return null;

  const handleDragStart = (e: React.DragEvent, tagName: string, name: string) => {
    e.dataTransfer.setData('application/livestyle-component', JSON.stringify({ tagName, name }));
    e.dataTransfer.effectAllowed = 'copy';
  };

  const handleClick = (tagName: string, name: string) => {
    const comp = addComponent(tagName, name);
    addLayer(comp.id, comp.name);
  };

  return (
    <div style={{ width: panelSizes.componentPalette, ...panelBaseStyle, borderRight: '1px solid #374151' }}>
      <PanelHeader title="组件" />
      <div style={{ padding: '8px 12px' }}>
        <input
          placeholder="搜索组件..."
          style={{
            width: '100%', padding: '6px 8px', background: '#374151',
            border: '1px solid #4b5563', borderRadius: 4, color: '#d1d5db',
            fontSize: 12, outline: 'none',
          }}
        />
      </div>
      {['展示类', '背景类', '特效类'].map((cat) => (
        <div key={cat} style={{ padding: '0 12px 8px' }}>
          <div style={{ fontSize: 11, color: '#6b7280', marginBottom: 6, textTransform: 'uppercase', letterSpacing: 1 }}>
            {cat}
          </div>
          {BUILTIN_PALETTE.filter((c) => c.category === cat).map((comp) => (
            <div
              key={comp.tagName}
              draggable
              onDragStart={(e) => handleDragStart(e, comp.tagName, comp.name)}
              onClick={() => handleClick(comp.tagName, comp.name)}
              style={{
                padding: '6px 8px', marginBottom: 4, background: '#1f2937',
                border: '1px solid #374151', borderRadius: 4, cursor: 'grab',
                fontSize: 12, color: '#d1d5db',
                transition: 'background 0.15s',
              }}
              onMouseEnter={(e) => (e.currentTarget.style.background = '#374151')}
              onMouseLeave={(e) => (e.currentTarget.style.background = '#1f2937')}
              title={comp.description}
            >
              {comp.name}
            </div>
          ))}
        </div>
      ))}
    </div>
  );
};

// ========== 图层面板 ==========

const LayerPanel: React.FC = () => {
  const { showLayerPanel, panelSizes } = useUIStore();
  const layers = useLayerStore((s) => s.layers);
  const { toggleLayerVisibility, toggleLayerLock, removeLayer } = useLayerStore();
  const selectComponent = useComponentStore((s) => s.selectComponent);
  const removeComponent = useComponentStore((s) => s.removeComponent);
  const selectedComponentId = useComponentStore((s) => s.selectedComponentId);

  if (!showLayerPanel) return null;

  // 按 zIndex 降序排列（顶部在最前）
  const sortedLayers = [...layers].sort((a, b) => b.zIndex - a.zIndex);

  return (
    <div style={{ width: panelSizes.layerPanel, ...panelBaseStyle, borderRight: '1px solid #374151' }}>
      <PanelHeader title="图层" />
      <div style={{ flex: 1, overflowY: 'auto', padding: '4px 0' }}>
        {sortedLayers.length === 0 && (
          <div style={{ padding: 16, fontSize: 12, color: '#6b7280', textAlign: 'center' }}>
            暂无图层
          </div>
        )}
        {sortedLayers.map((layer) => (
          <div
            key={layer.id}
            onClick={() => selectComponent(layer.id)}
            style={{
              display: 'flex', alignItems: 'center', gap: 6,
              padding: '6px 12px', cursor: 'pointer',
              background: selectedComponentId === layer.id ? '#374151' : 'transparent',
              borderBottom: '1px solid #1f2937', fontSize: 12,
            }}
          >
            <span style={{ color: '#6b7280', fontSize: 10, width: 20 }}>
              {layer.zIndex}
            </span>
            <span style={{ flex: 1, color: '#d1d5db' }}>{layer.name}</span>
            <button
              onClick={(e) => { e.stopPropagation(); toggleLayerVisibility(layer.id); }}
              style={layerBtnStyle}
              title={layer.visible ? '隐藏' : '显示'}
            >
              {layer.visible ? '👁' : '👁‍🗨'}
            </button>
            <button
              onClick={(e) => { e.stopPropagation(); toggleLayerLock(layer.id); }}
              style={layerBtnStyle}
              title={layer.locked ? '解锁' : '锁定'}
            >
              {layer.locked ? '🔒' : '🔓'}
            </button>
            <button
              onClick={(e) => {
                e.stopPropagation();
                removeComponent(layer.id);
                removeLayer(layer.id);
              }}
              style={{ ...layerBtnStyle, color: '#ef4444' }}
              title="删除"
            >
              🗑
            </button>
          </div>
        ))}
      </div>
    </div>
  );
};

const layerBtnStyle: React.CSSProperties = {
  background: 'none', border: 'none', cursor: 'pointer',
  fontSize: 11, padding: '2px 4px', opacity: 0.6,
};

const PanelHeader: React.FC<{ title: string }> = ({ title }) => (
  <div style={{
    padding: '8px 12px', borderBottom: '1px solid #374151',
    fontWeight: 600, fontSize: 13, color: '#f3f4f6',
  }}>
    {title}
  </div>
);

// ========== 属性面板 ==========

const PropsPanel: React.FC = () => {
  const { showPropsPanel, panelSizes } = useUIStore();
  const selectedId = useComponentStore((s) => s.selectedComponentId);
  const components = useComponentStore((s) => s.components);
  const updateComponentProps = useComponentStore((s) => s.updateComponentProps);
  const schemas = useComponentStore((s) => s.schemas);

  if (!showPropsPanel) return null;

  const selected = components.find((c) => c.id === selectedId);
  if (!selected) return null;

  const schema = schemas.get(selected.tagName);

  return (
    <div style={{ width: panelSizes.propsPanel, ...panelBaseStyle, borderLeft: '1px solid #374151' }}>
      <PanelHeader title="属性" />
      <div style={{ flex: 1, overflowY: 'auto', padding: '12px 16px', fontSize: 12 }}>
        <div style={{ color: '#9ca3af', marginBottom: 12 }}>
          {selected.name}
          <span style={{ color: '#6b7280', marginLeft: 8, fontSize: 11 }}>
            ({selected.tagName})
          </span>
        </div>

        {/* 基础属性 */}
        <FieldRow label="X" value={selected.x} onChange={(v) => updateComponentProps(selected.id, { x: v })} min={0} />
        <FieldRow label="Y" value={selected.y} onChange={(v) => updateComponentProps(selected.id, { y: v })} min={0} />
        <FieldRow label="宽度" value={selected.width} onChange={(v) => updateComponentProps(selected.id, { width: v })} min={50} />
        <FieldRow label="高度" value={selected.height} onChange={(v) => updateComponentProps(selected.id, { height: v })} min={50} />
        <FieldRow label="旋转" value={selected.rotation ?? 0} onChange={(v) => updateComponentProps(selected.id, { rotation: v })} min={-360} max={360} />

        {/* Schema 定义的自定义属性 */}
        {schema && (
          <>
            <div style={{ borderTop: '1px solid #374151', margin: '12px 0', paddingTop: 12, color: '#6b7280', fontSize: 11, textTransform: 'uppercase' }}>
              {schema.name} 配置
            </div>
            {schema.fields.map((field) => (
              <div key={field.key} style={{ marginBottom: 10 }}>
                <label style={{ color: '#9ca3af', display: 'block', marginBottom: 4 }}>
                  {field.label}
                </label>
                <FieldRenderer
                  field={field}
                  value={selected.props?.[field.key] ?? field.default}
                  onChange={(v) => updateComponentProps(selected.id, { [field.key]: v })}
                />
              </div>
            ))}
          </>
        )}
      </div>
    </div>
  );
};

// ========== 字段渲染器 ==========

interface FieldRendererProps {
  field: ComponentSchemaField;
  value: unknown;
  onChange: (value: unknown) => void;
}

const FieldRenderer: React.FC<FieldRendererProps> = ({ field, value, onChange }) => {
  const inputStyle: React.CSSProperties = {
    width: '100%', padding: '4px 6px', background: '#374151',
    border: '1px solid #4b5563', borderRadius: 4, color: '#d1d5db',
    fontSize: 12, outline: 'none',
  };

  switch (field.type) {
    case 'string':
      return (
        <input
          style={inputStyle}
          value={String(value ?? '')}
          onChange={(e) => onChange(e.target.value)}
        />
      );
    case 'number':
      return (
        <input
          type="number"
          style={inputStyle}
          value={Number(value ?? 0)}
          min={field.min}
          max={field.max}
          onChange={(e) => onChange(Number(e.target.value))}
        />
      );
    case 'boolean':
      return (
        <label style={{ display: 'flex', alignItems: 'center', gap: 6, cursor: 'pointer', color: '#d1d5db' }}>
          <input
            type="checkbox"
            checked={Boolean(value)}
            onChange={(e) => onChange(e.target.checked)}
          />
          {field.label}
        </label>
      );
    case 'color':
      return (
        <div style={{ display: 'flex', gap: 6, alignItems: 'center' }}>
          <input
            type="color"
            value={String(value ?? '#000000')}
            onChange={(e) => onChange(e.target.value)}
            style={{ width: 32, height: 26, padding: 0, border: 'none', cursor: 'pointer' }}
          />
          <input
            style={{ ...inputStyle, flex: 1 }}
            value={String(value ?? '')}
            onChange={(e) => onChange(e.target.value)}
          />
        </div>
      );
    case 'select':
      return (
        <select
          style={inputStyle}
          value={String(value ?? '')}
          onChange={(e) => onChange(e.target.value)}
        >
          {field.options?.map((opt) => (
            <option key={opt.value} value={opt.value}>{opt.label}</option>
          ))}
        </select>
      );
    case 'textarea':
      return (
        <textarea
          style={{ ...inputStyle, minHeight: 60, resize: 'vertical' }}
          value={String(value ?? '')}
          onChange={(e) => onChange(e.target.value)}
        />
      );
    default:
      return (
        <input
          style={inputStyle}
          value={String(value ?? '')}
          onChange={(e) => onChange(e.target.value)}
        />
      );
  }
};

/** 简单数值行 */
const FieldRow: React.FC<{
  label: string; value: number; onChange: (v: number) => void;
  min?: number; max?: number;
}> = ({ label, value, onChange, min, max }) => (
  <div style={{ display: 'flex', alignItems: 'center', marginBottom: 8, gap: 8 }}>
    <span style={{ color: '#9ca3af', width: 40, fontSize: 12 }}>{label}</span>
    <input
      type="number"
      value={value}
      min={min}
      max={max}
      onChange={(e) => onChange(Number(e.target.value))}
      style={{
        flex: 1, padding: '4px 6px', background: '#374151',
        border: '1px solid #4b5563', borderRadius: 4, color: '#d1d5db',
        fontSize: 12, outline: 'none',
      }}
    />
  </div>
);

// ========== 应用主组件 ==========

const App: React.FC = () => {
  const components = useComponentStore((s) => s.components);
  const addComponent = useComponentStore((s) => s.addComponent);
  const selectedComponentId = useComponentStore((s) => s.selectedComponentId);
  const addLayer = useLayerStore((s) => s.addLayer);
  const registerSchema = useComponentStore((s) => s.registerSchema);

  // 初始注册 TaskList Schema
  useEffect(() => {
    registerSchema({
      tagName: 'task-list',
      name: '任务列表',
      fields: [
        { key: 'title', label: '标题', type: 'string', default: '任务列表' },
        { key: 'backgroundColor', label: '背景色', type: 'color', default: '#1a1a2e' },
        { key: 'fontSize', label: '字体大小', type: 'number', default: 14, min: 10, max: 48 },
        { key: 'textColor', label: '文字颜色', type: 'color', default: '#ffffff' },
        { key: 'items', label: '任务列表', type: 'textarea', default: '任务1,任务2,任务3' },
        { key: 'showHeader', label: '显示标题', type: 'boolean', default: true },
        { key: 'borderRadius', label: '圆角', type: 'number', default: 8, min: 0, max: 20 },
      ],
    });
  }, []);

  const handleAddComponent = (tagName: string, name: string) => {
    const comp = addComponent(tagName, name);
    addLayer(comp.id, comp.name);
  };

  // 画布 drop 处理
  const handleCanvasDrop = (e: React.DragEvent) => {
    const data = e.dataTransfer.getData('application/livestyle-component');
    if (!data) return;
    const { tagName, name } = JSON.parse(data);
    handleAddComponent(tagName, name);
  };

  // 组件画板 visible（选中组件且已注册 Schema）
  const showComponentCanvas = selectedComponentId && components.find(
    (c) => c.id === selectedComponentId && useComponentStore.getState().schemas.has(c.tagName),
  );

  return (
    <div
      style={{
        display: 'flex',
        width: '100vw',
        height: '100vh',
        overflow: 'hidden',
        fontFamily: 'system-ui, -apple-system, sans-serif',
        color: '#e5e7eb',
      }}
    >
      {/* 左侧：组件面板 */}
      <ComponentPalette />

      {/* 中间区域：主画布 + 组件画板 */}
      <div
        style={{ flex: 1, display: 'flex', flexDirection: 'column' }}
        onDrop={handleCanvasDrop}
        onDragOver={(e) => e.preventDefault()}
      >
        {/* 主画布 */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column' }}>
          <Canvas />
        </div>

        {/* 组件画板 iframe（Phase 2） */}
        {showComponentCanvas && (
          <div
            style={{
              height: 200,
              borderTop: '1px solid #374151',
              background: '#0f172a',
              display: 'flex',
              flexDirection: 'column',
            }}
          >
            <div
              style={{
                display: 'flex', justifyContent: 'space-between',
                padding: '4px 12px', background: '#1f2937',
                borderBottom: '1px solid #374151',
                fontSize: 11, color: '#6b7280',
              }}
            >
              <span>组件画板 — 实时预览</span>
              <span>编辑模式</span>
            </div>
            <iframe
              src="/engine-bridge.html"
              style={{ flex: 1, border: 'none' }}
              title="Component Canvas"
            />
          </div>
        )}
      </div>

      {/* 右侧：图层面板 + 属性面板 */}
      <div style={{ display: 'flex' }}>
        <LayerPanel />
        <PropsPanel />
      </div>
    </div>
  );
};

const panelBaseStyle: React.CSSProperties = {
  display: 'flex',
  flexDirection: 'column',
  background: '#1f2937',
  height: '100vh',
};

export default App;
