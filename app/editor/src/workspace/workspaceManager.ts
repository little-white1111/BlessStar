/**
 * WorkspaceManager — front-end service for Project Workspace.
 *
 * ADR-全链路接通 不变量 #2 (Workspace 数据主权):
 *   All config file reads/writes must go through bs_workspace_* C ABI.
 *   This frontend manager communicates to the backend via Electron IPC.
 */

import { detectFormat, type ConfigFormat } from './formatDetect'

interface SourceInfo {
  relPath: string
  format: ConfigFormat
}

interface WorkspaceMetadata {
  projectRoot: string
  sources: SourceInfo[]
}

export class WorkspaceManager {
  private metadata: WorkspaceMetadata | null = null

  /**
   * Create a new workspace project.
   * Calls Electron IPC → Rust Addon → bs_workspace_create()
   */
  async create(projectRoot: string): Promise<void> {
    // IPC call to main process
    const result = await window.blessstar?.workspace?.create(projectRoot)
    if (result?.error) throw new Error(result.error)

    this.metadata = {
      projectRoot,
      sources: [],
    }
  }

  /**
   * Open an existing workspace project.
   */
  async open(projectRoot: string): Promise<void> {
    const result = await window.blessstar?.workspace?.open(projectRoot)
    if (result?.error) throw new Error(result.error)
    this.metadata = result ?? null
  }

  /**
   * Add a source file to the workspace.
   */
  async addSource(relPath: string, format?: ConfigFormat): Promise<void> {
    if (!this.metadata) throw new Error('No workspace open')

    const fmt = format ?? 'auto'
    await window.blessstar?.workspace?.addSource(relPath, fmt)

    this.metadata.sources.push({ relPath, format: fmt })
  }

  /**
   * Read a config source through the workspace pipeline.
   * Returns v1 JSON string.
   */
  async readConfig(srcName: string): Promise<string> {
    if (!this.metadata) throw new Error('No workspace open')

    const result = await window.blessstar?.workspace?.readConfig(srcName)
    if (result?.error) throw new Error(result.error)
    return result?.data ?? ''
  }

  /**
   * Write v1 JSON data to a config source.
   */
  async writeConfig(srcName: string, data: string): Promise<void> {
    if (!this.metadata) throw new Error('No workspace open')

    const result = await window.blessstar?.workspace?.writeConfig(srcName, data)
    if (result?.error) throw new Error(result.error)
  }

  /**
   * Build the workspace, converting all sources to target format.
   */
  async build(targetFormat: ConfigFormat = 'json'): Promise<void> {
    if (!this.metadata) throw new Error('No workspace open')

    const result = await window.blessstar?.workspace?.build(targetFormat)
    if (result?.error) throw new Error(result.error)
  }

  /**
   * Export a single source to a specific format and path.
   */
  async export(srcName: string, targetFormat: ConfigFormat, outputPath: string): Promise<void> {
    if (!this.metadata) throw new Error('No workspace open')

    const result = await window.blessstar?.workspace?.export(srcName, targetFormat, outputPath)
    if (result?.error) throw new Error(result.error)
  }

  /**
   * Get the list of sources in the workspace.
   */
  getSources(): SourceInfo[] {
    return this.metadata?.sources ?? []
  }

  /**
   * Get the project root path.
   */
  getProjectRoot(): string | null {
    return this.metadata?.projectRoot ?? null
  }

  /**
   * Detect format from file content and name.
   */
  detectFormat(data: Uint8Array, filename?: string) {
    return detectFormat(data, filename)
  }
}
