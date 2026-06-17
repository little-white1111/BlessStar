function Settings() {
  return (
    <div className="max-w-2xl mx-auto space-y-6">
      <div>
        <h1 className="text-2xl font-bold text-surface-900 dark:text-surface-50">
          设置
        </h1>
        <p className="text-sm text-surface-500 dark:text-surface-400 mt-1">
          编辑器全局设置
        </p>
      </div>

      <div className="card p-6 space-y-6">
        <SettingSection title="编辑器">
          <SettingRow label="字体大小" control={<select className="input-field w-32"><option>12</option><option>13</option><option selected>14</option><option>15</option><option>16</option><option>18</option><option>20</option></select>} />
          <SettingRow label="制表符大小" control={<select className="input-field w-32"><option>2</option><option selected>4</option><option>8</option></select>} />
          <SettingRow label="自动保存" control={<label className="relative inline-flex items-center cursor-pointer"><input type="checkbox" className="sr-only peer" /><div className="w-9 h-5 bg-surface-300 dark:bg-surface-600 peer-focus:outline-none peer-focus:ring-2 peer-focus:ring-primary-500/40 rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:start-[2px] after:bg-white after:rounded-full after:h-4 after:w-4 after:transition-all peer-checked:bg-primary-600"></div></label>} />
        </SettingSection>

        <SettingSection title="外观">
          <SettingRow label="主题" control={<select className="input-field w-40"><option>跟随系统</option><option>浅色</option><option>深色</option></select>} />
          <SettingRow label="语言" control={<select className="input-field w-40"><option selected>简体中文</option><option>English</option></select>} />
        </SettingSection>

        <SettingSection title="BlessStar 集成">
          <SettingRow label="napi-rs Addon 路径" control={<input type="text" className="input-field" placeholder="native/blessstar_core.linux-x64-gnu.node" />} />
          <SettingRow label="配置模板目录" control={<input type="text" className="input-field" placeholder="templates/" />} />
        </SettingSection>
      </div>

      <div className="text-right text-xs text-surface-400 dark:text-surface-500">
        BlessStar Config Editor v1.0.0
      </div>
    </div>
  )
}

function SettingSection({ title, children }: { title: string; children: React.ReactNode }) {
  return (
    <div>
      <h3 className="text-sm font-semibold text-surface-700 dark:text-surface-300 mb-4">
        {title}
      </h3>
      <div className="space-y-4">{children}</div>
    </div>
  )
}

function SettingRow({ label, control }: { label: string; control: React.ReactNode }) {
  return (
    <div className="flex items-center justify-between">
      <span className="text-sm text-surface-600 dark:text-surface-400">{label}</span>
      {control}
    </div>
  )
}

export default Settings
