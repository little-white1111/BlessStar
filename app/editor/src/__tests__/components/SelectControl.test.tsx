import { render, screen, fireEvent } from '@testing-library/react'
import { describe, it, expect, vi } from 'vitest'
import SelectControl from '../../components/forms/SelectControl'
import type { UIDLNode } from '../../types/uidl'

function makeField(overrides: Partial<UIDLNode> = {}): UIDLNode {
  return {
    widget: 'select',
    label: '字符集',
    key: 'charset',
    default_value: 'utf8',
    options: [
      { label: 'UTF-8', value: 'utf8' },
      { label: 'UTF-8 MB4', value: 'utf8mb4' },
    ],
    order: 1,
    ...overrides,
  }
}

describe('SelectControl', () => {
  it('renders label', () => {
    render(<SelectControl field={makeField()} onChange={() => {}} />)
    expect(screen.getByText('字符集')).toBeInTheDocument()
  })

  it('renders required asterisk', () => {
    render(<SelectControl field={makeField({ required: true })} onChange={() => {}} />)
    expect(screen.getByText('*')).toBeInTheDocument()
  })

  it('shows description', () => {
    render(
      <SelectControl field={makeField({ description: '选择字符集' })} onChange={() => {}} />,
    )
    expect(screen.getByText('选择字符集')).toBeInTheDocument()
  })

  it('renders all options', () => {
    render(<SelectControl field={makeField()} onChange={() => {}} />)
    expect(screen.getByText('UTF-8')).toBeInTheDocument()
    expect(screen.getByText('UTF-8 MB4')).toBeInTheDocument()
  })

  it('selects default value', () => {
    render(<SelectControl field={makeField()} onChange={() => {}} />)
    const select = screen.getByRole('combobox') as HTMLSelectElement
    expect(select.value).toBe('utf8')
  })

  it('calls onChange on selection', () => {
    const onChange = vi.fn()
    render(<SelectControl field={makeField()} onChange={onChange} />)
    fireEvent.change(screen.getByRole('combobox'), { target: { value: 'utf8mb4' } })
    expect(onChange).toHaveBeenCalledWith('utf8mb4')
  })
})
