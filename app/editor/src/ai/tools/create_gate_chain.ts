/**
 * create_gate_chain.ts — AI Tool：从 AST JSON 编译 Gate 链
 *
 * 接收 LLM 输出的结构化 AST JSON（And/Or/Then/Condition 树），
 * 校验 AST 结构，编译为 gate_chain 指针 DAG。
 *
 * 架构方案：② Gate 迷你 AST + ③ create_gate_chain AI tool
 * 见：架构方案选择记录（第25天以后）.md § 第32天/专题二
 */

import type { FunctionTool, ToolResult } from '../types'

// ── Gate Factory 桥接（sub_category 推断 + stable_key 生成）──────
import { inferSubCategory } from '../GateFactoryBridge'
import type { GateRuleDef } from '../GateFactoryBridge'

/**
 * Gate AST 节点类型
 * LLM 输出格式为结构化 JSON 树
 */
export type GateASTNode =
  | { type: 'condition'; field: string; op: string; value: string }
  | { type: 'and'; left: GateASTNode; right: GateASTNode }
  | { type: 'or'; left: GateASTNode; right: GateASTNode }
  | { type: 'not'; node: GateASTNode }
  | { type: 'then'; when: GateASTNode; do: GateASTNode }
  | { type: 'action'; name: string; value: string }

/** 编译后的扁平时序链节点（用于 UI 展示） */
export interface CompiledGateStep {
  type: string
  description: string
  field?: string
  op?: string
  value?: string
}

/** 编译结果 */
export interface CompiledGateChain {
  steps: CompiledGateStep[]
  astJson: string
}

/**
 * validateAST — 校验 AST JSON 结构合法性
 */
function validateAST(ast: unknown): { valid: boolean; error?: string } {
  if (!ast || typeof ast !== 'object') return { valid: false, error: 'AST 必须是非空对象' }

  const node = ast as Record<string, unknown>
  if (!node.type || typeof node.type !== 'string') return { valid: false, error: 'AST 节点缺少 type 字段' }

  switch (node.type) {
  case 'condition':
    if (!node.field || typeof node.field !== 'string') return { valid: false, error: 'condition 节点缺少 field' }
    if (!node.op || typeof node.op !== 'string') return { valid: false, error: 'condition 节点缺少 op' }
    if (node.value === undefined || node.value === null) return { valid: false, error: 'condition 节点缺少 value' }
    return { valid: true }

  case 'and':
  case 'or':
    if (!node.left || !node.right) return { valid: false, error: `${node.type} 节点缺少 left 或 right` }
    {
      const l = validateAST(node.left); if (!l.valid) return l
      const r = validateAST(node.right); if (!r.valid) return r
    }
    return { valid: true }

  case 'not':
    if (!node.node) return { valid: false, error: 'not 节点缺少 node' }
    return validateAST(node.node)

  case 'then':
    if (!node.when) return { valid: false, error: 'then 节点缺少 when' }
    if (!node.do) return { valid: false, error: 'then 节点缺少 do' }
    {
      const w = validateAST(node.when); if (!w.valid) return w
      const d = validateAST(node.do); if (!d.valid) return d
    }
    return { valid: true }

  case 'action':
    if (!node.name || typeof node.name !== 'string') return { valid: false, error: 'action 节点缺少 name' }
    return { valid: true }

  default:
    return { valid: false, error: `未知 AST 节点类型: ${node.type}` }
  }
}

/**
 * compileAST — 将 AST JSON 编译为扁平时序步骤
 */
function compileAST(ast: GateASTNode, steps: CompiledGateStep[] = []): CompiledGateStep[] {
  switch (ast.type) {
  case 'condition':
    steps.push({
      type: 'condition',
      description: `字段 ${ast.field} ${ast.op} ${ast.value}`,
      field: ast.field,
      op: ast.op,
      value: ast.value,
    })
    break
  case 'and':
    steps.push({ type: 'logic_and', description: 'AND 开始 ── 全部条件需满足' })
    compileAST(ast.left, steps)
    compileAST(ast.right, steps)
    steps.push({ type: 'logic_and', description: 'AND 结束' })
    break
  case 'or':
    steps.push({ type: 'logic_or', description: 'OR 开始 ── 任一条件满足即可' })
    compileAST(ast.left, steps)
    compileAST(ast.right, steps)
    steps.push({ type: 'logic_or', description: 'OR 结束' })
    break
  case 'not':
    steps.push({ type: 'logic_not', description: 'NOT 取反' })
    compileAST(ast.node, steps)
    break
  case 'then':
    compileAST(ast.when, steps)
    steps.push({ type: 'do_branch', description: '→ 条件满足时执行:' })
    compileAST(ast.do, steps)
    break
  case 'action':
    steps.push({
      type: 'action',
      description: `执行操作: ${ast.name} = ${ast.value}`,
      field: ast.name,
      value: ast.value,
    })
    break
  }
  return steps
}

