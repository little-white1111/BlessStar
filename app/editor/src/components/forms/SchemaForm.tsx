import { useCallback } from 'react'
import type { UIDLNode, FormValues } from '../../types/uidl'
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
}

function SchemaForm({ fields, values, onChange, parentKey = '' }: SchemaFormProps) {
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
}

function SchemaField({ field, value, onChange, values, onValuesChange, parentKey }: SchemaFieldProps) {
  switch (field.widget) {
    case 'input':
      return <InputControl field={field} value={value as string} onChange={onChange} />
    case 'select':
      return <SelectControl field={field} value={value as string} onChange={onChange} />
    case 'checkbox':
      return <CheckboxControl field={field} value={value as boolean} onChange={onChange} />
    case 'radio':
      return <RadioControl field={field} value={value as string} onChange={onChange} />
    case 'number':
      return <NumberControl field={field} value={value as number} onChange={onChange} />
    case 'textarea':
      return <TextareaControl field={field} value={value as string} onChange={onChange} />
    case 'group':
      return (
        <GroupControl
          field={field}
          value={value as Record<string, unknown>}
          onChange={onChange}
          allValues={values}
          onAllValuesChange={onValuesChange}
        />
      )
    case 'repeatable':
      return (
        <RepeatableControl
          field={field}
          value={value as Record<string, unknown>[]}
          onChange={onChange}
          allValues={values}
          onAllValuesChange={onValuesChange}
        />
      )
    default:
      return <div className="text-red-500 text-sm">未知控件类型: {field.widget}</div>
  }
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
