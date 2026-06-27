import { useNavigate } from 'react-router-dom'

function Dashboard() {
  const navigate = useNavigate()

  return (
    <div className="max-w-4xl mx-auto space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-surface-900 dark:text-surface-50">
            仪表盘
          </h1>
          <p className="text-sm text-surface-500 dark:text-surface-400 mt-1">
            LiveDesign 配置编辑器概览
          </p>
        </div>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
        <StatCard
          title="配置模板"
          value="12"
          description="可用配置模板数量"
          color="primary"
        />
        <StatCard
          title="最近编辑"
          value="3"
          description="最近 7 天编辑的配置"
          color="surface"
        />
        <StatCard
          title="已保存"
          value="5"
          description="全部已保存配置"
          color="surface"
        />
      </div>

      <div className="card p-6">
        <h2 className="text-lg font-semibold mb-4">快速操作</h2>
        <div className="flex flex-wrap gap-3">
          <button
            onClick={() => navigate('/editor')}
            className="btn-primary"
          >
            打开编辑器
          </button>
          <button className="btn-secondary">
            导入配置
          </button>
          <button className="btn-secondary">
            查看文档
          </button>
        </div>
      </div>

      <div className="card p-6">
        <h2 className="text-lg font-semibold mb-4">最近活动</h2>
        <div className="text-sm text-surface-400 dark:text-surface-500 text-center py-8">
          暂无活动记录。开始编辑配置后将在此显示。
        </div>
      </div>
    </div>
  )
}

interface StatCardProps {
  title: string
  value: string
  description: string
  color: 'primary' | 'surface'
}

function StatCard({ title, value, description, color }: StatCardProps) {
  const colorClasses =
    color === 'primary'
      ? 'bg-primary-50 dark:bg-primary-900/20 border-primary-200 dark:border-primary-800'
      : 'bg-white dark:bg-surface-800 border-surface-200 dark:border-surface-700'

  return (
    <div className={`card p-5 ${colorClasses}`}>
      <h3 className="text-sm font-medium text-surface-500 dark:text-surface-400">
        {title}
      </h3>
      <p className="text-3xl font-bold mt-2 text-surface-900 dark:text-surface-50">
        {value}
      </p>
      <p className="text-xs text-surface-400 dark:text-surface-500 mt-1">
        {description}
      </p>
    </div>
  )
}

export default Dashboard
