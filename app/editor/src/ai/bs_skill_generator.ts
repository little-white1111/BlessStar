// GenerateResult is defined locally in this file

const JSON_PARSE_ERR_PREFIX = 'bs_skill_generator: Failed to parse'

/* ── Input JSON shapes (mirroring agent_indexer output) ────────────── */

interface DomainKnowledge {
  version: string
  generated_at: string
  business_name: string
  domains: Array<{
    name: string
    field_count: number
    fields: string[]
  }>
}

interface ConstraintKnowledge {
  version: string
  generated_at: string
  business_name: string
  gates: Array<{
    gate_id: string
    scenario: string
    field_key: string
    op: string
    value: string
    layer: number
    sub_category: string
    stable_key: string
    ai_hint?: string
  }>
}

interface FieldSemantics {
  version: string
  generated_at: string
  business_name: string
  semantics: Array<{
    field_key: string
    type: string
    required: boolean
    ai_hint: string
    widget?: string
  }>
}

/* ── Generator result ──────────────────────────────────────────────── */

export interface GenerateResult {
  skillPath: string
  rulesDir: string
  toolDefsPath: string
  fingerprint: string
}

/* ── OpenAPI-style Function Tool definition ─────────────────────────── */

export interface OpenApiFunctionTool {
  type: 'function'
  function: {
    name: string
    description: string
    parameters: Record<string, unknown>
  }
}

/* ── Main generator ─────────────────────────────────────────────────── */

export class BsSkillGenerator {
  private readonly outputDir: string
  private readonly businessName: string

  constructor(config: { outputDir: string; businessName: string }) {
    this.outputDir = config.outputDir.replace(/\\/g, '/').replace(/\/$/, '')
    this.businessName = config.businessName
  }

  async generate(): Promise<GenerateResult> {
    const domain = await this.loadJson<DomainKnowledge>('domain_knowledge.json')
    const constraints = await this.loadJson<ConstraintKnowledge>('constraint_knowledge.json')
    const semantics = await this.loadJson<FieldSemantics>('field_semantics.json')

    const skillPath = await this.generateSkillMd(domain, constraints, semantics)
    const rulesDir = await this.generateGateRules(constraints)
    const toolDefsPath = await this.generateToolDefs(semantics)

    const fingerprint = this.computeFingerprint(domain, constraints, semantics)

    return { skillPath, rulesDir, toolDefsPath, fingerprint }
  }

  /* ── Private helpers ──────────────────────────────────────────────── */

  private async loadJson<T>(filename: string): Promise<T> {
    // In Electron, load via IPC or fs (depends on runtime context).
    // For a headless / test environment, we use a minimal fetch shim.
    try {
      const text = await this.readFile(this.resolvePath(filename))
      return JSON.parse(text) as T
    } catch (err) {
      throw new Error(`${JSON_PARSE_ERR_PREFIX} ${filename}: ${(err as Error).message}`)
    }
  }

  private resolvePath(filename: string): string {
    return `${this.outputDir}/agent_index/${filename}`
  }

  /* ------------------------------------------------------------------ */
  /*  STEP 4 — Generate SKILL.md                                        */
  /* ------------------------------------------------------------------ */

