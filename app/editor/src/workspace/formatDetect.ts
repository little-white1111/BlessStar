/**
 * FormatDetect — client-side format detection for workspace files.
 *
 * Mirrors the C-side bs_format_detect() logic in adapter/parser/config_format/.
 * Used in the Editor renderer process before sending to native backend.
 */

export type ConfigFormat = 'json' | 'yaml' | 'toml' | 'ini' | 'xml' | 'auto'

export interface DetectResult {
  format: ConfigFormat
  confidence: number
}

const MAGIC_PATTERNS: Array<{ pattern: number[]; format: ConfigFormat; confidence: number }> = [
  { pattern: [0x7B], format: 'json', confidence: 90 }, // {
  { pattern: [0x5B], format: 'json', confidence: 85 }, // [
  { pattern: [0x2D, 0x2D, 0x2D], format: 'yaml', confidence: 95 }, // ---
]

const EXTENSION_MAP: Record<string, ConfigFormat> = {
  '.json': 'json',
  '.yaml': 'yaml',
  '.yml': 'yaml',
  '.toml': 'toml',
  '.ini': 'ini',
  '.conf': 'ini',
  '.cfg': 'ini',
  '.xml': 'xml',
}

export function detectFormat(data: Uint8Array, filename?: string): DetectResult {
  // 1. Content-based detection
  for (const entry of MAGIC_PATTERNS) {
    if (data.length >= entry.pattern.length) {
      let match = true
      for (let i = 0; i < entry.pattern.length; i++) {
        if (data[i] !== entry.pattern[i]) {
          match = false
          break
        }
      }
      if (match) {
        return { format: entry.format, confidence: entry.confidence }
      }
    }
  }

  // Check for YAML-like content (key: value patterns)
  if (data.length > 0) {
    const header = new TextDecoder().decode(data.slice(0, Math.min(data.length, 512)))
    const lines = header.split('\n').filter(l => l.trim().length > 0)

    // TOML detection: [section] or key=value
    if (lines.some(l => /^\[.*\]$/.test(l.trim()) || /^[\w.]+\s*=/.test(l.trim()))) {
      if (lines.some(l => /^\[.*\]$/.test(l.trim()))) {
        return { format: 'toml', confidence: 80 }
      }
    }
  }

  // 2. Extension-based detection
  if (filename) {
    const dot = filename.lastIndexOf('.')
    if (dot >= 0) {
      const ext = filename.slice(dot).toLowerCase()
      const fmt = EXTENSION_MAP[ext]
      if (fmt) {
        return { format: fmt, confidence: 60 }
      }
    }
  }

  return { format: 'json', confidence: 30 }
}
