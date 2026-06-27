import React from 'react'
import ReactDOM from 'react-dom/client'
import { HashRouter } from 'react-router-dom'
import App from './App'
import './styles/globals.css'

// 同步数据到 AI 管线模块级缓存（仅在 module 初始化时注册可触发自动同步）
import('./ai/tools/configLabels').then(m => m.refreshFromRegistry())
import('./ai/context-manager/adaptiveIndex').then(m => m.syncBaselineFromRegistry())
import('./ai/intent/trie_dict').then(m => m.syncDomainKWFromRegistry())
import('./ai/context-manager/skillRouter').then(m => m.syncSkillsFromRegistry())

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <HashRouter>
      <App />
    </HashRouter>
  </React.StrictMode>,
)
