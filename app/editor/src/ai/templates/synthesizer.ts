/**
 * templates/synthesizer — 确定性模板合成器
 *
 * 专题六：汇报分支出路（PIPELINE-14）— is_chat=false 时用纯确定性模板拼接。
 * 零 LLM，直接用 planStep + toolResults 构造简洁成果汇报。
 *
 * 原位置：AIPanel.tsx synthesizeTemplateReply()
 */

/** 单条操作结果描述 */
export interface SynthesisInput {
  subject: string
  operation: string
  success: boolean
  detail: string
}

/**
 * 确定性模板合成 — 纯操作结果的简洁汇报。
 * 零 LLM，不解复用 emoji。
 */
export function synthesizeTemplateReply(results: SynthesisInput[]): string {
  const successAll = results.every(r => r.success)
  const summary = successAll ? '已完成。' : '部分操作未完成。'
  const details = results.map(r => {
    const status = r.success ? '✅' : '❌'
    return `${status} ${r.subject}: ${r.detail}`
  }).join('\n')
  return `${summary}\n${details}`
}
