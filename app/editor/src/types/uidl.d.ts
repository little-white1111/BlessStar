export interface UIDLValidation {
  min?: number
  max?: number
  pattern?: string
  max_length?: number
}

export interface UIDLSelectOption {
  label: string
  value: string
}

export interface UIDLNode {
  widget: 'input' | 'select' | 'checkbox' | 'radio' | 'number' | 'textarea' | 'group' | 'repeatable'
  label: string
  key: string
  required?: boolean
  placeholder?: string
  description?: string
  default_value?: unknown
  options?: UIDLSelectOption[]
  children?: UIDLNode[]
  order?: number
  visibility?: string | null
  ai_layout_hint?: string
  validation?: UIDLValidation
}

export interface UIDLDocument {
  render_type: 'dynamic_form'
  version: string
  title: string
  description?: string
  fields: UIDLNode[]
}

export type FormValues = Record<string, unknown>
