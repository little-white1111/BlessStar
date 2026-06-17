import { Routes, Route } from 'react-router-dom'
import Layout from './components/Layout'
import Dashboard from './routes/Dashboard'
import Editor from './routes/Editor'
import Settings from './routes/Settings'

function App() {
  return (
    <Layout>
      <Routes>
        <Route path="/" element={<Dashboard />} />
        <Route path="/editor" element={<Editor />} />
        <Route path="/settings" element={<Settings />} />
      </Routes>
    </Layout>
  )
}

export default App
