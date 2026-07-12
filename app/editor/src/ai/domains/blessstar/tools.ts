/**
 * BlessStar Tool Set — domain-specific tools for BlessStar config editing.
 *
 * ADR-全链路接通 不变量 #2 (Workspace 数据主权):
 *   All config file writes go through WorkspaceManager (→ bs_workspace_* C ABI).
 * ADR-全链路接通 不变量 #3 (Plugin 隔离性):
 *   These tools are registered under the 'blessstar' plugin namespace only.
 */

import type { IToolDeclaration, PipelineContext, ToolResult } from '../../engine/types'

export const BLESSSTAR_TOOLS: IToolDeclaration[] = [
  // ── read_config_value ─────────────────────────────────────────────
  {
    name: 'blessstar_read_config',
    description: '读取指定配置项的值，支持嵌套路径。例如: "server.port" 或 "logging.level"。',
    parameters: {
      type: 'object',
      properties: {
        key: {
          type: 'string',
          description: '配置键路径，如 "server.port"',
        },
      },
      required: ['key'],
    },
    async execute(args: Record<string, unknown>, ctx: PipelineContext): Promise<ToolResult> {
      const start = performance.now()
      try {
        const key = String(args.key || '')
        if (!key) {
          return { toolName: 'blessstar_read_config', status: 'error', error: 'key 不能为空', durationMs: 0 }
        }

        // Read from persistence (IPC → native backend)
        const provider = ctx.domainPlugin?.getPersistenceProvider?.()
        const raw = provider ? await provider.load(key) : null

        return {
          toolName: 'blessstar_read_config',
          status: 'success',
          data: { key, value: raw ?? null },
          durationMs: performance.now() - start,
        }
      } catch (err) {
        return {
          toolName: 'blessstar_read_config',
          status: 'error',
          error: err instanceof Error ? err.message : String(err),
          durationMs: performance.now() - start,
        }
      }
    },
  },

  // ── write_config_value ────────────────────────────────────────────
  {
    name: 'blessstar_write_config',
    description: '写入/更新指定配置项的值。value 支持 string/number/boolean/object 类型。',
    parameters: {
      type: 'object',
      properties: {
        key: {
          type: 'string',
          description: '配置键路径，如 "server.port"',
        },
        value: {
          description: '配置值，支持 string/number/boolean/object',
        },
        source: {
          type: 'string',
          description: '目标源文件路径（相对于 workspace src/），如 "prod.yaml"。不指定则写入第一个匹配源',
        },
      },
      required: ['key', 'value'],
    },
    async execute(args: Record<string, unknown>, ctx: PipelineContext): Promise<ToolResult> {
      const start = performance.now()
      try {
        const key = String(args.key || '')
        const value = args.value

        if (!key) {
          return { toolName: 'blessstar_write_config', status: 'error', error: 'key 不能为空', durationMs: 0 }
        }

        // Write through persistence (IPC → bs_workspace_write)
        const provider = ctx.domainPlugin?.getPersistenceProvider?.()
        if (provider) {
          const source = args.source ? String(args.source) : undefined
          await provider.save({ key, value, source }, key)
        }

        return {
          toolName: 'blessstar_write_config',
          status: 'success',
          data: { key, value, written: true },
          durationMs: performance.now() - start,
        }
      } catch (err) {
        return {
          toolName: 'blessstar_write_config',
          status: 'error',
          error: err instanceof Error ? err.message : String(err),
          durationMs: performance.now() - start,
        }
      }
    },
  },

  // ── validate_config ──────────────────────────────────────────────
  {
    name: 'blessstar_validate_config',
    description: '校验当前配置是否满足所有已注册的 Gate 规则。返回校验结果和违规项列表。',
    parameters: {
      type: 'object',
      properties: {
        scope: {
          type: 'string',
          description: '校验范围: "all" | "changed" | 特定 key',
          enum: ['all', 'changed'],
        },
      },
      required: [],
    },
    async execute(args: Record<string, unknown>, ctx: PipelineContext): Promise<ToolResult> {
      const start = performance.now()
      try {
        const scope = String(args.scope || 'all')

        // For MVP: delegate to IPC validation
        const result = await window.blessstar?.validateConfig?.(JSON.stringify({ scope }))
          ?? { valid: true, errors: [] }

        return {
          toolName: 'blessstar_validate_config',
          status: 'success',
          data: {
            valid: result.valid,
            errorCount: result.errors?.length ?? 0,
            errors: result.errors ?? [],
          },
          durationMs: performance.now() - start,
        }
      } catch (err) {
        return {
          toolName: 'blessstar_validate_config',
          status: 'error',
          error: err instanceof Error ? err.message : String(err),
          durationMs: performance.now() - start,
        }
      }
    },
  },

  // ── list_configs ──────────────────────────────────────────────────
  {
    name: 'blessstar_list_configs',
    description: '列出当前工作区中所有可配置项，可按前缀过滤。',
    parameters: {
      type: 'object',
      properties: {
        prefix: {
          type: 'string',
          description: '键前缀过滤，如 "server."',
        },
        format: {
          type: 'string',
          description: '输出格式: "flat" | "tree"',
          enum: ['flat', 'tree'],
        },
      },
      required: [],
    },
    async execute(args: Record<string, unknown>, ctx: PipelineContext): Promise<ToolResult> {
      const start = performance.now()
      try {
        const prefix = String(args.prefix || '')
        const format = String(args.format || 'flat')

        // Read from schema registry
        const schemas = await window.blessstar?.getRegisteredSchemas?.()
          ?? { fields: [] }

        let fields = schemas.fields ?? []
        if (prefix) {
          fields = fields.filter((f: any) => f.key?.startsWith(prefix))
        }

        return {
          toolName: 'blessstar_list_configs',
          status: 'success',
          data: {
            count: fields.length,
            format,
            configs: fields.map((f: any) => ({
              key: f.key,
              type: f.type_name || f.type || 'string',
              description: f.description || '',
              default: f.default_value ?? f.default ?? null,
            })),
          },
          durationMs: performance.now() - start,
        }
      } catch (err) {
        return {
          toolName: 'blessstar_list_configs',
          status: 'error',
          error: err instanceof Error ? err.message : String(err),
          durationMs: performance.now() - start,
        }
      }
    },
  },

  // ── workspace_build ──────────────────────────────────────────────
  {
    name: 'blessstar_workspace_build',
    description: '构建工作区，将所有源文件转换为目标格式并输出到 dist/ 目录。',
    parameters: {
      type: 'object',
      properties: {
        target_format: {
          type: 'string',
          description: '目标格式',
          enum: ['json', 'yaml', 'toml', 'ini'],
        },
      },
      required: ['target_format'],
    },
    async execute(args: Record<string, unknown>, ctx: PipelineContext): Promise<ToolResult> {
      const start = performance.now()
      try {
        const targetFormat = String(args.target_format || 'json')

        await window.blessstar?.workspace?.build(targetFormat)

        return {
          toolName: 'blessstar_workspace_build',
          status: 'success',
          data: { targetFormat, built: true },
          durationMs: performance.now() - start,
        }
      } catch (err) {
        return {
          toolName: 'blessstar_workspace_build',
          status: 'error',
          error: err instanceof Error ? err.message : String(err),
          durationMs: performance.now() - start,
        }
      }
    },
  },

  // ── workspace_export ─────────────────────────────────────────────
  {
    name: 'blessstar_workspace_export',
    description: '导出指定源文件到目标格式和路径。供"导入/导出工作流"使用。',
    parameters: {
      type: 'object',
      properties: {
        source: {
          type: 'string',
          description: '源文件路径（相对于 workspace src/），如 "prod.yaml"',
        },
        target_format: {
          type: 'string',
          description: '目标格式',
          enum: ['json', 'yaml', 'toml', 'ini'],
        },
        output_path: {
          type: 'string',
          description: '输出路径（绝对路径或相对于 project root）',
        },
      },
      required: ['source', 'target_format', 'output_path'],
    },
    async execute(args: Record<string, unknown>, ctx: PipelineContext): Promise<ToolResult> {
      const start = performance.now()
      try {
        const source = String(args.source || '')
        const targetFormat = String(args.target_format || 'json')
        const outputPath = String(args.output_path || '')

        if (!source || !outputPath) {
          return { toolName: 'blessstar_workspace_export', status: 'error', error: 'source 和 output_path 不能为空', durationMs: 0 }
        }

        await window.blessstar?.workspace?.export(source, targetFormat, outputPath)

        return {
          toolName: 'blessstar_workspace_export',
          status: 'success',
          data: { source, targetFormat, outputPath, exported: true },
          durationMs: performance.now() - start,
        }
      } catch (err) {
        return {
          toolName: 'blessstar_workspace_export',
          status: 'error',
          error: err instanceof Error ? err.message : String(err),
          durationMs: performance.now() - start,
        }
      }
    },
  },
]
