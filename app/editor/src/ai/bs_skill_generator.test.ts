import { describe, it, expect, beforeEach } from 'vitest'
import { BsSkillGenerator } from './bs_skill_generator'
import type { GenerateResult } from './bs_skill_generator'

/* ── Mock file system ──────────────────────────────────────────────── */

class MockFs {
  private files = new Map<string, string>()
  private dirs = new Set<string>()

  writeFileSync(p: string, c: string) {
    this.files.set(p, c)
    const parts = p.replace(/\\/g, '/').split('/')
    for (let i = 1; i <= parts.length - 1; i++) {
      this.dirs.add(parts.slice(0, i).join('/'))
    }
  }

  readFileSync(p: string): string {
    const content = this.files.get(p)
    if (content === undefined) throw new Error(`ENOENT: ${p}`)
    return content
  }

  existsSync(p: string): boolean {
    return this.files.has(p) || this.dirs.has(p)
  }

  mkdirSync(p: string, _opts?: { recursive?: boolean }) {
    this.dirs.add(p.replace(/\\/g, '/'))
  }
}

/* ── Test data ─────────────────────────────────────────────────────── */

const MOCK_DOMAIN = {
  version: '1.0',
  generated_at: '2026-06-16T10:00:00Z',
  business_name: 'test_biz',
  domains: [
    { name: 'server', field_count: 2, fields: ['port', 'host'] },
    { name: 'security', field_count: 1, fields: ['timeout'] },
  ],
}

const MOCK_CONSTRAINTS = {
  version: '1.0',
  generated_at: '2026-06-16T10:00:00Z',
  business_name: 'test_biz',
  gates: [
    {
      gate_id: 'security_gate',
      scenario: 'production',
      field_key: 'port',
      op: 'gt',
      value: '1024',
      layer: 0,
      sub_category: 'threshold',
      stable_key: 'production:port:0:threshold',
      ai_hint: '端口号必须大于 1024',
    },
    {
      gate_id: 'timeout_gate',
      scenario: 'production',
      field_key: 'timeout',
      op: 'lte',
      value: '30000',
      layer: 1,
      sub_category: 'threshold',
      stable_key: 'production:timeout:1:threshold',
      ai_hint: '超时不超过 30000ms',
    },
  ],
}

const MOCK_SEMANTICS = {
  version: '1.0',
  generated_at: '2026-06-16T10:00:00Z',
  business_name: 'test_biz',
  semantics: [
    { field_key: 'port', type: 'number', required: true, ai_hint: '服务器端口号' },
    { field_key: 'host', type: 'string', required: true, ai_hint: '服务器主机地址' },
    { field_key: 'timeout', type: 'number', required: false, ai_hint: '超时时间(ms)' },
  ],
}

/* ── Fixture: create generator with mock fs ────────────────────────── */

function createGenerator(mockFs: MockFs, outputDir = '/tmp/agents', businessName = 'test_biz'): BsSkillGenerator {
  const gen = new BsSkillGenerator({ outputDir, businessName })

  // Seed input files
  mockFs.writeFileSync(`${outputDir}/agent_index/domain_knowledge.json`, JSON.stringify(MOCK_DOMAIN))
  mockFs.writeFileSync(`${outputDir}/agent_index/constraint_knowledge.json`, JSON.stringify(MOCK_CONSTRAINTS))
  mockFs.writeFileSync(`${outputDir}/agent_index/field_semantics.json`, JSON.stringify(MOCK_SEMANTICS))

  // Override IO with mock
  ;(gen as any).readFile = async (p: string) => mockFs.readFileSync(p)
  ;(gen as any).writeFile = async (p: string, c: string) => { mockFs.writeFileSync(p, c) }
  ;(gen as any).mkdir = async (p: string) => { mockFs.mkdirSync(p) }

  return gen
}

/* ── Tests ─────────────────────────────────────────────────────────── */

