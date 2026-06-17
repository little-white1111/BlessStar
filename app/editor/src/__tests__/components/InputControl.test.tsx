import { render, screen, fireEvent } from '@testing-library/react'
import { describe, it, expect, vi } from 'vitest'
import InputControl from '../../components/forms/InputControl'
import type { UIDLNode } from '../../types/uidl'

function makeField(overrides: Partial<UIDLNode> = {}): UIDLNode {
  return {
    widget: 'input',
    label: '名称',
    key: 'name',
    required: false,
    placeholder: '输入名称',
    default_value: '',
    order: 1,
    ...overrides,
  }
}

describe('InputControl', () => {
  it('renders label', () => {
    render(<InputControl field={makeField()} onChange={() => {}} />)
    expect(screen.getByText('名称')).toBeInTheDocument()
  })

  it('renders required asterisk', () => {
    render(<InputControl field={makeField({ required: true })} onChange={() => {}} />)
    expect(screen.getByText('*')).toBeInTheDocument()
  })

  it('shows description', () => {
    render(
      <InputControl field={makeField({ description: '请输入名称' })} onChange={() => {}} />,
    )
    expect(screen.getByText('请输入名称')).toBeInTheDocument()
  })

  it('displays passed value', () => {
    render(<InputControl field={makeField()} value="hello" onChange={() => {}} />)
    const input = screen.getByRole('textbox') as HTMLInputElement
    expect(input.value).toBe('hello')
  })

  it('falls back to default_value', () => {
    render(
      <InputControl field={makeField({ default_value: 'default' })} onChange={() => {}} />,
    )
    const input = screen.getByRole('textbox') as HTMLInputElement
    expect(input.value).toBe('default')
  })

  it('calls onChange on input', () => {
    const onChange = vi.fn()
    render(<InputControl field={makeField()} value="" onChange={onChange} />)
    fireEvent.change(screen.getByRole('textbox'), { target: { value: 'new' } })
    expect(onChange).toHaveBeenCalledWith('new')
  })

  it('sets maxLength from validation', () => {
    render(
      <InputControl
        field={makeField({ validation: { max_length: 10 } })}
        onChange={() => {}}
      />,
    )
    const input = screen.getByRole('textbox') as HTMLInputElement
    expect(input.maxLength).toBe(10)
  })

  it('uses password type when key contains password', () => {
    render(
      <InputControl field={makeField({ key: 'user_password' })} onChange={() => {}} />,
    )
    const input = screen.getByDisplayValue('') as HTMLInputElement
    expect(input.type).toBe('password')
  })

  it('handles disabled state via placeholder only (no disabled attr)', () => {
    // InputControl doesn't have a disabled prop — just verify it renders
    render(<InputControl field={makeField({ placeholder: '不可用' })} onChange={() => {}} />)
    expect(screen.getByPlaceholderText('不可用')).toBeInTheDocument()
  })
})
