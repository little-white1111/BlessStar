import { render, screen, waitFor, fireEvent } from '@testing-library/react'
import { describe, it, expect, vi, beforeEach } from 'vitest'
import { MemoryRouter } from 'react-router-dom'
import Editor from '../../routes/Editor'

const mockLoadConfig = vi.fn()
const mockSaveConfig = vi.fn()
const mockSaveToPath = vi.fn()
const mockSchemaToUidl = vi.fn()
const mockOnMenuOpen = vi.fn()
const mockOnMenuSave = vi.fn()
const mockOnMenuSaveAs = vi.fn()

beforeEach(() => {
  vi.clearAllMocks()
  window.blessstar = {
    loadConfig: mockLoadConfig,
    saveConfig: mockSaveConfig,
    saveToPath: mockSaveToPath,
    schemaToUidl: mockSchemaToUidl,
    onConfigChanged: vi.fn(),
    onMenuOpen: mockOnMenuOpen,
    onMenuSave: mockOnMenuSave,
    onMenuSaveAs: mockOnMenuSaveAs,
  }
})

/** Helper: render Editor inside MemoryRouter */
function renderEditor() {
  return render(
    <MemoryRouter initialEntries={['/editor']}>
      <Editor />
    </MemoryRouter>,
  )
}

describe('Editor integration', () => {
  it('loads demo UIDL on mount via schemaToUidl()', async () => {
    const mockUidl = JSON.stringify({
      render_type: 'dynamic_form',
      version: '1.0.0',
      title: 'Demo Config',
      fields: [
        { widget: 'input', label: 'Field', key: 'field', order: 1 },
      ],
    })
    mockSchemaToUidl.mockResolvedValue(mockUidl)

    renderEditor()

    await waitFor(() => {
      expect(screen.getByText('Demo Config')).toBeInTheDocument()
    })
    expect(mockSchemaToUidl).toHaveBeenCalled()
  })

  it('shows "加载演示模板" button when no UIDL', () => {
    mockSchemaToUidl.mockResolvedValue(null)
    renderEditor()
    expect(screen.getByText('加载演示模板')).toBeInTheDocument()
  })

  it('clicking demo template button loads demo', async () => {
    mockSchemaToUidl
      .mockResolvedValueOnce(null) // mount
      .mockResolvedValueOnce(
        JSON.stringify({
          render_type: 'dynamic_form',
          version: '1.0.0',
          title: 'Demo',
          fields: [],
        }),
      )

    renderEditor()
    const btn = screen.getByText('加载演示模板')
    fireEvent.click(btn)

    await waitFor(() => {
      expect(screen.getByText('Demo')).toBeInTheDocument()
    })
  })

  it('clicking "打开" calls loadConfig then schemaToUidl', async () => {
    // Mount: no UIDL
    mockSchemaToUidl.mockResolvedValueOnce(null)
    mockLoadConfig.mockResolvedValue(
      JSON.stringify({ path: '/config.json', content: '{}' }),
    )
    mockSchemaToUidl.mockResolvedValue(
      JSON.stringify({
        render_type: 'dynamic_form',
        version: '1.0.0',
        title: 'Opened Config',
        fields: [],
      }),
    )

    renderEditor()

    // Wait for mount async to settle (loading→false)
    await waitFor(() => {
      expect(
        screen.getByText((content) => content.includes('点击「打开」加载配置文件')),
      ).toBeInTheDocument()
    })

    fireEvent.click(screen.getByText('打开'))

    await waitFor(() => {
      expect(mockLoadConfig).toHaveBeenCalled()
      expect(mockSchemaToUidl).toHaveBeenCalledWith('{}')
      expect(screen.getByText('Opened Config')).toBeInTheDocument()
    })
  })

  it('clicking "保存" when currentFilePath is set calls saveToPath', async () => {
    // Mount: need UIDL loaded first
    mockSchemaToUidl.mockResolvedValue(
      JSON.stringify({
        render_type: 'dynamic_form',
        version: '1.0.0',
        title: 'Config',
        fields: [
          { widget: 'input', label: 'Name', key: 'name', order: 1 },
        ],
      }),
    )

    // Load config to set currentFilePath
    mockLoadConfig.mockResolvedValue(
      JSON.stringify({ path: '/config.json', content: '{}' }),
    )

    mockSaveToPath.mockResolvedValue(true)

    renderEditor()

    // Wait for mount
    await waitFor(() => {
      expect(screen.getByText('Config')).toBeInTheDocument()
    })

    // Click open to set currentFilePath
    fireEvent.click(screen.getByText('打开'))
    await waitFor(() => {
      expect(mockLoadConfig).toHaveBeenCalled()
    })

    // Now click save
    fireEvent.click(screen.getByText('保存'))
    await waitFor(() => {
      expect(mockSaveToPath).toHaveBeenCalled()
    })
  })

  it('undo/redo buttons exist and toggle state', async () => {
    mockSchemaToUidl.mockResolvedValue(
      JSON.stringify({
        render_type: 'dynamic_form',
        version: '1.0.0',
        title: 'Test',
        fields: [],
      }),
    )
    renderEditor()

    await waitFor(() => {
      expect(screen.getByText('Test')).toBeInTheDocument()
    })

    const undoBtn = screen.getByTitle('撤销')
    const redoBtn = screen.getByTitle('重做')

    // Initially undo disabled, redo disabled
    expect(undoBtn).toBeDisabled()
    expect(redoBtn).toBeDisabled()
  })

  it('shows status bar message after loading', async () => {
    mockSchemaToUidl.mockResolvedValue(
      JSON.stringify({
        render_type: 'dynamic_form',
        version: '1.0.0',
        title: 'Status Test',
        fields: [],
      }),
    )
    renderEditor()

    await waitFor(() => {
      expect(screen.getByText('已加载演示配置模板')).toBeInTheDocument()
    })
  })
})
