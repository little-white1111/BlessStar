/**
 * ProjectManager — 多项目管理 UI（第35天 · UX-06）
 *
 * 功能：项目列表（本地目录扫描 + 最近打开）/ 新建 / 打开 / 切换 / 删除 / 导出 / 导入
 *
 * 项目元数据文件：.bsproject/project.json
 * 项目目录结构：~/BlessStar/projects/{projectName}/.bsproject/
 */

import { useState, useEffect, useCallback } from 'react'

/** 项目元数据 */
export interface ProjectMeta {
  name: string
  createdAt: string
  lastOpened: string
  path: string
  template?: string
}

/** 项目目录扫描结果 */
async function scanProjects(): Promise<ProjectMeta[]> {
  try {
    // 如果 blessstar 提供了 listProjects，使用它
    if ((window.blessstar as unknown as Record<string, unknown>).listProjects) {
      const data = await (window.blessstar as unknown as { listProjects: () => Promise<string> }).listProjects()
      if (data) return JSON.parse(data) as ProjectMeta[]
    }
  } catch {
    // fallback: 从 localStorage 读取项目列表
  }

  // 降级：从 localStorage 读取
  try {
    const raw = localStorage.getItem('blessstar_project_list')
    if (raw) return JSON.parse(raw) as ProjectMeta[]
  } catch {
    // ignore
  }
  return []
}

function saveProjects(projects: ProjectMeta[]) {
  localStorage.setItem('blessstar_project_list', JSON.stringify(projects))
}

interface ProjectManagerProps {
  /** 当前打开的项目名（null = 无项目打开） */
  currentProject: string | null
  /** 打开项目回调 */
  onOpenProject: (project: ProjectMeta) => void
  /** 项目创建回调 */
  onNewProject: (name: string, template?: string) => Promise<ProjectMeta | null>
  /** 关闭项目回调 */
  onCloseProject: () => void
}

