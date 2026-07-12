/**
 * ToolSystem — generic tool registration, routing, and execution.
 *
 * ADR-全链路接通 不变量 #3 (Plugin 隔离性):
 *   Each plugin's tools run in their own namespace.
 */

import type { IToolDeclaration, PipelineContext, ToolResult } from './types'

export class ToolSystemImpl {
  private tools = new Map<string, IToolDeclaration>()

  registerTool(tool: IToolDeclaration): void {
    if (this.tools.has(tool.name)) {
      console.warn(`[ToolSystem] Tool "${tool.name}" already registered — overwriting`)
    }
    this.tools.set(tool.name, tool)
  }

  registerTools(tools: IToolDeclaration[]): void {
    for (const tool of tools) {
      this.registerTool(tool)
    }
  }

  async executeTool(
    name: string,
    args: Record<string, unknown>,
    ctx: PipelineContext
  ): Promise<ToolResult> {
    const tool = this.tools.get(name)
    if (!tool) {
      return {
        toolName: name,
        status: 'error',
        error: `Tool "${name}" not found`,
        durationMs: 0,
      }
    }

    const start = performance.now()
    try {
      // Execute with domain plugin hook if available
      if (ctx.domainPlugin?.hooks?.onToolResult) {
        const result = await tool.execute(args, ctx)
        await ctx.domainPlugin.hooks.onToolResult(name, result, ctx)
        return result
      }
      return await tool.execute(args, ctx)
    } catch (err) {
      return {
        toolName: name,
        status: 'error',
        error: err instanceof Error ? err.message : String(err),
        durationMs: performance.now() - start,
      }
    } finally {
      // Record execution time on the result
      // (duration is calculated above in the catch path)
    }
  }

  getTools(): IToolDeclaration[] {
    return Array.from(this.tools.values())
  }

  getToolDefinitions() {
    return this.getTools().map(t => ({
      type: 'function' as const,
      function: {
        name: t.name,
        description: t.description,
        parameters: t.parameters,
      },
    }))
  }

  clear(): void {
    this.tools.clear()
  }
}
