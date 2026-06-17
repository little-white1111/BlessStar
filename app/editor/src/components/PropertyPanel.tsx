function PropertyPanel() {
  return (
    <aside className="w-72 bg-white dark:bg-surface-800 border-l border-surface-200 dark:border-surface-700 overflow-y-auto">
      <div className="h-14 flex items-center px-4 border-b border-surface-200 dark:border-surface-700">
        <h3 className="font-medium text-sm text-surface-700 dark:text-surface-300">
          属性面板
        </h3>
      </div>

      <div className="p-4 space-y-4">
        <p className="text-xs text-surface-400 dark:text-surface-500">
          选中表单控件后，在此编辑其属性。
        </p>

        {/* Placeholder sections */}
        <Section label="基本属性">
          <PropertyRow label="标识符" value="未选中" />
          <PropertyRow label="标签" value="未选中" />
          <PropertyRow label="类型" value="未选中" />
        </Section>

        <Section label="验证规则">
          <PropertyRow label="必填" value="未选中" />
          <PropertyRow label="最小值" value="未选中" />
          <PropertyRow label="最大值" value="未选中" />
        </Section>

        <Section label="外观">
          <PropertyRow label="占位文本" value="未选中" />
          <PropertyRow label="描述" value="未选中" />
        </Section>
      </div>
    </aside>
  )
}

function Section({ label, children }: { label: string; children: React.ReactNode }) {
  return (
    <div>
      <h4 className="text-xs font-semibold text-surface-500 dark:text-surface-400 uppercase tracking-wider mb-2">
        {label}
      </h4>
      <div className="space-y-2">{children}</div>
    </div>
  )
}

function PropertyRow({ label, value }: { label: string; value: string }) {
  return (
    <div className="flex items-center justify-between text-xs">
      <span className="text-surface-500 dark:text-surface-400">{label}</span>
      <span className="text-surface-700 dark:text-surface-300">{value}</span>
    </div>
  )
}

export default PropertyPanel
