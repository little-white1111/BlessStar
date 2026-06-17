import type { UIDLNode } from '../../types/uidl'

interface RadioControlProps {
  field: UIDLNode
  value?: string
  onChange: (value: unknown) => void
}

function RadioControl({ field, value, onChange }: RadioControlProps) {
  const selectedValue = value !== undefined ? String(value) : (field.default_value as string) ?? ''

  return (
    <div className="space-y-1.5">
      <label className="block text-sm font-medium text-surface-700 dark:text-surface-300">
        {field.label}
        {field.required && <span className="text-red-500 ml-1">*</span>}
      </label>
      {field.description && (
        <p className="text-xs text-surface-400 dark:text-surface-500">{field.description}</p>
      )}
      <div className="space-y-2 mt-2">
        {field.options?.map((opt) => (
          <label
            key={opt.value}
            className="flex items-center gap-3 cursor-pointer group"
          >
            <input
              type="radio"
              name={field.key}
              value={opt.value}
              checked={selectedValue === opt.value}
              onChange={(e) => onChange(e.target.value)}
              className="w-4 h-4 text-primary-600 focus:ring-primary-500
                         border-surface-300 dark:border-surface-600
                         bg-white dark:bg-surface-800"
            />
            <span className="text-sm text-surface-700 dark:text-surface-300 group-hover:text-surface-900 dark:group-hover:text-surface-50 transition-colors">
              {opt.label}
            </span>
          </label>
        ))}
      </div>
    </div>
  )
}

export default RadioControl
