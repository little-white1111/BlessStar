#!/usr/bin/env node

/**
 * BlessStar AI CLI entry point
 *
 * Usage:
 *   npx ts-node tools/ai/cli.ts generate "create a schema field for server_port"
 *   npx ts-node tools/ai/cli.ts generate --prompt "validate this config" < input.json
 *
 * Commands:
 *   generate <prompt>  - Run AI generation with the given prompt
 *   list-tools         - List all available Function Tools
 *   help               - Show this help
 */

import { getToolDefinitions } from './bridge.js'

async function main() {
  const args = process.argv.slice(2)
  const command = args[0] || 'help'

  switch (command) {
    case 'generate': {
      const promptIndex = args.indexOf('--prompt')
      const prompt = promptIndex !== -1
        ? args.slice(promptIndex + 1).join(' ')
        : args.slice(1).join(' ')

      if (!prompt) {
        console.error('用法: bs ai generate <提示文本>')
        console.error('  或:  bs ai generate --prompt "<提示文本>"')
        process.exit(1)
      }

      const { generateWithAI } = await import('./bridge.js')
      await generateWithAI(prompt)
      break
    }

    case 'list-tools': {
      const tools = getToolDefinitions()
      console.log(`BlessStar AI — 可用工具 (${tools.length} 个):\n`)
      for (const tool of tools) {
        console.log(`  ${tool.name}`)
        console.log(`    描述: ${tool.description}`)
        const params = tool.parameters.properties as Record<string, { type: string; description: string }>
        if (params) {
          const required = (tool.parameters.required as string[]) || []
          for (const [name, meta] of Object.entries(params)) {
            const req = required.includes(name) ? ' [必填]' : ''
            console.log(`    - ${name}: ${meta.description} (${meta.type})${req}`)
          }
        }
        console.log()
      }
      break
    }

    case 'help':
    default:
      console.log(`BlessStar AI CLI — 配置助手

用法:
  bs ai generate <提示文本>       用 AI 生成配置（自然语言驱动）
  bs ai generate --prompt "..."   同上，推荐含引号
  bs ai list-tools                列出所有可用的 Function Tool
  bs ai help                      显示此帮助

示例:
  bs ai generate "create a field named server_port of type number"
  bs ai generate "add a gate rule for production environment"
  bs ai list-tools

快捷键（编辑器中）:
  Ctrl+Shift+A                    切换 AI 助手面板

共享架构:
  - 编辑器 AI 面板和 CLI 使用同一套 Function Tool 定义
  - 所有 Tool 输出均通过 BlessStar Schema/Gate 校验器验证
  - 校验失败返回错误给 AI 重试（最多 3 次）
`)
      break
  }
}

main().catch((err) => {
  console.error('CLI 执行错误:', err)
  process.exit(1)
})
