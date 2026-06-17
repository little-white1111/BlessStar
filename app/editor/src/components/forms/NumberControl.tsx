import type { UIDLNode } from '../../types/uidl'

interface NumberControlProps {
  field: UIDLNode
  value?: number
  onChange: (value: unknown) => void
}

function NumberControl({ field, value, onChange }: NumberControlProps) {
  const numValue = value !== undefined ? Number(value) : (field.default_value as number) ?? 0

  return (
    <div className="space-y-1.5">
      <label className="block text-sm font-medium text-surface-700 dark:text-surface-300">
        {field.label}
        {field.required && <span className="text-red-500 ml-1">*</span>}
      </label>
      {field.description && (
        <p className="text-xs text-surface-400 dark:text-surface-500">{field.description}</p>
      )}
      <input
        type="number"
        value={numValue}
        onChange={(e) => {
          const val = e.target.value === '' ? '' : Number(e.target.value)
          onChange(val)
        }}
        placeholder={field.placeholder}
        min={field.validation?.min}
        max={field.validation?.max}
        className="input-field"
      />
      {(field.validation?.min !== undefined || field.validation?.max !== undefined) && (
        <p className="text-xs text-surface-400 dark:text-surface-500">
          范围: {field.validation?.min ?? '不限'} ~ {field.validation?.max ?? '不限'}
        </p>
      )}
    </div>
  )
}

export default NumberControl