  private async generateSkillMd(
    domain: DomainKnowledge,
    constraints: ConstraintKnowledge,
    semantics: FieldSemantics,
  ): Promise<string> {
    const name = this.businessName
    const domainList = domain.domains.map((d) => d.name).join(', ')
    const domainCount = domain.domains.length

    const fieldRows = semantics.semantics.map((s) => {
      const required = s.required ? '是' : '否'
      return `| ${s.field_key} | ${s.type} | ${required} | ${s.ai_hint} |`
    }).join('\n')

    const gateCount = constraints.gates.length

    const lines = [
      `# BlessStar Agent Skill: ${name}`,
      '',
      `## 领域概览`,
      `项目包含 ${domainCount} 个领域（domains）：${domainList}`,
      '',
      `## 字段定义（Fields）`,
      `| 字段名 | 类型 | 必填 | 语义提示 |`,
      `|--------|------|------|----------|`,
      fieldRows,
      '',
      `## Gate 约束规则`,
      `当前 Agent 管理 ${gateCount} 条 Gate 规则。`,
      `每条规则对应一个 \`.cursor/agents/biz-${name}/rules/*.mdc\` 文件。`,
      '',
      '## Function Tool',
      'Agent 支持以下 Function Tool，可直接操作 BlessStar 配置引擎：',
      '',
      '| Tool 名称 | 用途 |',
      '|-----------|------|',
      '| `create_schema_field` | 创建 Schema 字段定义 |',
      '| `update_gate_rule` | 更新 Gate 门禁规则 |',
      '| `validate_config` | 校验配置 JSON 是否符合 Schema + Gate |',
      '| `suggest_field_type` | 根据字段标签推荐控件类型 |',
      '| `generate_normalizer_template` | 生成厂商归一化模板 |',
      '',
      '## 架构不变量',
      '',
      '| ID | 规则 |',
      '|----|------|',
      '| AGF-01 | 索引数据 100% 来自 Schema Registry + Gate 链，禁止外部非结构化数据 |',
      '| AGF-02 | Gate 链条件分支完整保留逻辑依赖，不可简化 AND/OR 组合 |',
      '| AGF-03 | Function Tool 白名单与编辑器 Tool 白名单一致 |',
      '| AGF-04 | 每次 Schema 或 Gate 变更触发自动重建 agent 索引 |',
      '| AGF-07 | 每条 Gate 规则保留"必须不"条款，禁止性规则优先于允许性规则 |',
      '',
    ].join('\n')

    const target = this.resolvePath('SKILL.md')
    await this.writeFile(target, lines)

    // Also copy a mutable copy to biz-{name}/
    const bizDir = `${this.outputDir}/biz-${name}`
    const bizSkillPath = `${bizDir}/SKILL.md`
    await this.mkdir(bizDir)
    await this.writeFile(bizSkillPath, lines)

    return bizSkillPath
  }

  /* ------------------------------------------------------------------ */
  /*  STEP 5 — Generate Gate .mdc rules                                  */
  /* ------------------------------------------------------------------ */

  private async generateGateRules(constraints: ConstraintKnowledge): Promise<string> {
    const bizDir = `${this.outputDir}/biz-${this.businessName}`
    const rulesDir = `${bizDir}/rules`
    await this.mkdir(rulesDir)

    for (const gate of constraints.gates) {
      const opSymbol = this.opToLabel(gate.op)
      const doText = gate.ai_hint
        ? `若 ${gate.field_key} ${opSymbol} ${gate.value}（场景：${gate.scenario}），${gate.ai_hint}`
        : `若 ${gate.field_key} ${opSymbol} ${gate.value}（场景：${gate.scenario}），触发 Gate 规则 ${gate.gate_id}`

      const forbidClause = this.buildForbidClause(gate)

      const mdc = `---
description: ${gate.gate_id} — ${gate.field_key} ${gate.op} ${gate.value}
globs: *.json
alwaysApply: false
---
# Gate Rule: ${gate.gate_id}

## 约束
${doText}

## 禁止（必须不）
${forbidClause}

## 场景
${gate.scenario}

## 元数据
- stable_key: ${gate.stable_key}
- layer: ${gate.layer}
- sub_category: ${gate.sub_category}
`

      const filename = `${rulesDir}/${gate.gate_id}.mdc`
      await this.writeFile(filename, mdc)
    }

    // Also create an index file for .cursor/agents/biz-{name}/rules/_index.mdc
    const indexMdc = `---
description: ${this.businessName} Gate 规则索引（${constraints.gates.length} 条）
globs: *.json
alwaysApply: false
---
# Gate 规则索引 — ${this.businessName}

本目录包含 ${constraints.gates.length} 条 Gate 规则：

${constraints.gates.map((g, i) => `${i + 1}. \`${g.gate_id}\` — ${g.field_key} ${g.op} ${g.value}`).join('\n')}

