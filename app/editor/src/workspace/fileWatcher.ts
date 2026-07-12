/**
 * FileWatcher — watches workspace source files for external changes.
 *
 * ADR-全链路接通 不变量 #6 (元数据一致性):
 *   .blessstar/ 目录必须时刻与 src/ 内容一致。
 *   If src/ files are modified externally, the watcher detects the change
 *   and triggers a schema re-scan and workspace.yaml update.
 */

export type FileChangeCallback = (event: 'change' | 'rename', relPath: string) => void

export interface FileWatcherOptions {
  /** Polling interval in ms (default: 3000). */
  intervalMs?: number
  /** Callback invoked on file changes. */
  onChanged?: FileChangeCallback
  /** Glob patterns to ignore. */
  ignore?: string[]
}

interface FileEntry {
  path: string
  mtime: number
  size: number
}

export class FileWatcher {
  private rootPath: string
  private files = new Map<string, FileEntry>()
  private timer: ReturnType<typeof setInterval> | null = null
  private options: Required<FileWatcherOptions>

  constructor(rootPath: string, options?: FileWatcherOptions) {
    this.rootPath = rootPath
    this.options = {
      intervalMs: options?.intervalMs ?? 3000,
      onChanged: options?.onChanged ?? (() => {}),
      ignore: options?.ignore ?? ['node_modules', '.git', '.blessstar', 'dist'],
    }
  }

  /**
   * Start watching the workspace src/ directory.
   */
  start(): void {
    if (this.timer) return

    // Initial scan
    this.scan()

    this.timer = setInterval(() => {
      this.scan()
    }, this.options.intervalMs)
  }

  /**
   * Stop watching.
   */
  stop(): void {
    if (this.timer) {
      clearInterval(this.timer)
      this.timer = null
    }
    this.files.clear()
  }

  /**
   * Check if the watcher is running.
   */
  get running(): boolean {
    return this.timer !== null
  }

  /**
   * Perform a scan of the src/ directory.
   */
  private async scan(): Promise<void> {
    try {
      const entries = await window.blessstar?.workspace?.listDirectory?.(this.rootPath)
      if (!entries) return

      const currentPaths = new Set<string>()

      for (const entry of entries) {
        const relPath = entry.name
        const fullPath = `${this.rootPath}/${relPath}`

        // Skip ignored patterns
        if (this.options.ignore.some(p => relPath.includes(p))) continue

        currentPaths.add(relPath)
        const prev = this.files.get(relPath)

        if (!prev) {
          // New file detected
          this.files.set(relPath, {
            path: relPath,
            mtime: entry.mtime ?? Date.now(),
            size: entry.size ?? 0,
          })
          this.options.onChanged('rename', relPath)
        } else if (entry.mtime && entry.mtime > prev.mtime) {
          // File modified
          prev.mtime = entry.mtime
          prev.size = entry.size ?? 0
          this.options.onChanged('change', relPath)
        }
      }

      // Detect deletions
      for (const [relPath] of this.files) {
        if (!currentPaths.has(relPath)) {
          this.files.delete(relPath)
          this.options.onChanged('rename', relPath)
        }
      }
    } catch {
      // Silently fail — workspace may not be ready yet
    }
  }
}
