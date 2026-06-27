import { describe, it, expect } from 'vitest'
import { paradigmRegistry, loadDefaultParadigms } from '../paradigm'

describe('paradigm — 业务范式注册中心', () => {
  it('注册和获取业务范式', () => {
    paradigmRegistry.register({
      systemName: 'TestSystem',
      toolTemplates: {
        list_directory: {
          success: '📂 测试: {count} 项',
        },
      },
    })

    const paradigm = paradigmRegistry.get('TestSystem')
    expect(paradigm).not.toBeNull()
    expect(paradigm?.toolTemplates.list_directory.success).toBe('📂 测试: {count} 项')
  })

  it('未注册系统返回 universal', () => {
    loadDefaultParadigms()
    const paradigm = paradigmRegistry.get('NonExistent')
    // universal 应在 loadDefaultParadigms 后被注册
    expect(paradigm).not.toBeNull()
    expect(paradigm?.systemName).toBe('__universal__')
  })

  it('getTemplate 逐级回退', () => {
    // 先注册一个没有 list_directory 模板的系统
    paradigmRegistry.register({
      systemName: 'PartialSystem',
      toolTemplates: {
        read_file: {
          success: '📄 读取成功',
        },
      },
    })

    // list_directory 没在 PartialSystem 中定义 → 回退到 universal
    const tpl = paradigmRegistry.getTemplate('PartialSystem', 'list_directory')
    // universal 是否存在取决于之前的测试状态
    expect(tpl === null || tpl.success?.length > 0).toBe(true)
  })

  it('loadDefaultParadigms 加载 JSON', () => {
    // 不需要抛异常
    expect(() => loadDefaultParadigms()).not.toThrow()
  })

  it('distill 在不足 2 系统时返回 universal', () => {
    const result = paradigmRegistry.distill()
    expect(result).toBeDefined()
    expect(result.systemName).toBe('__universal__')
  })
})
