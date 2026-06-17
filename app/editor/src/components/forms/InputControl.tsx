import type { UIDLNode } from '../../types/uidl'

interface InputControlProps {
  field: UIDLNode
  value?: string
  onChange: (value: unknown) => void
}

function InputControl({ field, value, onChange }: InputControlProps) {
  const isPassword = field.key.toLowerCase().includes('password')
  const inputValue = value !== undefined ? String(value) : (field.default_value as string) ?? ''

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
        type={isPassword ? 'password' : 'text'}
        value={inputValue}
        onChange={(e) => onChange(e.target.value)}
        placeholder={field.placeholder}
        maxLength={field.validation?.max_length}
        className="input-field"
      />
    </div>
  )
}

export default InputControl
