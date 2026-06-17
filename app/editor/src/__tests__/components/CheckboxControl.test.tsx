import { render, screen, fireEvent } from '@testing-library/react'
import { describe, it, expect, vi } from 'vitest'
import CheckboxControl from '../../components/forms/CheckboxControl'
import type { UIDLNode } from '../../types/uidl'

function makeField(overrides: Partial<UIDLNode> = {}): UIDLNode {
  return {
    widget: 'checkbox',
    label: '启用 SSL',
    key: 'ssl_enabled',
    default_value: false,
    order: 1,
    ...overrides,
  }
}

describe('CheckboxControl', () => {
  it('renders label', () => {
    render(<CheckboxControl field={makeField()} onChange={() => {}} />)
    expect(screen.getByText('启用 SSL')).toBeInTheDocument()
  })

  it('shows required asterisk', () => {
    render(<CheckboxControl field={makeField({ required: true })} onChange={() => {}} />)
    expect(screen.getByText('*')).toBeInTheDocument()
  })

  it('shows description', () => {
    render(
      <CheckboxControl
        field={makeField({ description: '启用后使用 SSL 连接' })}
        onChange={() => {}}
      />,
    )
    expect(screen.getByText('启用后使用 SSL 连接')).toBeInTheDocument()
  })

  it('is unchecked by default (false)', () => {
    render(<CheckboxControl field={makeField()} onChange={() => {}} />)
    const checkbox = screen.getByRole('checkbox') as HTMLInputElement
    expect(checkbox.checked).toBe(false)
  })

  it('is checked when value=true', () => {
    render(<CheckboxControl field={makeField()} value={true} onChange={() => {}} />)
    const checkbox = screen.getByRole('checkbox') as HTMLInputElement
    expect(checkbox.checked).toBe(true)
  })

  it('calls onChange with checked state', () => {
    const onChange = vi.fn()
    render(<CheckboxControl field={makeField()} value={false} onChange={onChange} />)
    fireEvent.click(screen.getByRole('checkbox'))
    expect(onChange).toHaveBeenCalledWith(true)
  })
})
