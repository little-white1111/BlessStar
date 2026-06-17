import type { UIDLNode } from '../../types/uidl'

interface TextareaControlProps {
  field: UIDLNode
  value?: string
  onChange: (value: unknown) => void
}

function TextareaControl({ field, value, onChange }: TextareaControlProps) {
  const textValue = value !== undefined ? String(value) : (field.default_value as string) ?? ''

  return (
    <div className="space-y-1.5">
      <label className="block text-sm font-medium text-surface-700 dark:text-surface-300">
        {field.label}
        {field.required && <span className="text-red-500 ml-1">*</span>}
      </label>
      {field.description && (
        <p className="text-xs text-surface-400 dark:text-surface-500">{field.description}</p>
      )}
      <textarea
        value={textValue}
        onChange={(e) => onChange(e.target.value)}
        placeholder={field.placeholder}
        maxLength={field.validation?.max_length}
        rows={4}
        className="input-field resize-y min-h-[80px]"
      />
      {field.validation?.max_length && (
        <p className="text-xs text-surface-400 dark:text-surface-500 text-right">
          {String(textValue).length} / {field.validation.max_length}
        </p>
      )}
    </div>
  )
}

export default TextareaControl
