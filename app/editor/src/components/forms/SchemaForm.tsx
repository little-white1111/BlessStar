import { useCallback, useState } from 'react'
import type { UIDLNode, FormValues } from '../../types/uidl'
import type { VersionRegistry } from '../../ai/types'
import InputControl from './InputControl'
import SelectControl from './SelectControl'
import CheckboxControl from './CheckboxControl'
import RadioControl from './RadioControl'
import NumberControl from './NumberControl'
import TextareaControl from './TextareaControl'
import GroupControl from './GroupControl'

interface SchemaFormProps {
  fields: UIDLNode[]
  values: FormValues
  onChange: (values: FormValues) => void
  parentKey?: string
  /** 版本注册表（第33天 · 配置编辑器版本下拉） */
  versionRegistry?: VersionRegistry
}

function SchemaForm({ fields, values, onChange, parentKey = '', versionRegistry }: SchemaFormProps) {
  const updateValue = useCallback(
    (key: string, value: unknown) => {
      const fullKey = parentKey ? `${parentKey}.${key}` : key
      const keys = fullKey.split('.')
      const newValues = { ...values }

      let current: Record<string, unknown> = newValues
      for (let i = 0; i < keys.length - 1; i++) {
        if (!current[keys[i]] || typeof current[keys[i]] !== 'object') {
          current[keys[i]] = {}
        }
        current[keys[i]] = { ...(current[keys[i]] as Record<string, unknown>) }
        current = current[keys[i]] as Record<string, unknown>
      }
      current[keys[keys.length - 1]] = value

      onChange(newValues)
    },
    [values, onChange, parentKey],
  )

  const sortedFields = [...fields].sort((a, b) => (a.order ?? 999) - (b.order ?? 999))

  return (
    <div className="space-y-6">
      {sortedFields.map((field) => (
        <SchemaField
          key={field.key}
          field={field}
          value={getNestedValue(values, field.key)}
          onChange={(val) => updateValue(field.key, val)}
          values={values}
          onValuesChange={onChange}
          parentKey={field.key}
          versionRegistry={versionRegistry}
          configKey={parentKey ? `${parentKey}.${field.key}` : field.key}
        />
      ))}
    </div>
  )
}

interface SchemaFieldProps {
  field: UIDLNode
  value: unknown
  onChange: (value: unknown) => void
  values: FormValues
  onValuesChange: (values: FormValues) => void
  parentKey?: string
  /** 版本注册表（第33天 · 配置编辑器版本下拉） */
  versionRegistry?: VersionRegistry
  /** 当前字段的完整配置 key */
  configKey?: string
}

function SchemaField({ field, value, onChange, values, onValuesChange, parentKey: _parentKey, versionRegistry, configKey }: SchemaFieldProps) {
  // ── 版本下拉（第33天）─────────────────────────────────────────
  const versions = (configKey && versionRegistry?.[configKey]) || []
  const [showVersionDropdown, setShowVersionDropdown] = useState(false)
  const [versionFilter, setVersionFilter] = useState('')
  const filteredVersions = versionFilter
    ? versions.filter((v) =>
        (v.displayName || v.versionId).toLowerCase().includes(versionFilter.toLowerCase()),
      )
    : versions

  let control: React.ReactNode
  switch (field.widget) {
    case 'input':
      control = <InputControl field={field} value={value as string} onChange={onChange} />
      break
    case 'select':
      control = <SelectControl field={field} value={value as string} onChange={onChange} />
      break
    case 'checkbox':
      control = <CheckboxControl field={field} value={value as boolean} onChange={onChange} />
      break
    case 'radio':
      control = <RadioControl field={field} value={value as string} onChange={onChange} />
      break
    case 'number':
      control = <NumberControl field={field} value={value as number} onChange={onChange} />
      break
    case 'textarea':
      control = <TextareaControl field={field} value={value as string} onChange={onChange} />
      break
    case 'group':
      control = (
        <GroupControl
          field={field}
          value={value as Record<string, unknown>}
          onChange={onChange}
          allValues={values}
          onAllValuesChange={onValuesChange}
        />
      )
      break
    case 'repeatable':
      control = (
        <RepeatableControl
          field={field}
          value={value as Record<string, unknown>[]}
          onChange={onChange}
          allValues={values}
          onAllValuesChange={onValuesChange}
        />
      )
      break
    default:
      control = <div className="text-red-500 text-sm">未知控件类型: {field.widget}</div>
  }

  return (
    <div className="space-y-1">
      {control}
      {versions.length > 0 && (
        <div className="relative">
          <div className="flex items-center gap-1 mt-1">
            <span className="text-xs text-surface-400 dark:text-surface-500">版本:</span>
            <button
              onClick={() => setShowVersionDropdown(!showVersionDropdown)}
              className="text-xs px-2 py-0.5 rounded border border-surface-200 dark:border-surface-600 text-surface-500 dark:text-surface-400 hover:text-surface-700 dark:hover:text-surface-200 flex items-center gap-1"
            >
              {(() => {
                const currentVer = versions.find((v) => v.value === String(value))
                return (currentVer?.displayName || currentVer?.versionId || '最新')
              })()}
              <svg className="w-3 h-3" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 9l-7 7-7-7" />
              </svg>
            </button>
          </div>
          {showVersionDropdown && (
            <div className="absolute left-0 top-full mt-1 z-30 min-w-[240px] rounded-lg p-2 shadow-xl border border-surface-200 dark:border-surface-600 bg-white dark:bg-surface-800">
              <input
                autoFocus
                value={versionFilter}
                onChange={(e) => setVersionFilter(e.target.value)}
                placeholder="搜索版本或输入版本号..."
                className="w-full px-2 py-1.5 text-xs rounded border border-surface-200 dark:border-surface-600 bg-surface-50 dark:bg-surface-700 text-surface-900 dark:text-surface-50 mb-2"
              />
              <div className="max-h-48 overflow-y-auto space-y-0.5">
                {filteredVersions.map((v) => (
                  <button
                    key={v.versionId}
                    onClick={() => { onChange(v.value); setShowVersionDropdown(false); setVersionFilter('') }}
                    className={`w-full text-left px-2 py-1.5 rounded text-xs transition-colors ${
                      v.value === String(value)
                        ? 'bg-primary-50 dark:bg-primary-900/30 text-primary-600 dark:text-primary-400'
                        : 'text-surface-700 dark:text-surface-300 hover:bg-surface-100 dark:hover:bg-surface-700'
                    }`}
                  >
                    <span className="font-mono">{v.displayName || v.versionId}</span>
                    <span className="ml-2 text-surface-400 dark:text-surface-500">({v.value})</span>
                  </button>
                ))}
                {filteredVersions.length === 0 && (
                  <div className="text-xs text-surface-400 dark:text-surface-500 text-center py-2">
                    无匹配版本
                  </div>
                )}
              </div>
            </div>
          )}
        </div>
      )}
    </div>
  )
}

