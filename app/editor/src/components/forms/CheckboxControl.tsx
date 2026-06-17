import type { UIDLNode } from '../../types/uidl'

interface CheckboxControlProps {
  field: UIDLNode
  value?: boolean
  onChange: (value: unknown) => void
}

function CheckboxControl({ field, value, onChange }: CheckboxControlProps) {
  const checked = value !== undefined ? Boolean(value) : Boolean(field.default_value)

  return (
    <div className="space-y-1.5">
      <label className="flex items-start gap-3 cursor-pointer group">
        <input
          type="checkbox"
          checked={checked}
          onChange={(e) => onChange(e.target.checked)}
          className="mt-0.5 w-4 h-4 rounded border-surface-300 dark:border-surface-600
                     text-primary-600 focus:ring-primary-500 focus:ring-2
                     bg-white dark:bg-surface-800
                     cursor-pointer"
        />
        <div>
          <span className="text-sm font-medium text-surface-700 dark:text-surface-300 group-hover:text-surface-900 dark:group-hover:text-surface-50 transition-colors">
            {field.label}
            {field.required && <span className="text-red-500 ml-1">*</span>}
          </span>
          {field.description && (
            <p className="text-xs text-surface-400 dark:text-surface-500 mt-0.5">
              {field.description}
            </p>
          )}
        </div>
      </label>
    </div>
  )
}

export default CheckboxControl