export const createGateChainTool: FunctionTool = {
  definition: {
    name: 'create_gate_chain',
    description: '创建或更新 Gate 门禁链。接收结构化 AST JSON（And/Or/Then/Condition 树），编译为 Gate 链注册到系统。适用于复杂的多条件嵌套门禁规则，比如"金额>10000 且 部门=财务 → 总监审批"。',
    parameters: {
      type: 'object',
      properties: {
        ast_json: {
          type: 'string',
          description: 'Gate AST JSON 字符串，支持 condition/and/or/not/then/action 六种节点类型',
        },
        gate_id: {
          type: 'string',
          description: 'Gate 标识符，如 "amount_approval_gate"；删除时必填',
        },
        scenario: {
          type: 'string',
          description: '场景名称，如 "production"、"development"',
        },
        action: {
          type: 'string',
          description: '操作类型：add_rule（默认）/ update_rule / remove_rule',
          enum: ['add_rule', 'update_rule', 'remove_rule'],
        },
      },
      required: ['gate_id'],
    },
  },

  resultRenderer(data: unknown): string[] {
    const d = data as Record<string, unknown> | undefined
    if (!d) return ['❌ 无数据']
    if (d.deleted) {
      const lines: string[] = [`🗑️ 已删除 Gate 链（${d.gate_id || ''}）`]
      return lines
    }
    const compiled = d.compiled as Record<string, unknown> | undefined
    const steps = compiled?.steps as Array<{ type: string; description: string }> | undefined
    const lines: string[] = [`✅ 已编译 Gate 链（${d.gate_id || ''}）`]
    if (steps && steps.length > 0) {
      lines.push(`步骤（${steps.length} 步）:`)
      steps.forEach((s, i) => {
        lines.push(`  ${i + 1}. ${s.description}`)
      })
    }
    return lines
  },

  async execute(args: Record<string, unknown>): Promise<ToolResult> {
    const gateId = String(args.gate_id || '')
    const scenario = String(args.scenario || 'production')
    const action = String(args.action || 'add_rule')

    if (!gateId) {
      return { success: false, error: '缺少 gate_id 参数' }
    }

    // ── remove_rule：直接删除，不需要 AST ──
    if (action === 'remove_rule') {
      try {
        const result = await window.blessstar.registerGate('custom', JSON.stringify({
          type: 'custom',
          gate_id: gateId,
          action: 'remove_rule',
        }))
        if (!result.success) {
          return { success: false, error: `Gate 链删除失败: ${result.error || '未知错误'}` }
        }
        return { success: true, data: { gate_id: gateId, action: 'remove_rule', deleted: true } }
      } catch (e) {
        return { success: false, error: `Gate 链删除异常: ${(e as Error).message}` }
      }
    }

    const astJson = String(args.ast_json || '')
    if (!astJson) {
      return { success: false, error: '缺少 ast_json 参数' }
    }

    /* 解析 AST JSON */
    let ast: GateASTNode
    try {
      ast = JSON.parse(astJson) as GateASTNode
    } catch {
      return { success: false, error: `AST JSON 语法解析失败: ${astJson.slice(0, 100)}` }
    }

    /* 校验 AST 结构 */
    const validation = validateAST(ast)
    if (!validation.valid) {
      return { success: false, error: `AST 校验失败: ${validation.error}` }
    }

    /* 编译为时序步骤 */
    const steps = compileAST(ast)

    /* ── Gate Factory 集成：推断 sub_category + 生成 stable_key ── */
    const firstCondition = steps.find((s) => s.type === 'condition')
    const ruleDef: GateRuleDef = {
      field_key: firstCondition?.field || '',
      field_type: 'string',
      op: firstCondition?.op || 'eq',
      value: firstCondition?.value || '',
      ai_hint: gateId,
    }
    const factorySubCategory = inferSubCategory(ruleDef)
    const stableKey = `${scenario}:${gateId}:${factorySubCategory}`

    const compiled: CompiledGateChain = {
      steps,
      astJson: JSON.stringify(ast, null, 2),
    }

    // ── 第38天 · DAY38-02：通过 executeTool IPC 调用原生 gate_factory ──
    try {
      const resultJson = await (window as any).blessstar.executeTool({
        tool: 'create_gate_chain',
        args: {
          factory_type: 'custom',
          field_key: firstCondition?.field || '',
          field_type: firstCondition?.op || 'string',
          op: firstCondition?.op || 'eq',
          value: firstCondition?.value || '',
          scenario,
          layer: 2,
          ai_hint: gateId,
        },
      })

      const nativeResult = typeof resultJson === 'string' ? JSON.parse(resultJson) : resultJson
      if (!nativeResult || !nativeResult.success) {
        return { success: false, error: `原生 Gate factory 失败: ${nativeResult?.error || '未知错误'}` }
      }

      // 同时注册到 Gate Map
      await (window as any).blessstar.executeTool({
        tool: 'gate_map_upsert',
        args: {
          stable_key: stableKey,
          node_json: JSON.stringify({
            type: 'bs_custom_gate',
            id: gateId,
            field_key: firstCondition?.field || '',
            op: firstCondition?.op || 'eq',
            value: firstCondition?.value || '',
            layer: 2,
            sub_category: factorySubCategory,
          }),
        },
      })
    } catch (e) {
      return { success: false, error: `原生 Gate factory 异常: ${(e as Error).message}` }
    }

    return {
      success: true,
      data: {
        compiled,
        gate_id: gateId,
        scenario,
        sub_category: factorySubCategory,
        stable_key: stableKey,
        summary: steps.map((s) => s.description).join(' → '),
      },
    }
  },
}
