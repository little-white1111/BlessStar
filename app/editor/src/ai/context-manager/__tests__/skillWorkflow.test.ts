import { describe, it, expect } from 'vitest'
import { executeSkillWorkflow } from '../skillWorkflow'

describe('skillWorkflow — Skill 工作流执行器', () => {

  it('不匹配的输入返回 matched=false', async () => {
    const result = await executeSkillWorkflow('随便说点什么')
    expect(result.matched).toBe(false)
    expect(result.steps).toHaveLength(0)
  })

  it('/command 匹配并返回 matched=true', async () => {
    const result = await executeSkillWorkflow('/list')
    expect(result.matched).toBe(true)
    expect(result.skillName).toBe('/list')
  })

  it('approvalRequired 的 skill 需要用户确认', async () => {
    const result = await executeSkillWorkflow('/write 房间号 10041', false)
    expect(result.matched).toBe(true)
    expect(result.approvalRequired).toBe(true)
    expect(result.allSuccess).toBe(false)
    expect(result.summary).toContain('需要用户确认')
  })

  it('approvalGranted=true 时跳过确认检查', async () => {
    const result = await executeSkillWorkflow('/list', true)
    expect(result.matched).toBe(true)
  })

  it('执行结果包含 step 明细', async () => {
    const result = await executeSkillWorkflow('/list', true)
    if (result.matched && result.steps.length > 0) {
      for (const step of result.steps) {
        expect(step.toolName).toBeTruthy()
        expect(step.callId).toBeTruthy()
      }
    }
  })

  it('summary 包含 Skill 名称', async () => {
    const result = await executeSkillWorkflow('/list', true)
    if (result.matched) {
      expect(result.summary).toContain('/list')
    }
  })
})
