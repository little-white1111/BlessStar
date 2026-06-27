import { describe, it, expect } from 'vitest'
import { matchSkill, getAllSkills, getSkillHelpText, parseCommand } from '../skillRouter'

describe('skillRouter — L0 Skill 路由匹配', () => {
  // ── matchSkill（旧 SKILL_ROUTES，已清空仅占位）──
  it('matchSkill 返回 false（SKILL_ROUTES 已清空）', () => {
    const result = matchSkill('/createconfig 帮我添加房间号')
    expect(result.matched).toBe(false)
  })

  it('无 /command 前缀不命中', () => {
    const result = matchSkill('帮我添加一个房间号配置')
    expect(result.matched).toBe(false)
  })

  // ── parseCommand（新 UNIFIED_SKILLS）──
  it('/list 匹配 UNIFIED_SKILLS', () => {
    const result = parseCommand('/list')
    expect(result.matched).toBe(true)
    expect(result.command).toBe('list')
    expect(result.intent).toBe('QUERY_LIST')
  })

  it('/list 别名 /ls', () => {
    const result = parseCommand('/ls')
    expect(result.matched).toBe(true)
    expect(result.command).toBe('ls')
    expect(result.intent).toBe('QUERY_LIST')
  })

  it('/list 房间号 提取 rest 参数', () => {
    const result = parseCommand('/list 房间号')
    expect(result.matched).toBe(true)
    expect(result.rest).toBe('房间号')
  })

  it('/createconfig 匹配 UNIFIED_SKILLS', () => {
    const result = parseCommand('/createconfig')
    expect(result.matched).toBe(true)
    expect(result.command).toBe('createconfig')
    expect(result.intent).toBe('ACTION')
  })

  it('/createrule 匹配 UNIFIED_SKILLS', () => {
    const result = parseCommand('/createrule')
    expect(result.matched).toBe(true)
    expect(result.command).toBe('createrule')
    expect(result.intent).toBe('ACTION')
  })

  it('/write 房间号 10041 提取参数和值', () => {
    const result = parseCommand('/write 房间号 10041')
    expect(result.matched).toBe(true)
    expect(result.rest).toBe('房间号')
    expect(result.value).toBe('10041')
  })

  it('/read 房间号 提取 QUERY_SINGLE', () => {
    const result = parseCommand('/read 房间号')
    expect(result.matched).toBe(true)
    expect(result.intent).toBe('QUERY_SINGLE')
    expect(result.rest).toBe('房间号')
  })

  it('未知命令 /xxx 不命中', () => {
    const result = parseCommand('/xxx')
    expect(result.matched).toBe(false)
  })

  // ── getAllSkills（旧路由表已清空）──
  it('getAllSkills 返回空数组（SKILL_ROUTES 已占位）', () => {
    const skills = getAllSkills()
    expect(skills.length).toBe(0)
  })

  // ── getSkillHelpText ──
  it('getSkillHelpText 返回空字符串', () => {
    const help = getSkillHelpText()
    expect(help).toBe('')
  })
})
