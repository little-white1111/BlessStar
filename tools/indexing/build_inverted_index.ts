/**
 * build_inverted_index — 从 compact 索引自动生成倒排索引
 *
 * 输入：field_semantics.compact（管道分隔格式）
 * 输出：tools/indexing/inverted_index.json
 *
 * 用法：
 *   npx ts-node tools/indexing/build_inverted_index.ts \
 *     --input .cursor/agents/biz-finance/field_semantics.compact \
 *     --output tools/indexing/inverted_index.json
 *
 * 或使用默认路径（从开发目录结构推断）。
 */

import * as fs from 'fs'
import * as path from 'path'

// ── 类型 ─────────────────────────────────────────────────────────────

interface InvertedIndex {
  [keyword: string]: string[]
}

interface CompactFieldRow {
  fieldKey: string
  type: string
  required: string
  widget: string
  aiHint: string
}

// ── 解析 compact 格式 ────────────────────────────────────────────────

/**
 * 解析 field_semantics.compact 管道分隔格式。
 *
 * 格式：
 * ```
 * # field_semantics.compact
 * field_key|type|required|widget|ai_hint
 * host_address|str|true|input|数据库主机地址
 * port|int|true|input|端口号
 * ```
 */
function parseCompact(content: string): CompactFieldRow[] {
  const rows: CompactFieldRow[] = []
  const lines = content.split('\n')
  let headerFound = false

  for (const line of lines) {
    const trimmed = line.trim()
    if (!trimmed || trimmed.startsWith('#')) continue
    if (!headerFound) {
      headerFound = true
      continue // skip header line
    }

    const cols = trimmed.split('|')
    if (cols.length >= 5) {
      rows.push({
        fieldKey: cols[0].trim(),
        type: cols[1].trim(),
        required: cols[2].trim(),
        widget: cols[3].trim(),
        aiHint: cols[4].trim(),
      })
    }
  }

  return rows
}

// ── 中文关键词提取 ──────────────────────────────────────────────────

/**
 * 从 ai_hint 提取中文关键词。匹配连续 2 个及以上中文字符。
 */
function extractChineseKeywords(text: string): string[] {
  const cjkPattern = /[\u4e00-\u9fff\u3400-\u4dbf]{2,}/g
  const matches = text.match(cjkPattern)
  return matches || []
}

/**
 * 从 field_key 提取语义片段（分隔符分割的单词）。
 */
function extractKeySegments(fieldKey: string): string[] {
  return fieldKey.split(/[._-]/).filter((s) => s.length > 0)
}

// ── 构建索引 ─────────────────────────────────────────────────────────

function buildIndex(inputPath: string): InvertedIndex {
  const content = fs.readFileSync(inputPath, 'utf-8')
  const rows = parseCompact(content)
  const index: InvertedIndex = {}

  for (const row of rows) {
    // 1. ai_hint → 中文关键词 → 映射到 fieldKey
    const keywords = extractChineseKeywords(row.aiHint)
    for (const kw of keywords) {
      if (!index[kw]) index[kw] = []
      if (!index[kw].includes(row.fieldKey)) {
        index[kw].push(row.fieldKey)
      }
    }

    // 2. field_key 分段 → 每个段作为关键词
    const segments = extractKeySegments(row.fieldKey)
    for (const seg of segments) {
      if (!index[seg]) index[seg] = []
      if (!index[seg].includes(row.fieldKey)) {
        index[seg].push(row.fieldKey)
      }
    }

    // 3. widget 类型映射
    const widgetKeywords: Record<string, string[]> = {
      input: ['输入', '输入框', '文本'],
      number: ['数字', '数值'],
      select: ['下拉', '选择'],
      switch: ['开关'],
      slider: ['滑块', '范围'],
    }
    const wKeywords = widgetKeywords[row.widget]
    if (wKeywords) {
      for (const wkw of wKeywords) {
        if (!index[wkw]) index[wkw] = []
        if (!index[wkw].includes(row.fieldKey)) {
          index[wkw].push(row.fieldKey)
        }
      }
    }

    // 4. type 映射
    const typeKeywords: Record<string, string[]> = {
      str: ['字符串', '文本'],
      int: ['整数', '整型'],
      float: ['浮点', '小数'],
      bool: ['布尔', '开关'],
    }
    const tKeywords = typeKeywords[row.type]
    if (tKeywords) {
      for (const tkw of tKeywords) {
        if (!index[tkw]) index[tkw] = []
        if (!index[tkw].includes(row.fieldKey)) {
          index[tkw].push(row.fieldKey)
        }
      }
    }
  }

  return index
}

// ── 主入口 ───────────────────────────────────────────────────────────

function main(): void {
  const args = process.argv.slice(2)
  const inputFlag = args.indexOf('--input')
  const outputFlag = args.indexOf('--output')

  let inputPath: string
  let outputPath: string

  if (inputFlag >= 0 && args[inputFlag + 1]) {
    inputPath = path.resolve(args[inputFlag + 1])
  } else {
    // 默认路径：从 tools/indexing/ 向上找项目根
    inputPath = path.resolve(__dirname, 'field_semantics.compact')
    if (!fs.existsSync(inputPath)) {
      // fallback: 从 .cursor/agents/ 读取第一个找到的 compact
      const agentsDir = path.resolve(__dirname, '../../.cursor/agents')
      if (fs.existsSync(agentsDir)) {
        const dirs = fs.readdirSync(agentsDir)
        for (const dir of dirs) {
          const candidate = path.join(agentsDir, dir, 'field_semantics.compact')
          if (fs.existsSync(candidate)) {
            inputPath = candidate
            break
          }
        }
      }
    }
  }

  if (outputFlag >= 0 && args[outputFlag + 1]) {
    outputPath = path.resolve(args[outputFlag + 1])
  } else {
    outputPath = path.resolve(__dirname, 'inverted_index.json')
  }

  if (!fs.existsSync(inputPath)) {
    console.error(`输入文件不存在: ${inputPath}`)
    console.error('请指定 --input <path>')
    process.exit(1)
  }

  console.log(`读取: ${inputPath}`)
  const index = buildIndex(inputPath)
  const outputDir = path.dirname(outputPath)
  if (!fs.existsSync(outputDir)) {
    fs.mkdirSync(outputDir, { recursive: true })
  }

  fs.writeFileSync(outputPath, JSON.stringify(index, null, 2), 'utf-8')
  console.log(`写入: ${outputPath}`)
  console.log(`索引条目: ${Object.keys(index).length}`)
  console.log(`覆盖字段: ${[...new Set(Object.values(index).flat())].length}`)
}

if (require.main === module) {
  main()
}
