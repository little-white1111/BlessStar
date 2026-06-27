/**
 * ProjectExport — 项目导出/导入工具函数（第35天 · UX-05）
 *
 * 导出：将项目配置序列化为 JSON 文件 → 用户选择保存路径
 * 导入：用户选择 .json 文件 → 解析 → 返回项目数据
 *
 * MVP 默认：纯 JSON 格式（不依赖 archiver），通过 Electron IPC save/open dialog 完成文件操作。
 */

/** 项目导出数据格式 */
export interface ProjectExportData {
  /** 格式版本号 */
  formatVersion: '1.0'
  /** 导出时间 */
  exportedAt: string
  /** 项目元数据 */
  project: {
    name: string
    createdAt: string
    lastOpened: string
    template?: string
  }
  /** Schema 定义 */
  schema: unknown
  /** Gate 链定义 */
  gateChains: unknown
  /** 当前配置值 */
  config: Record<string, unknown>
}

/**
 * 将项目数据序列化为导出 JSON 字符串
 */
export function serializeProjectExport(data: ProjectExportData): string {
  return JSON.stringify(data, null, 2)
}

/**
 * 解析导入的 JSON 字符串为项目数据
 * @returns { valid, data, error }
 */
export function deserializeProjectImport(
  json: string
): { valid: boolean; data?: ProjectExportData; error?: string } {
  try {
    const obj = JSON.parse(json)
    if (!obj || typeof obj !== 'object') {
      return { valid: false, error: '文件格式无效：不是有效的 JSON 对象' }
    }
    if (obj.formatVersion !== '1.0') {
      return { valid: false, error: `不支持的格式版本: ${obj.formatVersion}` }
    }
    if (!obj.project || !obj.project.name) {
      return { valid: false, error: '文件缺少项目元数据' }
    }
    return { valid: true, data: obj as ProjectExportData }
  } catch (e) {
    return { valid: false, error: `JSON 解析失败: ${(e as Error).message}` }
  }
}

/**
 * 比较导入的项目是否与当前数据一致
 */
export function verifyProjectImport(
  original: ProjectExportData,
  imported: ProjectExportData
): { consistent: boolean; diffs: string[] } {
  const diffs: string[] = []
  if (JSON.stringify(original.schema) !== JSON.stringify(imported.schema)) {
    diffs.push('Schema 定义不一致')
  }
  if (JSON.stringify(original.gateChains) !== JSON.stringify(imported.gateChains)) {
    diffs.push('Gate 链定义不一致')
  }
  if (JSON.stringify(original.config) !== JSON.stringify(imported.config)) {
    diffs.push('配置值不一致')
  }
  return { consistent: diffs.length === 0, diffs }
}
