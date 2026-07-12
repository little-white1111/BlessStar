/**
 * 德性一致性检查器（Virtue Consistency Checker）
 *
 * 架构不变量 C2: 自适应演进必须经一致性检查 — Evolve() 末尾调用 Check()，失败则回滚+警告
 *
 * 一致性规则:
 *   1. 所有自适应值必须在各自 min/max 范围内
 *   2. 自适应值不得偏离对应核心特质超过 0.3
 */

import type { CorePersona, AdaptivePersona } from '../types';
import { ADAPTIVE_PERSONA_RANGES, ADAPTIVE_TO_CORE_MAP } from '../types';

/** 一致性检查结果 */
export interface ConsistencyCheckResult {
  /** 是否通过 */
  passed: boolean;
  /** 失败的检查项目列表 */
  failures: ConsistencyFailure[];
}

/** 一致性检查失败项 */
export interface ConsistencyFailure {
  /** 特质名称 */
  trait: string;
  /** 失败原因 */
  reason: string;
  /** 当前值 */
  value: number;
  /** 期望范围或约束 */
  constraint: string;
}

/** 自适应特质偏离核心特质的最大允许差值 */
const MAX_CORE_DEVIATION = 0.3;
const EPSILON = 1e-10;

/**
 * 执行德性一致性检查（架构不变量 C2）。
 *
 * @param core    L1 核心人格
 * @param adaptive L2 自适应人格
 * @returns 检查结果
 */
export function checkConsistency(
  core: CorePersona,
  adaptive: AdaptivePersona
): ConsistencyCheckResult {
  const failures: ConsistencyFailure[] = [];

  // 规则 1: 检查范围约束
  for (const [trait, range] of Object.entries(ADAPTIVE_PERSONA_RANGES)) {
    const value = adaptive[trait as keyof AdaptivePersona];
    if (value < range.min || value > range.max) {
      failures.push({
        trait,
        reason: `超出范围 [${range.min}, ${range.max}]`,
        value,
        constraint: `[${range.min}, ${range.max}]`,
      });
    }
  }

  // 规则 2: 检查与核心特质的偏离
  for (const [adaptiveTrait, coreTrait] of Object.entries(ADAPTIVE_TO_CORE_MAP)) {
    const adaptiveValue = adaptive[adaptiveTrait as keyof AdaptivePersona];
    const coreValue = core[coreTrait as keyof CorePersona];
    const deviation = Math.abs(adaptiveValue - coreValue);

    if (deviation > MAX_CORE_DEVIATION + EPSILON) {
      failures.push({
        trait: adaptiveTrait,
        reason: `偏离核心特质 ${coreTrait} 超过 ${MAX_CORE_DEVIATION}`,
        value: adaptiveValue,
        constraint: `core.${coreTrait} ± ${MAX_CORE_DEVIATION} (当前 core=${coreValue})`,
      });
    }
  }

  return {
    passed: failures.length === 0,
    failures,
  };
}
