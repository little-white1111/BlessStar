import type { UIDLNode } from '../../types/uidl'

interface SelectControlProps {
  field: UIDLNode
  value?: string
  onChange: (value: unknown) => void
}

function SelectControl({ field, value, onChange }: SelectControlProps) {
  const selectValue = value !== undefined ? String(value) : (field.default_value as string) ?? ''

  return (
    <div className="space-y-1.5">
      <label className="block text-sm font-medium text-surface-700 dark:text-surface-300">
        {field.label}
        {field.required && <span className="text-red-500 ml-1">*</span>}
      </label>
      {field.description && (
        <p className="text-xs text-surface-400 dark:text-surface-500">{field.description}</p>
      )}
      <select
        value={selectValue}
        onChange={(e) => onChange(e.target.value)}
        className="input-field"
      >
        {field.options?.map((opt) => (
          <option key={opt.value} value={opt.value}>
            {opt.label}
          </option>
        ))}
      </select>
    </div>
  )
}

export default SelectControl
