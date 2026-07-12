/**
 * 配置 Schema 单元测试
 * 测试 validateConfigValue 和 getDefaultValue 函数
 */
import { describe, it, expect } from 'vitest';
import { ConfigField, validateConfigValue, getDefaultValue } from '../../src/shared/config-schema';

describe('validateConfigValue', () => {
  describe('range 约束校验', () => {
    it('值在 range 范围内时应通过校验', () => {
      const field: ConfigField = {
        key: 'temperature',
        type: 'F64',
        default: 0.7,
        required: true,
        business_desc: '模型温度参数',
        contract: { range: [0, 1] },
      };

      expect(validateConfigValue(field, 0.5)).toBeNull();
      expect(validateConfigValue(field, 0)).toBeNull();
      expect(validateConfigValue(field, 1)).toBeNull();
    });

    it('值超出 range 范围时应返回错误', () => {
      const field: ConfigField = {
        key: 'temperature',
        type: 'F64',
        default: 0.7,
        required: true,
        business_desc: '模型温度参数',
        contract: { range: [0, 1] },
        ui_meta: { label: '温度', order: 1, hidden: false },
      };

      const result = validateConfigValue(field, 1.5);
      expect(result).not.toBeNull();
      expect(result).toContain('温度');
      expect(result).toContain('0~1');
    });

    it('超出 range 范围时未设置 ui_meta.label 应使用 key', () => {
      const field: ConfigField = {
        key: 'temperature',
        type: 'F64',
        default: 0.7,
        required: true,
        business_desc: '模型温度参数',
        contract: { range: [0, 1] },
      };

      const result = validateConfigValue(field, 2);
      expect(result).toContain('temperature');
    });

    it('非数值类型的 value 应跳过 range 校验', () => {
      const field: ConfigField = {
        key: 'model',
        type: 'STR',
        default: 'gpt-4',
        required: true,
        business_desc: '模型名称',
        contract: { range: [0, 100] },
      };

      // 字符串值跳过 range 校验
      expect(validateConfigValue(field, 'gpt-4')).toBeNull();
    });
  });

  describe('enum_values 约束校验', () => {
    it('值在 enum_values 中时应通过校验', () => {
      const field: ConfigField = {
        key: 'model',
        type: 'STR',
        default: 'gpt-4',
        required: true,
        business_desc: '模型选择',
        contract: { enum_values: ['gpt-4', 'gpt-3.5', 'claude-3'] },
      };

      expect(validateConfigValue(field, 'gpt-4')).toBeNull();
      expect(validateConfigValue(field, 'claude-3')).toBeNull();
    });

    it('值不在 enum_values 中时应返回错误', () => {
      const field: ConfigField = {
        key: 'model',
        type: 'STR',
        default: 'gpt-4',
        required: true,
        business_desc: '模型选择',
        contract: { enum_values: ['gpt-4', 'gpt-3.5'] },
        ui_meta: { label: '模型', order: 1, hidden: false },
      };

      const result = validateConfigValue(field, 'llama-3');
      expect(result).not.toBeNull();
      expect(result).toContain('模型');
      expect(result).toContain('gpt-4, gpt-3.5');
    });

    it('非字符串类型的 value 应跳过 enum_values 校验', () => {
      const field: ConfigField = {
        key: 'model',
        type: 'I32',
        default: 1,
        required: true,
        business_desc: '模型编号',
        contract: { enum_values: ['a', 'b'] },
      };

      expect(validateConfigValue(field, 1)).toBeNull();
    });
  });

  describe('无 contract 字段', () => {
    it('没有 contract 时应始终通过校验', () => {
      const field: ConfigField = {
        key: 'maxTokens',
        type: 'I32',
        default: 2048,
        required: false,
        business_desc: '最大 Token 数',
      };

      expect(validateConfigValue(field, 4096)).toBeNull();
      expect(validateConfigValue(field, 0)).toBeNull();
    });
  });

  describe('多个约束同时存在', () => {
    it('应同时检查 range 和 enum_values', () => {
      const field: ConfigField = {
        key: 'priority',
        type: 'STR',
        default: 'medium',
        required: true,
        business_desc: '优先级',
        contract: {
          range: [0, 10],
          enum_values: ['low', 'medium', 'high'],
        },
      };

      // 字符串值：跳过 range，校验 enum_values
      expect(validateConfigValue(field, 'low')).toBeNull();
      expect(validateConfigValue(field, 'urgent')).not.toBeNull();
    });
  });
});

describe('getDefaultValue', () => {
  it('应返回字段的 default 值', () => {
    const field: ConfigField = {
      key: 'temperature',
      type: 'F64',
      default: 0.7,
      required: true,
      business_desc: '温度参数',
    };

    expect(getDefaultValue(field)).toBe(0.7);
  });

  it('应返回字符串默认值', () => {
    const field: ConfigField = {
      key: 'model',
      type: 'STR',
      default: 'gpt-4',
      required: true,
      business_desc: '模型',
    };

    expect(getDefaultValue(field)).toBe('gpt-4');
  });

  it('应返回布尔默认值', () => {
    const field: ConfigField = {
      key: 'enabled',
      type: 'BOOL',
      default: true,
      required: true,
      business_desc: '启用开关',
    };

    expect(getDefaultValue(field)).toBe(true);
  });

  it('应返回数组默认值', () => {
    const field: ConfigField = {
      key: 'tools',
      type: 'ARR',
      default: ['read', 'write'],
      required: false,
      business_desc: '工具列表',
    };

    const val = getDefaultValue(field) as unknown[];
    expect(val).toEqual(['read', 'write']);
  });
});
