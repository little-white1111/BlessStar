import SchemaForm from './SchemaForm'
import type { UIDLNode, FormValues } from '../../types/uidl'

interface GroupControlProps {
  field: UIDLNode
  value?: Record<string, unknown>
  onChange: (value: unknown) => void
  allValues: FormValues
  onAllValuesChange: (values: FormValues) => void
}

function GroupControl({ field, value, onChange, allValues, onAllValuesChange }: GroupControlProps) {
  const groupValues = value ?? {}

  return (
    <div className="space-y-3">
      <div className="border-b border-surface-200 dark:border-surface-700 pb-1">
        <h3 className="text-sm font-semibold text-surface-800 dark:text-surface-200">
          {field.label}
        </h3>
        {field.description && (
          <p className="text-xs text-surface-400 dark:text-surface-500 mt-0.5">
            {field.description}
          </p>
        )}
      </div>
      <div className="pl-4 border-l-2 border-primary-200 dark:border-primary-800 space-y-4">
        <SchemaForm
          fields={field.children ?? []}
          values={groupValues}
          onChange={(newValues) => {
            onChange(newValues)
            onAllValuesChange({ ...allValues, [field.key]: newValues })
          }}
        />
      </div>
    </div>
  )
}

export default GroupControl
