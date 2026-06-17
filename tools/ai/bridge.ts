#!/usr/bin/env node

/**
 * BlessStar AI CLI — `bs ai` commands
 *
 * Usage:
 *   npx ts-node tools/ai/cli.ts generate --prompt "create a field named server_port"
 *   npx ts-node tools/ai/cli.ts generate --file input.json
 *
 * Shares same Function Tool definitions and validation logic as the editor AI panel.
 */

import { getToolDefinitions, findTool, executeToolCall } from '../../app/editor/src/ai/executor.js'

// Re-export for CLI bridge usage
export { getToolDefinitions, findTool, executeToolCall }

export async function generateWithAI(prompt: string): Promise<void> {
  const tools = getToolDefinitions()
  console.log(`[BlessStar AI] 收到提示: "${prompt}"`)
  console.log(`[BlessStar AI] 可用工具: ${tools.map((t) => t.name).join(', ')}`)

  // Simple keyword matching for MVP
  const matchedTool = tools.find((t) => prompt.toLowerCase().includes(t.name))
  if (!matchedTool) {
    console.log('[BlessStar AI] 未匹配到工具，返回可用工具列表。')
    console.log(JSON.stringify(tools, null, 2))
    return
  }

  console.log(`[BlessStar AI] 匹配工具: ${matchedTool.name}\n`)

  const toolCall = {
    id: `cli_${Date.now()}`,
    type: 'function' as const,
    function: {
      name: matchedTool.name,
      arguments: '{}',
    },
  }

  const result = await executeToolCall(toolCall)
  if (result.success) {
    console.log('[BlessStar AI] 执行成功:')
    console.log(JSON.stringify(result.data, null, 2))
  } else {
    console.error(`[BlessStar AI] 执行失败: ${result.error}`)
    process.exit(1)
  }
}
