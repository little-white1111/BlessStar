interface SidebarProps {
  currentPath: string
  onNavigate: (path: string) => void
  darkMode: boolean
  onToggleTheme: () => void
  aiPanelOpen: boolean
  onToggleAiPanel: () => void
}

interface NavItem {
  label: string
  path: string
  icon: string
}

const navItems: NavItem[] = [
  { label: '仪表盘', path: '/', icon: '📊' },
  { label: '配置编辑器', path: '/editor', icon: '⚙️' },
  { label: '设置', path: '/settings', icon: '🔧' },
]

function Sidebar({ currentPath, onNavigate, darkMode, onToggleTheme, aiPanelOpen, onToggleAiPanel }: SidebarProps) {
  return (
    <aside className="w-60 bg-white dark:bg-surface-800 border-r border-surface-200 dark:border-surface-700 flex flex-col">
      {/* Logo */}
      <div className="h-14 flex items-center gap-3 px-4 border-b border-surface-200 dark:border-surface-700">
        <div className="w-8 h-8 rounded-lg bg-primary-600 flex items-center justify-center text-white font-bold text-sm">
          BS
        </div>
        <span className="font-semibold text-sm text-surface-900 dark:text-surface-50">
          BlessStar
        </span>
      </div>

      {/* Navigation */}
      <nav className="flex-1 p-3 space-y-1">
        {navItems.map((item) => (
          <button
            key={item.path}
            onClick={() => onNavigate(item.path)}
            className={`sidebar-item w-full text-left ${
              currentPath === item.path ? 'active' : ''
            }`}
          >
            <span className="text-lg">{item.icon}</span>
            <span>{item.label}</span>
          </button>
        ))}
        <div className="pt-2">
          <button
            onClick={onToggleAiPanel}
            className={`sidebar-item w-full text-left ${aiPanelOpen ? 'active' : ''}`}
            title="AI 助手 (Ctrl+Shift+A)"
          >
            <span className="text-lg">🤖</span>
            <span>AI 助手</span>
            {aiPanelOpen && (
              <span className="ml-auto text-[10px] bg-primary-500 text-white px-1.5 py-0.5 rounded">ON</span>
            )}
          </button>
        </div>
      </nav>

      {/* Footer */}
      <div className="p-3 border-t border-surface-200 dark:border-surface-700 space-y-1">
        <button
          onClick={onToggleTheme}
          className="sidebar-item w-full text-left"
        >
          <span className="text-lg">{darkMode ? '☀️' : '🌙'}</span>
          <span>{darkMode ? '浅色模式' : '深色模式'}</span>
        </button>
        <div className="px-3 py-2 text-xs text-surface-400 dark:text-surface-500 flex items-center justify-between">
          <span>v1.0.0</span>
          <span className="text-[10px] bg-surface-100 dark:bg-surface-700 px-1.5 py-0.5 rounded">Ctrl+Shift+A</span>
        </div>
      </div>
    </aside>
  )
}

export default Sidebar
