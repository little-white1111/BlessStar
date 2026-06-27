import React, { useState } from 'react'

interface LoginDialogProps {
  /** Called when authentication succeeds */
  onUnlock: () => void
  /** External auth verify function (via window.blessstar.executeTool) */
  authVerify?: (token: string) => Promise<{ success: boolean; valid?: number; error?: string }>
}

/** ── LoginDialog ────────────────────────────────────────────────
 * 第38天 ⑫ · 管理员密码登录对话框
 *
 * 首次启动（无密码时）：弹出「设置管理员密码」
 * 已有密码时：弹出「输入管理员密码」
 * 验证通过后调用 onUnlock()
 * ───────────────────────────────────────────────────────────────── */
const LoginDialog: React.FC<LoginDialogProps> = ({ onUnlock, authVerify }) => {
  const [password, setPassword] = useState('')
  const [confirmPassword, setConfirmPassword] = useState('')
  const [error, setError] = useState('')
  const [isFirstRun, setIsFirstRun] = useState(true) // 默认首次运行

  const handleSubmit = async () => {
    setError('')

    if (!password.trim()) {
      setError('密码不能为空')
      return
    }

    if (isFirstRun) {
      if (password !== confirmPassword) {
        setError('两次输入的密码不一致')
        return
      }
      if (password.length < 4) {
        setError('密码至少需要4个字符')
        return
      }
      // 首次运行：保存密码并解锁
      try {
        await (window as any).blessstar.executeTool('write_config_value', {
          key: '_auth_admin_password',
          value: password,
        })
        onUnlock()
      } catch {
        setError('保存密码失败')
      }
      return
    }

    // 非首次：验证密码
    if (authVerify) {
      try {
        const result = await authVerify(password)
        if (result.success && result.valid === 1) {
          onUnlock()
        } else {
          setError('密码错误')
        }
      } catch {
        setError('验证失败')
      }
    } else {
      // 降级：直接解锁（开发模式）
      onUnlock()
    }
  }

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter') handleSubmit()
  }

  return (
    <div
      style={{
        position: 'fixed',
        inset: 0,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        backgroundColor: 'rgba(0,0,0,0.5)',
        zIndex: 9999,
      }}
    >
      <div
        style={{
          background: '#fff',
          borderRadius: 8,
          padding: 32,
          width: 360,
          boxShadow: '0 4px 24px rgba(0,0,0,0.3)',
        }}
      >
        <h2 style={{ marginTop: 0, fontSize: 18, textAlign: 'center' }}>
          {isFirstRun ? '设置管理员密码' : 'BlessStar 管理员登录'}
        </h2>

        <p style={{ color: '#888', fontSize: 13, textAlign: 'center', marginBottom: 20 }}>
          {isFirstRun
            ? '首次启动，请设置管理员密码以保护配置安全'
            : '请输入管理员密码以继续'}
        </p>

        <input
          type="password"
          placeholder="管理员密码"
          value={password}
          onChange={(e) => setPassword(e.target.value)}
          onKeyDown={handleKeyDown}
          autoFocus
          style={{
            width: '100%',
            padding: '10px 12px',
            marginBottom: 12,
            borderRadius: 4,
            border: '1px solid #d9d9d9',
            fontSize: 14,
            boxSizing: 'border-box',
          }}
        />

        {isFirstRun && (
          <input
            type="password"
            placeholder="确认密码"
            value={confirmPassword}
            onChange={(e) => setConfirmPassword(e.target.value)}
            onKeyDown={handleKeyDown}
            style={{
              width: '100%',
              padding: '10px 12px',
              marginBottom: 12,
              borderRadius: 4,
              border: '1px solid #d9d9d9',
              fontSize: 14,
              boxSizing: 'border-box',
            }}
          />
        )}

        {error && (
          <p style={{ color: '#e74c3c', fontSize: 13, marginBottom: 12 }}>{error}</p>
        )}

        <button
          onClick={handleSubmit}
          style={{
            width: '100%',
            padding: '10px 0',
            background: '#1890ff',
            color: '#fff',
            border: 'none',
            borderRadius: 4,
            fontSize: 14,
            cursor: 'pointer',
          }}
        >
          {isFirstRun ? '设置密码并进入' : '登录'}
        </button>

        {!isFirstRun && (
          <button
            onClick={() => setIsFirstRun(true)}
            style={{
              width: '100%',
              padding: '8px 0',
              marginTop: 8,
              background: 'transparent',
              color: '#888',
              border: 'none',
              fontSize: 12,
              cursor: 'pointer',
            }}
          >
            重置密码（首次运行）
          </button>
        )}
      </div>
    </div>
  )
}

export default LoginDialog