function RepeatableControl({
  field,
  value = [],
  onChange,
  allValues,
  onAllValuesChange,
}: {
  field: UIDLNode
  value?: Record<string, unknown>[]
  onChange: (value: unknown) => void
  allValues: FormValues
  onAllValuesChange: (values: FormValues) => void
}) {
  const items = Array.isArray(value) ? value : []

  const addItem = () => {
    const newItem: Record<string, unknown> = {}
    field.children?.forEach((child) => {
      newItem[child.key] = child.default_value ?? ''
    })
    onChange([...items, newItem])
  }

  const removeItem = (index: number) => {
    const newItems = items.filter((_, i) => i !== index)
    onChange(newItems)
  }

  const updateItem = (index: number, key: string, val: unknown) => {
    const newItems = items.map((item, i) => {
      if (i === index) {
        return { ...item, [key]: val }
      }
      return item
    })
    onChange(newItems)
  }

  return (
    <div className="space-y-3">
      <div className="flex items-center justify-between">
        <LabelWithDescription field={field} />
        <button type="button" onClick={addItem} className="btn-ghost text-xs">
          + 添加
        </button>
      </div>
      {items.length === 0 && (
        <p className="text-xs text-surface-400 dark:text-surface-500">暂无数据，点击「添加」新增一项</p>
      )}
      {items.map((item, index) => (
        <div key={index} className="border border-surface-200 dark:border-surface-700 rounded p-4 space-y-3">
          <div className="flex items-center justify-between">
            <span className="text-xs font-medium text-surface-500 dark:text-surface-400">
              项 #{index + 1}
            </span>
            <button type="button" onClick={() => removeItem(index)} className="text-red-500 hover:text-red-600 text-xs">
              删除
            </button>
          </div>
          {field.children?.map((child) => (
            <SchemaField
              key={child.key}
              field={child}
              value={item[child.key]}
              onChange={(val) => updateItem(index, child.key, val)}
              values={allValues}
              onValuesChange={onAllValuesChange}
            />
          ))}
        </div>
      ))}
    </div>
  )
}

function LabelWithDescription({ field }: { field: UIDLNode }) {
  return (
    <div>
      <label className="block text-sm font-medium text-surface-700 dark:text-surface-300">
        {field.label}
        {field.required && <span className="text-red-500 ml-1">*</span>}
      </label>
      {field.description && (
        <p className="text-xs text-surface-400 dark:text-surface-500 mt-0.5">{field.description}</p>
      )}
    </div>
  )
}

function getNestedValue(obj: Record<string, unknown>, key: string): unknown {
  const keys = key.split('.')
  let current: unknown = obj
  for (const k of keys) {
    if (current === null || current === undefined || typeof current !== 'object') {
      return undefined
    }
    current = (current as Record<string, unknown>)[k]
  }
  return current
}

export { SchemaField, getNestedValue, LabelWithDescription }
export default SchemaForm
