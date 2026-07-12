"use strict";
/**
 * 角色卡数据结构（架构不变量 #7 — 角色卡是角色扮演模式的唯一真理源）
 * 用户自行编写 .json 文件格式
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.validateCharacterCard = validateCharacterCard;
/**
 * 校验角色卡的完整性
 */
function validateCharacterCard(card) {
    const errors = [];
    const warnings = [];
    if (!card || typeof card !== 'object') {
        return { valid: false, errors: ['角色卡必须是一个对象'], warnings: [] };
    }
    const c = card;
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
//# sourceMappingURL=character-card.js.map