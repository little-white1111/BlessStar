/**
 * 角色卡数据结构（架构不变量 #7 — 角色卡是角色扮演模式的唯一真理源）
 * 用户自行编写 .json 文件格式
 */

/** 角色卡 */
export interface CharacterCard {
  /** 角色名称 */
  name: string;
  /** 角色描述 / 人格设定 */
  description: string;
  /** 说话风格指导 */
  style: string;
  /** 角色背景故事 */
  background: string;
  /** 角色知识领域 */
  knowledge: string[];
  /** 角色示例对话 (few-shot) */
  examples: Array<{
    user: string;
    assistant: string;
    emotion?: string;
  }>;
  /** 关联的 Live2D 模型路径（相对于 resources/live2d/） */
  modelPath: string;
  /** 角色特有的动作-情绪映射表（覆盖默认映射） */
  emotionActionMap?: Record<string, string>;
  /** 附加設定 */
  settings?: Record<string, unknown>;
}

/** 角色卡校验结果 */
export interface CharacterCardValidation {
  valid: boolean;
  errors: string[];
  warnings: string[];
}

/**
 * 校验角色卡的完整性
 */
export function validateCharacterCard(card: unknown): CharacterCardValidation {
  const errors: string[] = [];
  const warnings: string[] = [];

  if (!card || typeof card !== 'object') {
    return { valid: false, errors: ['角色卡必须是一个对象'], warnings: [] };
  }

  const c = card as Record<string, unknown>;

  if (!c.name || typeof c.name !== 'string') {
    errors.push('缺少必填字段: name (角色名称)');
  }
  if (!c.description || typeof c.description !== 'string') {
    errors.push('缺少必填字段: description (角色描述)');
  }
  if (!c.style || typeof c.style !== 'string') {
    warnings.push('建议填写 style (说话风格)，有助于 AI 更准确地模拟角色语气');
  }
  if (c.knowledge !== undefined && !Array.isArray(c.knowledge)) {
    errors.push('knowledge 必须是字符串数组');
  }
  if (c.modelPath && typeof c.modelPath !== 'string') {
    errors.push('modelPath 必须是字符串');
  }

  return {
    valid: errors.length === 0,
    errors,
    warnings,
  };
}
