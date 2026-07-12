/**
 * FormatDetect Unit Tests
 *
 * ADR-全链路接通 不变量 #5 (格式转换双向幂等性):
 *   格式检测必须能正确识别所有支持的格式。
 *
 * API: detectFormat(data: Uint8Array, filename?: string): DetectResult
 */

import { describe, it, expect } from 'vitest'
import { detectFormat } from './formatDetect'
import type { DetectResult } from './formatDetect'

/** Helper: string → Uint8Array */
const enc = (s: string): Uint8Array => new TextEncoder().encode(s)

/** Assert format + minimum confidence */
const assertFormat = (result: DetectResult, expectedFmt: string, minConf = 50) => {
  expect(result.format).toBe(expectedFmt)
  expect(result.confidence).toBeGreaterThanOrEqual(minConf)
}

describe('detectFormat', () => {
  describe('JSON detection (magic bytes)', () => {
    it('should detect JSON by leading {', () => {
      assertFormat(detectFormat(enc('{"key": "value"}')), 'json', 90)
    })

    it('should detect JSON array by leading [', () => {
      assertFormat(detectFormat(enc('[1, 2, 3]')), 'json', 85)
    })

    it('should detect nested JSON', () => {
      assertFormat(detectFormat(enc('{"a": {"b": [1, 2]}}')), 'json', 90)
    })
  })

  describe('YAML detection (extension)', () => {
    it('should detect .yaml by extension', () => {
      assertFormat(detectFormat(enc('key: value'), 'config.yaml'), 'yaml', 60)
    })

    it('should detect .yml by extension', () => {
      assertFormat(detectFormat(enc('key: value'), 'config.yml'), 'yaml', 60)
    })
  })

  describe('TOML detection', () => {
    it('should detect .toml by extension regardless of content', () => {
      assertFormat(detectFormat(enc('title = "example"'), 'config.toml'), 'toml', 60)
    })

    it('should detect TOML with key=value via content + extension', () => {
      assertFormat(detectFormat(enc('port = 8080\nhost = "localhost"'), 'server.toml'), 'toml', 60)
    })
  })

  describe('Extension detection', () => {
    it('should detect .json extension', () => {
      assertFormat(detectFormat(enc(''), 'config.json'), 'json', 60)
    })

    it('should detect .toml extension', () => {
      assertFormat(detectFormat(enc(''), 'config.toml'), 'toml', 60)
    })

    it('should detect .ini extension', () => {
      assertFormat(detectFormat(enc(''), 'config.ini'), 'ini', 60)
    })
  })

  describe('Fallback behavior', () => {
    it('should default to json with low confidence for unknown content', () => {
      const r = detectFormat(enc('some random text'))
      expect(r.format).toBe('json')
      expect(r.confidence).toBe(30)
    })

    it('should default to json for empty content', () => {
      const r = detectFormat(new Uint8Array([]))
      expect(r.format).toBe('json')
      expect(r.confidence).toBe(30)
    })
  })
})
