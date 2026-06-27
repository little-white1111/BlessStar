/**
 * CasConflictDialog.tsx — CAS 乐观锁冲突弹窗
 * DAY38-07: CAS conflict UX with etag version number
 *
 * 当 commitBatch 因版本冲突（etag 不匹配）失败时弹出此对话框，
 * 展示冲突详情并提供刷新当前配置或保留本地编辑的选项。
 */

import React, { useEffect, useState } from 'react'

export interface CasConflictInfo {
  etag: number
  serverEtag: number
  conflictingFields: string[]
  message: string
}

interface Props {
  conflict: CasConflictInfo | null
  onRefresh: () => void
  onKeepLocal: () => void
  onDismiss: () => void
}

const CasConflictDialog: React.FC<Props> = ({
  conflict,
  onRefresh,
  onKeepLocal,
  onDismiss,
}) => {
  const [visible, setVisible] = useState(false)

  useEffect(() => {
    if (conflict) {
      setVisible(true)
    }
  }, [conflict])

  if (!visible || !conflict) return null

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/40">
      <div className="bg-white dark:bg-surface-800 rounded-xl shadow-2xl p-6 max-w-md w-full mx-4">
        <div className="flex items-start gap-3 mb-4">
          <span className="text-2xl">&#9888;</span>
          <div>
            <h3 className="text-lg font-semibold text-surface-900 dark:text-surface-50">
              配置版本冲突
            </h3>
            <p className="text-sm text-surface-500 dark:text-surface-400 mt-1">
              {conflict.message || '另一位用户或进程已修改了相同的配置项。'}
            </p>
          </div>
        </div>

        <div className="bg-surface-100 dark:bg-surface-700 rounded-lg p-3 mb-4 text-sm">
          <div className="flex justify-between mb-1">
            <span className="text-surface-500">你的版本 (etag):</span>
            <span className="font-mono text-amber-600">{conflict.etag}</span>
          </div>
          <div className="flex justify-between">
            <span className="text-surface-500">服务器版本 (etag):</span>
            <span className="font-mono text-green-600">{conflict.serverEtag}</span>
          </div>
          {conflict.conflictingFields.length > 0 && (
            <div className="mt-2 pt-2 border-t border-surface-200 dark:border-surface-600">
              <span className="text-surface-500">冲突字段:</span>
              <div className="mt-1 space-y-0.5">
                {conflict.conflictingFields.map((f) => (
                  <code key={f} className="text-xs bg-surface-200 dark:bg-surface-600 px-1.5 py-0.5 rounded">
                    {f}
                  </code>
                ))}
              </div>
            </div>
          )}
        </div>

        <div className="flex gap-3 justify-end">
          <button
            onClick={() => {
              setVisible(false)
              onDismiss()
            }}
            className="btn-ghost text-sm px-4 py-2"
          >
            忽略
          </button>
          <button
            onClick={() => {
              setVisible(false)
              onKeepLocal()
            }}
            className="btn-secondary text-sm px-4 py-2"
          >
            保留本地
          </button>
          <button
            onClick={() => {
              setVisible(false)
              onRefresh()
            }}
            className="btn-primary text-sm px-4 py-2"
          >
            刷新并重试
          </button>
        </div>
      </div>
    </div>
  )
}

export default CasConflictDialog
