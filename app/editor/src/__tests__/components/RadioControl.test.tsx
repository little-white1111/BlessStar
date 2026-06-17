import { render, screen, fireEvent } from '@testing-library/react'
import { describe, it, expect, vi } from 'vitest'
import RadioControl from '../../components/forms/RadioControl'
import type { UIDLNode } from '../../types/uidl'

const options = [
  { label: '选项 A', value: 'a' },
  { label: '选项 B', value: 'b' },
]

function makeField(overrides: Partial<UIDLNode> = {}): UIDLNode {
  return {
    widget: 'radio',
    label: '模式',
    key: 'mode',
    options,
    order: 1,
    ...overrides,
  }
}

describe('RadioControl', () => {
  it('renders label', () => {
    render(<RadioControl field={makeField()} onChange={() => {}} />)
    expect(screen.getByText('模式')).toBeInTheDocument()
  })

  it('renders required asterisk', () => {
    render(<RadioControl field={makeField({ required: true })} onChange={() => {}} />)
    expect(screen.getByText('*')).toBeInTheDocument()
  })

  it('renders all options', () => {
    render(<RadioControl field={makeField()} onChange={() => {}} />)
    expect(screen.getByLabelText('选项 A')).toBeInTheDocument()
    expect(screen.getByLabelText('选项 B')).toBeInTheDocument()
  })

  it('selects default value', () => {
    render(
      <RadioControl field={makeField({ default_value: 'b' })} onChange={() => {}} />,
    )
    expect((screen.getByLabelText('选项 B') as HTMLInputElement).checked).toBe(true)
    expect((screen.getByLabelText('选项 A') as HTMLInputElement).checked).toBe(false)
  })

  it('calls onChange when selecting an option', () => {
    const onChange = vi.fn()
    render(<RadioControl field={makeField({ default_value: 'a' })} onChange={onChange} />)
    fireEvent.click(screen.getByLabelText('选项 B'))
    expect(onChange).toHaveBeenCalledWith('b')
  })

  it('shows description', () => {
    render(
      <RadioControl field={makeField({ description: '选择运行模式' })} onChange={() => {}} />,
    )
    expect(screen.getByText('选择运行模式')).toBeInTheDocument()
  })
})
