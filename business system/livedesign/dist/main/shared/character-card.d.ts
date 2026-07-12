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
export declare function validateCharacterCard(card: unknown): CharacterCardValidation;
//# sourceMappingURL=character-card.d.ts.map