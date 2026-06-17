import { render, screen, fireEvent } from '@testing-library/react'
import { describe, it, expect, vi } from 'vitest'
import GroupControl from '../../components/forms/GroupControl'
import type { UIDLNode, FormValues } from '../../types/uidl'

const groupField: UIDLNode = {
  widget: 'group',
  label: '连接池设置',
  key: 'pool',
  order: 1,
  children: [
    {
      widget: 'number',
      label: '最小连接数',
      key: 'min_size',
      default_value: 2,
      order: 1,
    },
    {
      widget: 'number',
      label: '最大连接数',
      key: 'max_size',
      default_value: 10,
      validation: { min: 1, max: 200 },
      order: 2,
    },
  ],
}

describe('GroupControl', () => {
  it('renders group label', () => {
    render(
      <GroupControl
        field={groupField}
        onChange={() => {}}
        allValues={{}}
        onAllValuesChange={() => {}}
      />,
    )
    expect(screen.getByText('连接池设置')).toBeInTheDocument()
  })

  it('renders child field labels', () => {
    render(
      <GroupControl
        field={groupField}
        onChange={() => {}}
        allValues={{}}
        onAllValuesChange={() => {}}
      />,
    )
    expect(screen.getByText('最小连接数')).toBeInTheDocument()
    expect(screen.getByText('最大连接数')).toBeInTheDocument()
  })

  it('shows description', () => {
    render(
      <GroupControl
        field={{ ...groupField, description: '连接池参数配置' }}
        onChange={() => {}}
        allValues={{}}
        onAllValuesChange={() => {}}
      />,
    )
    expect(screen.getByText('连接池参数配置')).toBeInTheDocument()
  })

  it('calls onChange when child changes', () => {
    const onChange = vi.fn()
    const onAllValuesChange = vi.fn()
    render(
      <GroupControl
        field={groupField}
        value={{ min_size: 2, max_size: 10 }}
        onChange={onChange}
        allValues={{ pool: { min_size: 2 } }}
        onAllValuesChange={onAllValuesChange}
      />,
    )
    // Change min_size
    const spinbuttons = screen.getAllByRole('spinbutton')
    fireEvent.change(spinbuttons[0], { target: { value: '5' } })
    expect(onChange).toHaveBeenCalled()
    expect(onAllValuesChange).toHaveBeenCalled()
  })

  it('renders with empty value', () => {
    render(
      <GroupControl
        field={groupField}
        onChange={() => {}}
        allValues={{}}
        onAllValuesChange={() => {}}
      />,
    )
    expect(screen.getByText('连接池设置')).toBeInTheDocument()
  })
})
