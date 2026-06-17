import { render, screen, fireEvent } from '@testing-library/react'
import { describe, it, expect, vi } from 'vitest'
import NumberControl from '../../components/forms/NumberControl'
import type { UIDLNode } from '../../types/uidl'

function makeField(overrides: Partial<UIDLNode> = {}): UIDLNode {
  return {
    widget: 'number',
    label: '端口号',
    key: 'port',
    default_value: 3306,
    validation: { min: 1, max: 65535 },
    order: 1,
    ...overrides,
  }
}

describe('NumberControl', () => {
  it('renders label', () => {
    render(<NumberControl field={makeField()} onChange={() => {}} />)
    expect(screen.getByText('端口号')).toBeInTheDocument()
  })

  it('renders required asterisk', () => {
    render(<NumberControl field={makeField({ required: true })} onChange={() => {}} />)
    expect(screen.getByText('*')).toBeInTheDocument()
  })

  it('shows description', () => {
    render(
      <NumberControl field={makeField({ description: '数据库端口' })} onChange={() => {}} />,
    )
    expect(screen.getByText('数据库端口')).toBeInTheDocument()
  })

  it('displays default value', () => {
    render(<NumberControl field={makeField()} onChange={() => {}} />)
    const input = screen.getByRole('spinbutton') as HTMLInputElement
    expect(input.value).toBe('3306')
  })

  it('displays passed value', () => {
    render(<NumberControl field={makeField()} value={5432} onChange={() => {}} />)
    const input = screen.getByRole('spinbutton') as HTMLInputElement
    expect(input.value).toBe('5432')
  })

  it('calls onChange with number', () => {
    const onChange = vi.fn()
    render(<NumberControl field={makeField()} value={3306} onChange={onChange} />)
    fireEvent.change(screen.getByRole('spinbutton'), { target: { value: '5432' } })
    expect(onChange).toHaveBeenCalledWith(5432)
  })

  it('shows range text', () => {
    render(<NumberControl field={makeField()} onChange={() => {}} />)
    expect(screen.getByText(/范围/)).toBeInTheDocument()
    expect(screen.getByText(/1/)).toBeInTheDocument()
    expect(screen.getByText(/65535/)).toBeInTheDocument()
  })

  it('sets min/max attributes', () => {
    render(<NumberControl field={makeField()} onChange={() => {}} />)
    const input = screen.getByRole('spinbutton') as HTMLInputElement
    expect(input.min).toBe('1')
    expect(input.max).toBe('65535')
  })

  it('sets placeholder', () => {
    render(
      <NumberControl
        field={makeField({ placeholder: '输入端口' })}
        onChange={() => {}}
      />,
    )
    expect(screen.getByPlaceholderText('输入端口')).toBeInTheDocument()
  })
})