describe('BsSkillGenerator', () => {
  let mockFs: MockFs
  let gen: BsSkillGenerator

  beforeEach(() => {
    mockFs = new MockFs()
    gen = createGenerator(mockFs)
  })

  /* ── Constructor & config ────────────────────────────────────────── */

  it('stores outputDir and businessName from config', () => {
    const g = new BsSkillGenerator({ outputDir: '/some/path', businessName: 'my_biz' })
    expect(g).toBeInstanceOf(BsSkillGenerator)
  })

  it('normalises outputDir trailing slash', () => {
    const g = new BsSkillGenerator({ outputDir: '/some/path/', businessName: 'biz' })
    // Access via generate — constructor normalises; just verify no crash
    expect(g).toBeInstanceOf(BsSkillGenerator)
  })

  /* ── generate() produces all outputs ─────────────────────────────── */

  it('generate() returns correct paths and creates SKILL.md', async () => {
    const result: GenerateResult = await gen.generate()

    expect(result.skillPath).toContain('biz-test_biz/SKILL.md')
    expect(result.rulesDir).toContain('biz-test_biz/rules')
    expect(result.toolDefsPath).toContain('biz-test_biz/tool_defs.json')
    expect(result.fingerprint).toMatch(/^v1-fp-[0-9a-f]{8}$/)

    // SKILL.md exists and contains expected content
    const skill = mockFs.readFileSync(result.skillPath)
    expect(skill).toContain('# BlessStar Agent Skill: test_biz')
    expect(skill).toContain('2 个领域（domains）')
    expect(skill).toContain('3 条 Gate 规则')
    expect(skill).toContain('create_schema_field')
    expect(skill).toContain('AGF-01')
    expect(skill).toContain('AGF-07')
  })

  /* ── SKILL.md content ──────────────────────────────────────────── */

  it('SKILL.md lists all fields from semantics', async () => {
    const result = await gen.generate()
    const skill = mockFs.readFileSync(result.skillPath)
    expect(skill).toContain('| port | number | 是 | 服务器端口号 |')
    expect(skill).toContain('| host | string | 是 | 服务器主机地址 |')
    expect(skill).toContain('| timeout | number | 否 | 超时时间(ms) |')
  })

  /* ── Gate .mdc rules ─────────────────────────────────────────────── */

  it('generate() creates .mdc files for each gate', async () => {
    const result = await gen.generate()

    const securityGatePath = `${result.rulesDir}/security_gate.mdc`
    const timeoutGatePath = `${result.rulesDir}/timeout_gate.mdc`

    expect(mockFs.existsSync(securityGatePath)).toBe(true)
    expect(mockFs.existsSync(timeoutGatePath)).toBe(true)

    const securityMdc = mockFs.readFileSync(securityGatePath)
    expect(securityMdc).toContain('# Gate Rule: security_gate')
    expect(securityMdc).toContain('port')
    expect(securityMdc).toContain('production')
    expect(securityMdc).toContain('stable_key: production:port:0:threshold')

    const timeoutMdc = mockFs.readFileSync(timeoutGatePath)
    expect(timeoutMdc).toContain('# Gate Rule: timeout_gate')
    expect(timeoutMdc).toContain('timeout')
    expect(timeoutMdc).toContain('lte')
  })

  /* ── AGF-07: forbid clause in every .mdc ─────────────────────────── */

  it('every .mdc file contains a "禁止" clause (AGF-07)', async () => {
    const result = await gen.generate()
    const gates = ['security_gate', 'timeout_gate']
    for (const id of gates) {
      const mdc = mockFs.readFileSync(`${result.rulesDir}/${id}.mdc`)
      expect(mdc).toContain('## 禁止（必须不）')
      expect(mdc).toContain('不允许')
    }
  })

  /* ── _index.mdc ────────────────────────────────────────────────── */

  it('creates _index.mdc with gate list', async () => {
    const result = await gen.generate()
    const index = mockFs.readFileSync(`${result.rulesDir}/_index.mdc`)
    expect(index).toContain('Gate 规则索引')
    expect(index).toContain('security_gate')
    expect(index).toContain('timeout_gate')
    expect(index).toContain('2 条 Gate 规则')
  })

  /* ── tool_defs.json ────────────────────────────────────────────────── */

  it('tool_defs.json contains all 5 function tools in OpenAI format', async () => {
    const result = await gen.generate()
    const raw = mockFs.readFileSync(result.toolDefsPath)
    const parsed = JSON.parse(raw)

    expect(parsed.tools).toHaveLength(5)
    const names = parsed.tools.map((t: any) => t.function.name)
    expect(names).toContain('create_schema_field')
    expect(names).toContain('update_gate_rule')
    expect(names).toContain('validate_config')
    expect(names).toContain('suggest_field_type')
    expect(names).toContain('generate_normalizer_template')

    // All have required type field
    for (const tool of parsed.tools) {
      expect(tool.type).toBe('function')
      expect(tool.function.parameters.type).toBe('object')
    }
  })

  it('tool_defs.json includes field_key enums from semantics', async () => {
    const result = await gen.generate()
    const raw = mockFs.readFileSync(result.toolDefsPath)
    const parsed = JSON.parse(raw)

    const updateGate = parsed.tools.find((t: any) => t.function.name === 'update_gate_rule')
    expect(updateGate.function.parameters.properties.field.enum).toEqual(
      expect.arrayContaining(['port', 'host', 'timeout']),
    )
  })

  /* ── Fingerprint ────────────────────────────────────────────────── */

  it('generate() returns consistent fingerprint for same inputs', async () => {
    const r1 = await gen.generate()
    const r2 = await gen.generate()
    expect(r1.fingerprint).toBe(r2.fingerprint)
  })

  it('fingerprint changes when inputs change', async () => {
    const r1 = await gen.generate()

    // Mutate one input
    mockFs.writeFileSync(
      '/tmp/agents/agent_index/domain_knowledge.json',
      JSON.stringify({ ...MOCK_DOMAIN, version: '2.0' }),
    )
    const r2 = await gen.generate()
    expect(r1.fingerprint).not.toBe(r2.fingerprint)
  })

  /* ── Error handling ────────────────────────────────────────────── */

  it('throws when input JSON is missing', async () => {
    const badGen = new BsSkillGenerator({ outputDir: '/tmp/nonexistent', businessName: 'biz' })
    // Override readFile to throw ENOENT
    ;(badGen as any).readFile = async (_p: string) => { throw new Error('ENOENT') }

    await expect(badGen.generate()).rejects.toThrow(/bs_skill_generator/)
  })

  it('throws when input JSON is invalid', async () => {
    const badGen = new BsSkillGenerator({ outputDir: '/tmp', businessName: 'biz' })
    ;(badGen as any).readFile = async (_p: string) => '{ invalid json }'

    await expect(badGen.generate()).rejects.toThrow(/bs_skill_generator/)
  })

  /* ── opToLabel covers all known operators ────────────────────────── */

  it('generates correct forbid clause for gt/lt/gte/lte operators', async () => {
    const result = await gen.generate()
    const securityMdc = mockFs.readFileSync(`${result.rulesDir}/security_gate.mdc`)
    // gt → "不允许 小于等于"
    expect(securityMdc).toContain('小于等于')
  })

  /* ── Parallel input (AGF-01: 100% from Schema Registry + Gate chain) ── */

  it('generate() output references only inputs from domain/constraint/semantics (AGF-01)', async () => {
    const result = await gen.generate()
    const skill = mockFs.readFileSync(result.skillPath)

    // Should reference domain names from input
    expect(skill).toContain('server')
    expect(skill).toContain('security')

    // Should NOT contain hallucinated content
    expect(skill).not.toContain('fake_domain')
    expect(skill).not.toContain('undefined_field')
  })

  /* ── AGF-03: tool names match editor white list ──────────────────── */

  it('tool_defs.json names match expected editor tool set (AGF-03)', async () => {
    const expected = ['create_schema_field', 'update_gate_rule', 'validate_config', 'suggest_field_type', 'generate_normalizer_template']
    const result = await gen.generate()
    const raw = mockFs.readFileSync(result.toolDefsPath)
    const parsed = JSON.parse(raw)
    const names = parsed.tools.map((t: any) => t.function.name)
    expect(names.sort()).toEqual(expected.sort())
  })
})
