/**
 * DomainPluginRegistry — singleton registry for domain plugins.
 *
 * ADR-全链路接通 不变量 #3 (Plugin 隔离性):
 *   Each plugin is stored by id; cross-plugin access is not allowed.
 * ADR-全链路接通 不变量 #9 (Plugin 版本契约):
 *   Plugins with mismatched supportPipelineVersion are rejected.
 */

import type { IDomainPlugin } from './types'

const CURRENT_PIPELINE_VERSION = '1.0.0'

class DomainPluginRegistryImpl {
  private plugins = new Map<string, IDomainPlugin>()
  private defaultPluginId: string | null = null

  register(plugin: IDomainPlugin): boolean {
    // Version check (不变量 #9)
    if (plugin.supportedPipelineVersion !== CURRENT_PIPELINE_VERSION) {
      console.warn(
        `[DomainPluginRegistry] Plugin "${plugin.id}" v${plugin.version} ` +
          `requires pipeline v${plugin.supportedPipelineVersion}, ` +
          `but current is v${CURRENT_PIPELINE_VERSION}. Rejected.`
      )
      return false
    }

    if (this.plugins.has(plugin.id)) {
      console.warn(`[DomainPluginRegistry] Plugin "${plugin.id}" already registered — overwriting`)
    }

    this.plugins.set(plugin.id, plugin)
    if (!this.defaultPluginId) {
      this.defaultPluginId = plugin.id
    }

    console.log(`[DomainPluginRegistry] Registered plugin: ${plugin.id} v${plugin.version}`)
    return true
  }

  get(id: string): IDomainPlugin | undefined {
    return this.plugins.get(id)
  }

  getAll(): IDomainPlugin[] {
    return Array.from(this.plugins.values())
  }

  getDefault(): IDomainPlugin | undefined {
    return this.defaultPluginId ? this.plugins.get(this.defaultPluginId) : undefined
  }

  setDefault(id: string): void {
    if (this.plugins.has(id)) {
      this.defaultPluginId = id
    }
  }

  clear(): void {
    this.plugins.clear()
    this.defaultPluginId = null
  }
}

export const DomainPluginRegistry = new DomainPluginRegistryImpl()
