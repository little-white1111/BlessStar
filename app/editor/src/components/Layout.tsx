import { useState, useEffect, useCallback, type ReactNode } from 'react'
import { useNavigate, useLocation } from 'react-router-dom'
import Sidebar from './Sidebar'
import PropertyPanel from './PropertyPanel'
import AIPanel, { type PanelPosition } from '../ai/AIPanel'
import { ErrorBoundary } from './ErrorBoundary'

interface LayoutProps {
  children: ReactNode
}

function Layout({ children }: LayoutProps) {
  const [darkMode, setDarkMode] = useState(() => {
    if (typeof window !== 'undefined') {
      return localStorage.getItem('theme') === 'dark'
    }
    return false
  })
  const [aiPanelOpen, setAiPanelOpen] = useState(false)
  const [aiPanelPosition, setAiPanelPosition] = useState<PanelPosition>('right')
  const [aiPanelSize, setAiPanelSize] = useState<number>(380)
  const navigate = useNavigate()
  const location = useLocation()

  useEffect(() => {
    const root = document.documentElement
    if (darkMode) {
      root.classList.add('dark')
      localStorage.setItem('theme', 'dark')
    } else {
      root.classList.remove('dark')
      localStorage.setItem('theme', 'light')
    }
  }, [darkMode])

  useEffect(() => {
    // Register AI toggle hotkey: Ctrl+Shift+A
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.ctrlKey && e.shiftKey && e.key === 'A') {
        e.preventDefault()
        setAiPanelOpen((prev) => !prev)
      }
    }
    window.addEventListener('keydown', handleKeyDown)
    return () => window.removeEventListener('keydown', handleKeyDown)
  }, [])

  const toggleTheme = () => setDarkMode((prev) => !prev)

  const handleAcceptSuggestion = useCallback((suggestion: string) => {
    // In MVP, log the accepted suggestion; in production, dispatch to editor store
    console.log('AI suggestion accepted:', suggestion)
  }, [])

  // Only show right-side panels on editor route
  const showPropertyPanel = location.pathname === '/editor' && !aiPanelOpen

  return (
    <div className="flex h-screen overflow-hidden">
      <Sidebar
        currentPath={location.pathname}
        onNavigate={(path) => navigate(path)}
        darkMode={darkMode}
        onToggleTheme={toggleTheme}
        aiPanelOpen={aiPanelOpen}
        onToggleAiPanel={() => setAiPanelOpen((prev) => !prev)}
      />

      {aiPanelOpen ? (
        aiPanelPosition === 'bottom' ? (
          // 底部模式：main 占顶部，AI 面板可上下拉伸
          <div className="flex-1 flex flex-col overflow-hidden">
            <main
              className="overflow-y-auto p-6"
              style={{ flex: `0 0 calc(100% - ${Math.max(200, aiPanelSize)}px)`, minHeight: 100 }}
            >
              {children}
            </main>
            <ErrorBoundary panelName="AI 助手">
            <AIPanel
              isOpen={aiPanelOpen}
              position={aiPanelPosition}
              panelSize={aiPanelSize}
              onResize={setAiPanelSize}
              onChangePosition={setAiPanelPosition}
              onClose={() => setAiPanelOpen(false)}
              onAcceptSuggestion={handleAcceptSuggestion}
            />
            </ErrorBoundary>
          </div>
        ) : (
          // 右侧模式：main 占左侧，AI 面板可左右拉伸
          <div className="flex-1 flex flex-row overflow-hidden">
            <main className="flex-1 overflow-y-auto p-6">
              {children}
            </main>
            <ErrorBoundary panelName="AI 助手">
            <AIPanel
              isOpen={aiPanelOpen}
              position={aiPanelPosition}
              panelSize={aiPanelSize}
              onResize={setAiPanelSize}
              onChangePosition={setAiPanelPosition}
              onClose={() => setAiPanelOpen(false)}
              onAcceptSuggestion={handleAcceptSuggestion}
            />
            </ErrorBoundary>
          </div>
        )
      ) : (
        <>
          <main className="flex-1 overflow-y-auto p-6">
            {children}
          </main>
          {showPropertyPanel && (
            <PropertyPanel />
          )}
        </>
      )}
    </div>
  )
}

export default Layout