**遵循方式**：按 stable_key 幂等覆盖。每修改一条 Gate 须更新对应 .mdc 文件。
`
    await this.writeFile(`${rulesDir}/_index.mdc`, indexMdc)

    return rulesDir
  }

  /** Convert operator to readable Chinese label */
  private opToLabel(op: string): string {
    const map: Record<string, string> = {
      eq: '等于',
      ne: '不等于',
      gt: '大于',
      lt: '小于',
      gte: '大于等于',
      lte: '小于等于',
      in: '在集合内',
      range: '在范围内',
      match: '匹配模式',
    }
    return map[op] || op
  }

  /** AGF-07: Build "must-not" clause for every gate */
  private buildForbidClause(gate: ConstraintKnowledge['gates'][0]): string {
    const fk = gate.field_key
    const op = gate.op
    const val = gate.value

    // Invert the condition to get the "forbidden" state
    if (op === 'eq') return `不允许 ${fk} 等于 ${val} 时的配置`
    if (op === 'ne') return `不允许 ${fk} 不等于 ${val} 时的配置`
    if (op === 'gt') return `不允许 ${fk} 小于等于 ${val} 时的配置`
    if (op === 'lt') return `不允许 ${fk} 大于等于 ${val} 时的配置`
    if (op === 'gte') return `不允许 ${fk} 小于 ${val} 时的配置`
    if (op === 'lte') return `不允许 ${fk} 大于 ${val} 时的配置`
    if (op === 'in') return `不允许 ${fk} 不在集合 [${val}] 内的配置`
    if (op === 'range') return `不允许 ${fk} 超出范围 [${val}] 的配置`
    return `不允许违反 Gate ${gate.gate_id} 的配置`
  }

  /* ------------------------------------------------------------------ */
  /*  STEP 6 — Generate tool_defs.json (OpenAI Function Calling format)  */
  /* ------------------------------------------------------------------ */

  private async generateToolDefs(
    semantics: FieldSemantics,
  ): Promise<string> {
    // Build enum for field keys from semantics
    const fieldKeys = semantics.semantics.map((s) => s.field_key)

    const operatorEnum = ['eq', 'ne', 'gt', 'lt', 'gte', 'lte', 'in', 'range', 'match']

    const tools: OpenApiFunctionTool[] = [
      {
        type: 'function',
        function: {
          name: 'create_schema_field',
          description: '在 Schema 配置中创建一个新的字段定义',
          parameters: {
            type: 'object',
            properties: {
              key: {
                type: 'string',
                description: '字段标识符（字母数字下划线）',
                ...(fieldKeys.length > 0 ? { examples: fieldKeys.slice(0, 3) } : {}),
              },
              widget: {
                type: 'string',
                description: '控件类型',
                enum: ['input', 'select', 'checkbox', 'radio', 'number', 'textarea', 'group', 'repeatable'],
              },
              label: { type: 'string', description: '界面显示标签' },
              required: { type: 'boolean', description: '是否为必填字段', default: false },
              placeholder: { type: 'string', description: '输入提示文本' },
              description: { type: 'string', description: '字段描述说明' },
              default_value: { type: 'string', description: '默认值' },
            },
            required: ['key', 'widget', 'label'],
          },
        },
      },
      {
        type: 'function',
        function: {
          name: 'update_gate_rule',
          description: '更新或添加 Gate 门禁规则',
          parameters: {
            type: 'object',
            properties: {
              gate_id: { type: 'string', description: 'Gate 标识符' },
              scenario: { type: 'string', description: '场景名称，如 production' },
              action: {
                type: 'string',
                enum: ['add_rule', 'update_rule', 'remove_rule'],
                description: '操作类型',
              },
              field: {
                type: 'string',
                description: '目标字段名',
                ...(fieldKeys.length > 0 ? { enum: fieldKeys } : {}),
              },
              operator: {
                type: 'string',
                enum: operatorEnum,
                description: '比较运算符',
              },
              value: { type: 'string', description: '比较值' },
            },
            required: ['gate_id', 'scenario', 'action'],
          },
        },
      },
      {
        type: 'function',
        function: {
          name: 'validate_config',
          description: '校验配置文件 JSON 是否符合 BlessStar Schema 和 Gate 规则',
          parameters: {
            type: 'object',
            properties: {
              config_json: { type: 'string', description: '待校验的配置 JSON 字符串' },
              mode: {
                type: 'string',
                enum: ['schema', 'gate', 'all'],
                description: '校验模式',
                default: 'schema',
              },
            },
            required: ['config_json'],
          },
        },
      },
      {
        type: 'function',
        function: {
          name: 'suggest_field_type',
          description: '根据字段标签或名称推荐最合适的控件类型',
          parameters: {
            type: 'object',
            properties: {
              label: { type: 'string', description: '字段标签或名称文本' },
              context: { type: 'string', description: '额外上下文说明（可选）' },
            },
            required: ['label'],
          },
        },
      },
      {
        type: 'function',
        function: {
          name: 'generate_normalizer_template',
          description: '生成厂商/业务配置归一化器模板，将厂商特有格式映射到统一 Schema',
          parameters: {
            type: 'object',
            properties: {
              vendor_name: { type: 'string', description: '厂商或业务源名称，如 yonyou、kingdee' },
              version: { type: 'string', description: '模板版本号，默认 1.0' },
              field_count: { type: 'number', description: '预期的字段数量', default: 3 },
            },
            required: ['vendor_name'],
          },
        },
      },
    ]

    const content = JSON.stringify({ tools }, null, 2)
    const path = `${this.outputDir}/biz-${this.businessName}/tool_defs.json`
    await this.writeFile(path, content)
    return path
  }

  /* ── Fingerprint computation (AGF-04: detect changes) ────────────── */

  private computeFingerprint(
    domain: DomainKnowledge,
    constraints: ConstraintKnowledge,
    semantics: FieldSemantics,
  ): string {
    const raw = [
      domain.version,
      domain.generated_at,
      domain.domains.map((d) => `${d.name}:${d.field_count}`).join('|'),
      constraints.version,
      constraints.gates.map((g) => `${g.stable_key}:${g.value}`).join('|'),
      semantics.version,
      semantics.semantics.map((s) => `${s.field_key}:${s.type}:${s.required}`).join('|'),
    ].join('||')

    // Simple hash: sum of char codes (deterministic, sufficient for change detection)
    let hash = 0
    for (let i = 0; i < raw.length; i++) {
      const chr = raw.charCodeAt(i)
      hash = ((hash << 5) - hash) + chr
      hash |= 0 // Convert to 32-bit int
    }
    return `v1-fp-${Math.abs(hash).toString(16).padStart(8, '0')}`
  }

  /* ── File I/O (abstracted for testability & environment) ──────────── */

  /** Read text from a file path. Override in tests. */
  private async readFile(_path: string): Promise<string> {
    // In Electron, this goes through IPC via window.blessstar IPC channels.
    // For headless / test, we supply a mock via override.
    throw new Error('readFile must be overridden — use BsSkillGenerator with withFileIo() or mock')
  }

  /** Write text to a file path. Override in tests. */
  private async writeFile(_path: string, _content: string): Promise<void> {
    throw new Error('writeFile must be overridden — use BsSkillGenerator with withFileIo() or mock')
  }

  /** Create directory recursively. Override in tests. */
  private async mkdir(_path: string): Promise<void> {
    throw new Error('mkdir must be overridden — use BsSkillGenerator with withFileIo() or mock')
  }
}

/* ── File IO adapter for Electron main process (Node.js fs) ────────── */

let _fsInstance: typeof import('fs') | null = null
let _pathInstance: typeof import('path') | null = null

/**
 * Call once to inject Node.js fs/path modules for Electron main process usage.
 */
export function injectNodeFs(fs: typeof import('fs'), pathMod: typeof import('path')): void {
  _fsInstance = fs
  _pathInstance = pathMod
}

/**
 * Create a generator wired with real Node.js fs.
 * Requires injectNodeFs() called first.
 */
export function createBsSkillGenerator(config: { outputDir: string; businessName: string }): BsSkillGenerator {
  if (!_fsInstance) throw new Error('injectNodeFs() must be called first')
  return createWithFs(config, _fsInstance, _pathInstance!)
}

/** Internal: wire a generator with real fs/path. Exported for advanced use. */
export function createWithFs(
  config: { outputDir: string; businessName: string },
  fs: typeof import('fs'),
  pathMod: typeof import('path'),
): BsSkillGenerator {
  const gen = new BsSkillGenerator(config)
  // Override private methods with fs-backed implementations
  ;(gen as any).readFile = async (p: string) => fs.readFileSync(p, 'utf-8')
  ;(gen as any).writeFile = async (p: string, c: string) => {
    const dir = pathMod.dirname(p)
    if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true })
    fs.writeFileSync(p, c, 'utf-8')
  }
  ;(gen as any).mkdir = async (p: string) => {
    if (!fs.existsSync(p)) fs.mkdirSync(p, { recursive: true })
  }
  return gen
}