export default function ProjectManager({
  currentProject,
  onOpenProject,
  onNewProject,
  onCloseProject,
}: ProjectManagerProps) {
  const [projects, setProjects] = useState<ProjectMeta[]>([])
  const [isOpen, setIsOpen] = useState(false)
  const [isCreating, setIsCreating] = useState(false)
  const [newProjectName, setNewProjectName] = useState('')
  const [filter, setFilter] = useState('')

  const refreshProjects = useCallback(async () => {
    const list = await scanProjects()
    setProjects(list)
  }, [])

  useEffect(() => {
    refreshProjects()
  }, [refreshProjects])

  // 按最近打开时间排序，最近 10 个
  const recentProjects = [...projects]
    .sort((a, b) => new Date(b.lastOpened).getTime() - new Date(a.lastOpened).getTime())
    .slice(0, 10)
    .filter((p) => !filter || p.name.toLowerCase().includes(filter.toLowerCase()))

  const handleCreate = async () => {
    if (!newProjectName.trim()) return
    setIsCreating(false)
    const meta = await onNewProject(newProjectName.trim())
    if (meta) {
      const updated = [...projects, meta]
      setProjects(updated)
      saveProjects(updated)
      setNewProjectName('')
    }
  }

  const handleOpen = (p: ProjectMeta) => {
    p.lastOpened = new Date().toISOString()
    const updated = projects.map((x) => (x.path === p.path ? p : x))
    setProjects(updated)
    saveProjects(updated)
    onOpenProject(p)
    setIsOpen(false)
  }

  const handleDelete = (p: ProjectMeta) => {
    const updated = projects.filter((x) => x.path !== p.path)
    setProjects(updated)
    saveProjects(updated)
    if (currentProject === p.name) {
      onCloseProject()
    }
  }

  return (
    <>
      {/* 项目切换按钮 */}
      <button
        onClick={() => setIsOpen(!isOpen)}
        className="flex items-center gap-2 px-3 py-1.5 text-sm rounded border border-surface-200 dark:border-surface-700 text-surface-600 dark:text-surface-400 hover:bg-surface-100 dark:hover:bg-surface-800"
        title="项目管理"
      >
        <span>{currentProject || '无项目'}</span>
        <span className="text-xs opacity-50">▼</span>
      </button>

      {/* 下拉面板 */}
      {isOpen && (
        <div className="absolute top-full right-0 mt-1 w-80 bg-white dark:bg-surface-800 rounded-lg shadow-xl border border-surface-200 dark:border-surface-700 z-50">
          {/* 搜索 + 新建 */}
          <div className="p-3 border-b border-surface-100 dark:border-surface-700">
            <div className="flex gap-2">
              <input
                value={filter}
                onChange={(e) => setFilter(e.target.value)}
                placeholder="搜索项目…"
                className="flex-1 px-2 py-1 text-sm rounded border border-surface-200 dark:border-surface-700 bg-surface-50 dark:bg-surface-900 text-surface-900 dark:text-surface-50"
              />
              <button
                onClick={() => setIsCreating(!isCreating)}
                className="px-3 py-1 text-sm rounded bg-primary-500 text-white hover:bg-primary-600"
              >
                + 新建
              </button>
            </div>
          </div>

          {/* 新建项目表单 */}
          {isCreating && (
            <div className="p-3 border-b border-surface-100 dark:border-surface-700">
              <div className="flex gap-2">
                <input
                  autoFocus
                  value={newProjectName}
                  onChange={(e) => setNewProjectName(e.target.value)}
                  onKeyDown={(e) => { if (e.key === 'Enter') handleCreate() }}
                  placeholder="项目名称…"
                  className="flex-1 px-2 py-1 text-sm rounded border border-surface-200 dark:border-surface-700 bg-surface-50 dark:bg-surface-900 text-surface-900 dark:text-surface-50"
                />
                <button
                  onClick={handleCreate}
                  disabled={!newProjectName.trim()}
                  className="px-3 py-1 text-sm rounded bg-green-500 text-white hover:bg-green-600 disabled:opacity-40"
                >
                  创建
                </button>
              </div>
            </div>
          )}

          {/* 项目列表 */}
          <div className="max-h-64 overflow-y-auto">
            {recentProjects.length === 0 ? (
              <div className="p-6 text-center text-sm text-surface-400 dark:text-surface-500">
                {/* ── 空状态引导（B-35-06）────────────────────── */}
                {projects.length === 0 ? (
                  <div>
                    <p className="mb-2">还没有项目？从模板开始</p>
                    <button
                      onClick={() => setIsCreating(true)}
                      className="px-4 py-1.5 text-sm rounded bg-primary-500 text-white hover:bg-primary-600"
                    >
                      创建第一个项目
                    </button>
                  </div>
                ) : (
                  <p>没有匹配的项目</p>
                )}
              </div>
            ) : (
              recentProjects.map((p) => (
                <div
                  key={p.path}
                  className={`flex items-center justify-between px-3 py-2 hover:bg-surface-100 dark:hover:bg-surface-700 ${
                    currentProject === p.name ? 'bg-primary-50 dark:bg-primary-900/20' : ''
                  }`}
                >
                  <button
                    onClick={() => handleOpen(p)}
                    className="flex-1 text-left"
                  >
                    <div className="text-sm font-medium text-surface-900 dark:text-surface-50">
                      {p.name}
                    </div>
                    <div className="text-xs text-surface-400 dark:text-surface-500">
                      {new Date(p.lastOpened).toLocaleDateString('zh-CN')}
                    </div>
                  </button>
                  <button
                    onClick={(e) => { e.stopPropagation(); handleDelete(p) }}
                    className="ml-2 text-xs text-red-400 hover:text-red-600 opacity-50 hover:opacity-100"
                    title="删除"
                  >
                    ✕
                  </button>
                </div>
              ))
            )}
          </div>
        </div>
      )}

      {/* 背景遮罩 - 点击关闭 */}
      {isOpen && (
        <div className="fixed inset-0 z-40" onClick={() => setIsOpen(false)} />
      )}
    </>
  )
}
