import '@testing-library/jest-dom'

// Mock window.blessstar API for tests
Object.defineProperty(window, 'blessstar', {
  value: {
    loadConfig: vi.fn(),
    saveConfig: vi.fn(),
    saveToPath: vi.fn(),
    schemaToUidl: vi.fn(),
    onConfigChanged: vi.fn(),
    onMenuOpen: vi.fn(),
    onMenuSave: vi.fn(),
    onMenuSaveAs: vi.fn(),
  },
  writable: true,
})
