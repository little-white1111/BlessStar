/**
 * L1 核心人格加载器
 *
 * 架构不变量 C1: 核心人格不可在运行时修改 — 读取 config-schema.yaml 的 immutable 字段
 * EvolutionEngine 在演进时必须跳过 L1 字段。
 *
 * 加载策略（与现有 llm-service/index.ts 一致）:
 *   1. 优先读取环境变量（由主进程通过 config-schema 注入）
 *   2. 回退到硬编码默认值（与 config-schema.yaml 一致）
 */

import {
  CorePersona,
  DEFAULT_CORE_PERSONA,
  CORE_PERSONA_RANGES,
} from './types';

/** 环境变量前缀 */
const ENV_PREFIX = 'PERSONA_CORE_';

/**
 * 从环境变量加载 L1 核心人格。
 * 环境变量名: PERSONA_CORE_WARMTH, PERSONA_CORE_RATIONALITY, PERSONA_CORE_CURIOSITY
 *
 * 架构不变量 C1: 返回值不可在运行时修改 — 调用方不得写入这些字段。
 */
export function loadCorePersona(): CorePersona {
  const warmth = parseFloat(process.env[`${ENV_PREFIX}WARMTH`] ?? String(DEFAULT_CORE_PERSONA.warmth));
  const rationality = parseFloat(process.env[`${ENV_PREFIX}RATIONALITY`] ?? String(DEFAULT_CORE_PERSONA.rationality));
  const curiosity = parseFloat(process.env[`${ENV_PREFIX}CURIOSITY`] ?? String(DEFAULT_CORE_PERSONA.curiosity));

  return {
    warmth: clamp(warmth, CORE_PERSONA_RANGES.warmth.min, CORE_PERSONA_RANGES.warmth.max),
    rationality: clamp(rationality, CORE_PERSONA_RANGES.rationality.min, CORE_PERSONA_RANGES.rationality.max),
    curiosity: clamp(curiosity, CORE_PERSONA_RANGES.curiosity.min, CORE_PERSONA_RANGES.curiosity.max),
  };
}

/**
 * 检查是否为 L1 核心人格字段。
 * EvolutionEngine 演进时应跳过这些字段（架构不变量 C1）。
 */
export function isCoreField(fieldName: string): boolean {
  const coreFields: Array<keyof CorePersona> = ['warmth', 'rationality', 'curiosity'];
  return coreFields.includes(fieldName as keyof CorePersona);
}

function clamp(value: number, min: number, max: number): number {
  return Math.max(min, Math.min(max, value));
}
