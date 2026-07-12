"use strict";
/**
 * 配置 Schema 类型定义（对应 config-schema.yaml）
 * 用于运行时类型安全和配置校验
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.getDefaultValue = getDefaultValue;
exports.validateConfigValue = validateConfigValue;
/**
 * 获取配置字段的默认值（按照 Schema 定义）
 */
function getDefaultValue(field) {
    return field.default;
}
/**
 * 校验配置值是否在 contract 约束范围内
 */
function validateConfigValue(field, value) {
    if (field.contract?.range && typeof value === 'number') {
        const [min, max] = field.contract.range;
        if (value < min || value > max) {
            return `${field.ui_meta?.label || field.key} 的值必须在 ${min}~${max} 之间`;
        }
    }
    if (field.contract?.enum_values && typeof value === 'string') {
        if (!field.contract.enum_values.includes(value)) {
            return `${field.ui_meta?.label || field.key} 的值必须是以下之一: ${field.contract.enum_values.join(', ')}`;
        }
    }
    return null;
}
//# sourceMappingURL=config-schema.js.map