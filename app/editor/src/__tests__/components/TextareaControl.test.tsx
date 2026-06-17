import { render, screen, fireEvent } from '@testing-library/react'
import { describe, it, expect, vi } from 'vitest'
import TextareaControl from '../../components/forms/TextareaControl'
import type { UIDLNode } from '../../types/uidl'

function makeField(overrides: Partial<UIDLNode> = {}): UIDLNode {
  return {
    widget: 'textarea',
    label: '备注',
    key: 'notes',
    placeholder: '输入备注',
    validation: { max_length: 500 },
    order: 1,
    ...overrides,
  }
}

describe('TextareaControl', () => {
  it('renders label', () => {
    render(<TextareaControl field={makeField()} onChange={() => {}} />)
    expect(screen.getByText('备注')).toBeInTheDocument()
  })

  it('renders required asterisk', () => {
    render(<TextareaControl field={makeField({ required: true })} onChange={() => {}} />)
    expect(screen.getByText('*')).toBeInTheDocument()
  })

  it('shows description', () => {
    render(
      <TextareaControl
        field={makeField({ description: '输入备注信息' })}
        onChange={() => {}}
      />,
    )
    expect(screen.getByText('输入备注信息')).toBeInTheDocument()
  })

  it('displays default value', () => {
    render(
      <TextareaControl field={makeField({ default_value: 'hello' })} onChange={() => {}} />,
    )
    const textarea = screen.getByRole('textbox') as HTMLTextAreaElement
    expect(textarea.value).toBe('hello')
  })

  it('displays passed value', () => {
    render(<TextareaControl field={makeField()} value="world" onChange={() => {}} />)
    const textarea = screen.getByRole('textbox') as HTMLTextAreaElement
    expect(textarea.value).toBe('world')
  })

  it('calls onChange', () => {
    const onChange = vi.fn()
    render(<TextareaControl field={makeField()} value="" onChange={onChange} />)
    fireEvent.change(screen.getByRole('textbox'), { target: { value: 'new text' } })
    expect(onChange).toHaveBeenCalledWith('new text')
  })

  it('shows character count', () => {
    render(
      <TextareaControl field={makeField()} value="hi" onChange={() => {}} />,
    )
    expect(screen.getByText(/2 \/ 500/)).toBeInTheDocument()
  })

  it('sets maxLength', () => {
    render(<TextareaControl field={makeField()} onChange={() => {}} />)
    const textarea = screen.getByRole('textbox') as HTMLTextAreaElement
    expect(textarea.maxLength).toBe(500)
  })
})
