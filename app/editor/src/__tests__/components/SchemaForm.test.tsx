import { render, screen, fireEvent } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { describe, it, expect, vi } from 'vitest'
import SchemaForm from '../../components/forms/SchemaForm'
import { MINIMAL_UIDL, FULL_UIDL, NESTED_GROUP_UIDL, FULL_FORM_VALUES } from '../support/test-data'

describe('SchemaForm', () => {
  it('renders all fields from UIDLDocument', () => {
    render(
      <SchemaForm fields={FULL_UIDL.fields} values={{}} onChange={() => {}} />,
    )
    expect(screen.getByText('主机地址')).toBeInTheDocument()
    expect(screen.getByText('端口号')).toBeInTheDocument()
    expect(screen.getByText('数据库名')).toBeInTheDocument()
    expect(screen.getByText('用户名')).toBeInTheDocument()
  })

  it('renders nested group 3 levels deep', () => {
    render(
      <SchemaForm fields={NESTED_GROUP_UIDL.fields} values={{}} onChange={() => {}} />,
    )
    expect(screen.getByText('Level 1')).toBeInTheDocument()
    expect(screen.getByText('Level 2')).toBeInTheDocument()
    expect(screen.getByText('Level 3')).toBeInTheDocument()
    expect(screen.getByText('L3 Field')).toBeInTheDocument()
  })

  it('executes onChange when editing a text field', async () => {
    const onChange = vi.fn()
    render(
      <SchemaForm fields={MINIMAL_UIDL.fields} values={{ name: '' }} onChange={onChange} />,
    )
    const input = screen.getByRole('textbox')
    await userEvent.type(input, 'a')
    // Each keystroke fires onChange
    expect(onChange).toHaveBeenCalled()
  })

  it('renders checkbox and toggles it', () => {
    const onChange = vi.fn()
    render(
      <SchemaForm
        fields={FULL_UIDL.fields}
        values={{ ssl_enabled: false }}
        onChange={onChange}
      />,
    )
    const checkbox = screen.getByRole('checkbox')
    expect(checkbox).not.toBeChecked()
    fireEvent.click(checkbox)
    expect(onChange).toHaveBeenCalled()
  })

  it('renders select and changes it', () => {
    const onChange = vi.fn()
    render(
      <SchemaForm
        fields={FULL_UIDL.fields}
        values={FULL_FORM_VALUES}
        onChange={onChange}
      />,
    )
    const select = screen.getByRole('combobox')
    fireEvent.change(select, { target: { value: 'gbk' } })
    expect(onChange).toHaveBeenCalled()
  })

  it('renders radio options', () => {
    // Add a radio field to the minimal UIDL
    const fields = [
      ...MINIMAL_UIDL.fields,
      {
        widget: 'radio' as const,
        label: '模式',
        key: 'mode',
        options: [
          { label: '自动', value: 'auto' },
          { label: '手动', value: 'manual' },
        ],
        order: 2,
      },
    ]
    render(<SchemaForm fields={fields} values={{}} onChange={() => {}} />)
    expect(screen.getByLabelText('自动')).toBeInTheDocument()
    expect(screen.getByLabelText('手动')).toBeInTheDocument()
  })

  it('renders repeatable section and adds item', () => {
    const onChange = vi.fn()
    render(
      <SchemaForm
        fields={FULL_UIDL.fields}
        values={{ extra_params: [] }}
        onChange={onChange}
      />,
    )
    expect(screen.getByText('额外参数')).toBeInTheDocument()
    const addBtn = screen.getByText('+ 添加')
    expect(addBtn).toBeInTheDocument()
    fireEvent.click(addBtn)
    expect(onChange).toHaveBeenCalled()
  })

  it('shows error for unknown widget type', () => {
    const fields = [
      {
        widget: 'unknown_widget' as any,
        label: 'Bad',
        key: 'bad',
        order: 1,
      },
    ]
    render(<SchemaForm fields={fields} values={{}} onChange={() => {}} />)
    expect(screen.getByText(/未知控件类型/)).toBeInTheDocument()
  })

  it('renders fields sorted by order', () => {
    const fields = [
      { widget: 'input' as const, label: 'B', key: 'b', order: 2 },
      { widget: 'input' as const, label: 'A', key: 'a', order: 1 },
    ]
    render(<SchemaForm fields={fields} values={{}} onChange={() => {}} />)
    const labels = screen.getAllByText(/^[AB]$/)
    // A should come first (order 1), then B (order 2)
    expect(labels[0]).toHaveTextContent('A')
    expect(labels[1]).toHaveTextContent('B')
  })
})
