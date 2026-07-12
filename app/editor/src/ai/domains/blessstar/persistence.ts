/**
 * BlessStar Persistence Provider — reads/writes configs via Electron IPC.
 *
 * ADR-全链路接通 不变量 #2 (Workspace 数据主权):
 *   All config file reads/writes go through bs_workspace_* C ABI.
 *   This provider communicates via window.blessstar.workspace.* IPC.
 */

import type { IPersistenceProvider } from '../../engine/types'

/**
 * BlessStarPersistenceProvider — wraps Workspace IPC calls.
 *
 * The provider is instantiated per-workspace and uses the existing
 * WorkspaceManager (window.blessstar.workspace.*) under the hood.
 */
export class BlessStarPersistenceProvider implements IPersistenceProvider {
  /**
   * Save data to a config source.
   * @param data  The data to persist. For config writes, should be
   *              { key: string, value: unknown, source?: string }.
   * @param path  The config key path (e.g. "server.port").
   */
  async save(data: unknown, path: string): Promise<void> {
    const payload = data as { key: string; value: unknown; source?: string }

    if (payload.source) {
      // Write to a specific source file via IPC → bs_workspace_write
      const jsonData = JSON.stringify({ [payload.key]: payload.value })
      await window.blessstar?.workspace?.writeConfig(
        `src/${payload.source}`,
        jsonData,
      )
    } else {
      // Write via generic IPC → addon.writeBlessStarConfig
      const result = await window.blessstar?.executeTool?.('write_config_value', {
        key: path,
        value: String(payload.value ?? ''),
      })
      if (result?.success === false) {
        throw new Error(result.error || 'write_config_value failed')
      }
    }
  }

  /**
   * Load data for a config key.
   * @param path  The config key path (e.g. "server.port").
   * @returns The value, or null if not found.
   */
  async load(path: string): Promise<unknown> {
    const result = await window.blessstar?.executeTool?.('read_config_value', {
      key: path,
    })
    if (result?.success) {
      return result.result
    }
    return null
  }

  /**
   * Delete a config key.
   * @param path  The config key path to delete.
   */
  async delete(path: string): Promise<void> {
    // For MVP: write empty string (native backend will handle)
    await this.save({ key: path, value: '' }, path)
  }

  /**
   * List all configs matching a prefix.
   * @param prefix  Optional key prefix filter.
   */
  async list(prefix?: string): Promise<Array<{ key: string; value: unknown }>> {
    const result = await window.blessstar?.executeTool?.('list_configs', {
      prefix: prefix ?? '',
    })
    if (result?.success) {
      try {
        const parsed = JSON.parse(result.result as string)
        return (parsed.configs || []).map((c: any) => ({
          key: c.key,
          value: c.value ?? c.default ?? null,
        }))
      } catch {
        return []
      }
    }
    return []
  }
}
