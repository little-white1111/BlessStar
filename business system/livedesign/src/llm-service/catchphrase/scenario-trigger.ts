/**
 * ScenarioTriggerMatcher（场景触发匹配器）
 *
 * 根据场景标签匹配对应的口头禅。
 * 支持精确匹配和模糊匹配。
 */

import type { RelationalCatchphrase } from './types';

export class ScenarioTriggerMatcher {
  /**
   * 根据场景标签匹配活跃的 Relational 口头禅
   * @param tags 当前场景标签列表
   * @param activeCatchphrases 所有活跃的 Relational 口头禅
   * @returns 匹配结果（按置信度排序）
   */
  match(
    tags: string[],
    activeCatchphrases: RelationalCatchphrase[],
  ): Array<{ catchphrase: RelationalCatchphrase; confidence: number }> {
    if (tags.length === 0 || activeCatchphrases.length === 0) return [];

    const results: Array<{ catchphrase: RelationalCatchphrase; confidence: number }> = [];

    for (const cp of activeCatchphrases) {
      if (!cp.scenarioTag) continue;

      // 精确匹配
      if (tags.includes(cp.scenarioTag)) {
        results.push({ catchphrase: cp, confidence: 1.0 });
        continue;
      }

      // 模糊匹配：检查场景标签是否包含关系型口头禅的场景标签
      const partialMatch = tags.some(
        (tag) => tag.includes(cp.scenarioTag!) || cp.scenarioTag!.includes(tag),
      );
      if (partialMatch) {
        results.push({ catchphrase: cp, confidence: 0.6 });
      }
    }

    // 按置信度降序
    results.sort((a, b) => b.confidence - a.confidence);
    return results;
  }
}
