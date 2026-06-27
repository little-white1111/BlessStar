/**
 * versionRegistry — 配置级版本注册表（第33天 · RV-01 配置级平坦映射）
 *
 * 每个 configKey 拥有独立的版本数组 [{versionId, displayName, value, timestamp, userInput}]。
 * 不形成链，不截断；仅 WRITE 操作产生版本。
 * 持久化走独立文件（userData/version_registry.json），不经过 Gate 链。
 */

import type { ConfigVersion, VersionRegistry } from './types'

/** 从持久化文件加载版本注册表 */
export async function loadVersionRegistry(): Promise<VersionRegistry> {
  try {
    const raw = await window.blessstar.loadVersionRegistry()
    if (!raw) return {}
    return JSON.parse(raw)
  } catch {
    return {}
  }
}

/** 将版本注册表持久化到文件 */
export async function saveVersionRegistry(registry: VersionRegistry): Promise<void> {
  try {
    const json = JSON.stringify(registry, null, 2)
    await window.blessstar.saveVersionRegistry(json)
  } catch (err) {
    console.warn('[VersionRegistry] 保存失败:', err)
  }
}

/**
 * 为一次管线执行产生的写入条目追加版本。
 * @param writeEntries 本次写入的 {key, value} 条目
 * @param userInput 用户对话原文（用于追溯）
 * @returns 更新后的注册表，以及每个 key→versionId 的映射
 */
export async function addVersionEntries(
  writeEntries: Array<{ key: string; value: string }>,
  userInput: string,
): Promise<{ registry: VersionRegistry; newVersionIds: Record<string, string> }> {
  const registry = await loadVersionRegistry()
  const now = Date.now()
  const newVersionIds: Record<string, string> = {}

  for (const entry of writeEntries) {
    if (!entry.key) continue
    const versions = registry[entry.key] || []
    const seq = versions.length + 1
    const versionId = `${entry.key}_v${seq}`
    versions.push({
      versionId,
      displayName: '',
      value: entry.value,
      timestamp: now,
      userInput,
    })
    registry[entry.key] = versions
    newVersionIds[entry.key] = versionId
  }

  await saveVersionRegistry(registry)
  return { registry, newVersionIds }
}

/** 获取某个配置 key 的全部版本 */
export async function getVersionsForKey(configKey: string): Promise<ConfigVersion[]> {
  const registry = await loadVersionRegistry()
  return registry[configKey] || []
}

/** 按 versionId 查找版本 */
export async function getVersionById(
  configKey: string,
  versionId: string,
): Promise<ConfigVersion | null> {
  const versions = await getVersionsForKey(configKey)
  return versions.find((v) => v.versionId === versionId) || null
}

/** 为指定版本重命名 */
export async function renameVersion(
  configKey: string,
  versionId: string,
  displayName: string,
): Promise<void> {
  const registry = await loadVersionRegistry()
  const versions = registry[configKey]
  if (!versions) return
  const v = versions.find((c) => c.versionId === versionId)
  if (v) v.displayName = displayName
  await saveVersionRegistry(registry)
}
