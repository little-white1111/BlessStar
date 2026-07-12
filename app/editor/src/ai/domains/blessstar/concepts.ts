/**
 * BlessStar Concept Knowledge — domain concepts for context building.
 *
 * ADR-全链路接通 — These are returned by IDomainPlugin.getConceptKnowledge()
 * and used for intent matching and context enrichment.
 */

import type { IConceptEntry } from '../../engine/types'

export const BLESSSTAR_CONCEPTS: IConceptEntry[] = [
  {
    key: 'workspace',
    description: 'BlessStar 项目工作区，包含 src/ (源文件)、dist/ (构建输出)、.blessstar/ (元数据)',
    keywords: ['workspace', '工作区', '项目', 'project', '工作空间'],
  },
  {
    key: 'config_key',
    description: '配置键路径，点号分隔的嵌套路径，如 "server.port"、"logging.level"',
    keywords: ['key', '配置项', '字段', 'field', '配置键', '键路径'],
  },
  {
    key: 'gate_chain',
    description: '门控链校验规则，配置变更必须通过 Gate 校验才能写入',
    keywords: ['gate', '门控', '校验', '验证', 'validate', 'check', '规则', 'rule'],
  },
  {
    key: 'format',
    description: '配置文件格式，支持 json/yaml/toml/ini，自动检测',
    keywords: ['format', '格式', 'json', 'yaml', 'toml', 'ini', '配置文件格式'],
  },
  {
    key: 'build',
    description: '构建工作区，将源文件转换为目标格式输出到 dist/ 目录',
    keywords: ['build', '构建', '编译', '生成', 'dist', '输出'],
  },
  {
    key: 'export',
    description: '导出配置，将工作区中的源文件转换为指定格式输出到外部路径',
    keywords: ['export', '导出', '导入', 'import', '分发', '发布'],
  },
  {
    key: 'source_file',
    description: '源文件，位于 workspace 的 src/ 目录下的配置文件',
    keywords: ['source', '源文件', 'src', '配置文件', 'yaml文件', 'json文件'],
  },
  {
    key: 'trace',
    description: '执行追踪，每次 AI 操作都有唯一的 traceId 记录完整调用链',
    keywords: ['trace', '追踪', '日志', '历史', 'history', 'traceId'],
  },
  {
    key: 'schema',
    description: '配置 Schema，描述配置项的字段定义、类型、默认值和校验规则',
    keywords: ['schema', '模式', '字段定义', '类型', '默认值'],
  },
  {
    key: 'env',
    description: '环境区分，支持多环境配置（production/staging/development）',
    keywords: ['环境', 'environment', 'production', '生产', 'staging', '预发布', 'development', '开发'],
  },
]
