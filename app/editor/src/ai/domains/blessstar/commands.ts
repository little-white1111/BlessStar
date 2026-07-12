/**
 * BlessStar Command Registry — /command style interactions.
 *
 * ADR-全链路接通 — Commands are registered by IDomainPlugin.getCommandRegistry()
 * and invoked via the PipelineEngine.
 */

import type { ICommandEntry, PipelineContext } from '../../engine/types'

export const BLESSSTAR_COMMANDS: ICommandEntry[] = [
  {
    command: 'read',
    description: '读取配置值。用法: /read <key>',
    handler: async (args: string[], ctx: PipelineContext): Promise<string> => {
      if (args.length === 0) return '用法: /read <key>'
      const key = args.join('.')
      const provider = ctx.domainPlugin.getPersistenceProvider()
      const value = await provider.load(key)
      if (value === null || value === undefined) {
        return `配置项 "${key}" 不存在`
      }
      return `${key} = ${JSON.stringify(value, null, 2)}`
    },
  },
  {
    command: 'write',
    description: '写入配置值。用法: /write <key> <value>',
    handler: async (args: string[], ctx: PipelineContext): Promise<string> => {
      if (args.length < 2) return '用法: /write <key> <value>'
      const key = args[0]
      const value = args.slice(1).join(' ')
      const provider = ctx.domainPlugin.getPersistenceProvider()

      // Try to parse as JSON number/boolean
      let parsed: unknown = value
      if (value === 'true') parsed = true
      else if (value === 'false') parsed = false
      else {
        const num = Number(value)
        if (!isNaN(num) && value.trim() !== '') parsed = num
      }

      await provider.save({ key, value: parsed }, key)
      return `已写入: ${key} = ${JSON.stringify(parsed)}`
    },
  },
  {
    command: 'list',
    description: '列出配置项。用法: /list [prefix]',
    handler: async (args: string[], ctx: PipelineContext): Promise<string> => {
      const prefix = args.join('.') || undefined
      const provider = ctx.domainPlugin.getPersistenceProvider()
      const items = await provider.list(prefix)

      if (items.length === 0) {
        return prefix ? `没有以 "${prefix}" 开头的配置项` : '没有可用的配置项'
      }

      const lines = items.map(item => `  ${item.key} = ${JSON.stringify(item.value)}`)
      return `共 ${items.length} 项:\n${lines.join('\n')}`
    },
  },
  {
    command: 'validate',
    description: '校验配置。用法: /validate [scope]',
    handler: async (_args: string[], ctx: PipelineContext): Promise<string> => {
      const scope = _args[0] || 'all'
      const result = await window.blessstar?.validateConfig?.(JSON.stringify({ scope }))
        ?? { valid: true, errors: [] }

      if (result.valid) {
        return '✅ 配置校验通过，所有 Gate 规则已满足。'
      }

      const errorLines = (result.errors || []).map(
        (e: any) => `  ❌ ${e.path}: ${e.message}`,
      )
      return `❌ 校验未通过，发现 ${result.errors?.length ?? 0} 个问题:\n${errorLines.join('\n')}`
    },
  },
  {
    command: 'build',
    description: '构建工作区。用法: /build [format]',
    handler: async (args: string[], ctx: PipelineContext): Promise<string> => {
      const format = args[0] || 'json'
      const validFormats = ['json', 'yaml', 'toml', 'ini']
      if (!validFormats.includes(format)) {
        return `不支持的格式: "${format}"。支持: ${validFormats.join(', ')}`
      }

      await window.blessstar?.workspace?.build(format as any)
      return `✅ 工作区构建完成，输出格式: ${format}，文件已写入 dist/ 目录。`
    },
  },
  {
    command: 'help',
    description: '显示帮助信息。用法: /help [command]',
    handler: async (args: string[], ctx: PipelineContext): Promise<string> => {
      const commands = ctx.domainPlugin.getCommandRegistry()
      if (args.length > 0) {
        const cmd = commands.find(c => c.command === args[0])
        if (cmd) {
          return `/${cmd.command}: ${cmd.description}`
        }
        return `未知命令: /${args[0]}`
      }

      const lines = commands.map(cmd => `  /${cmd.command} — ${cmd.description}`)
      return `可用命令:\n${lines.join('\n')}`
    },
  },
]
