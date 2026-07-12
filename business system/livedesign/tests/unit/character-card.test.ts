/**
 * 角色卡校验单元测试
 * 测试 validateCharacterCard 函数的各种场景
 */
import { describe, it, expect } from 'vitest';
import { validateCharacterCard, CharacterCard } from '../../src/shared/character-card';

describe('validateCharacterCard', () => {
  it('有效角色卡应通过校验', () => {
    const card: CharacterCard = {
      name: '绫波丽',
      description: '沉默寡言的少女',
      style: '简洁、冷静的对话风格',
      background: 'EVA 驾驶员',
      knowledge: ['战斗', '驾驶'],
      examples: [
        { user: '你好', assistant: '嗯。' },
      ],
      modelPath: 'ayanami/model.json',
    };

    const result = validateCharacterCard(card);
    expect(result.valid).toBe(true);
    expect(result.errors).toHaveLength(0);
  });

  it('非对象输入应校验失败', () => {
    const result = validateCharacterCard(null);
    expect(result.valid).toBe(false);
    expect(result.errors).toContain('角色卡必须是一个对象');
  });

  it('缺少必填字段 name 时应校验失败', () => {
    const result = validateCharacterCard({
      description: 'test',
      style: 'test',
      background: 'test',
      knowledge: [],
      examples: [],
      modelPath: 'test',
    });

    expect(result.valid).toBe(false);
    expect(result.errors).toContain('缺少必填字段: name (角色名称)');
  });

  it('缺少必填字段 description 时应校验失败', () => {
    const result = validateCharacterCard({
      name: 'Test',
      style: 'test',
      background: 'test',
    });

    expect(result.valid).toBe(false);
    expect(result.errors).toContain('缺少必填字段: description (角色描述)');
  });

  it('name 类型错误时应校验失败', () => {
    const result = validateCharacterCard({
      name: 123,
      description: 'test',
      style: 'test',
      background: 'test',
      knowledge: [],
      examples: [],
      modelPath: 'test',
    });

    expect(result.valid).toBe(false);
    expect(result.errors).toContain('缺少必填字段: name (角色名称)');
  });

  it('modelPath 类型错误时应校验失败', () => {
    const result = validateCharacterCard({
      name: 'Test',
      description: 'test',
      style: 'test',
      background: 'test',
      knowledge: [],
      examples: [],
      modelPath: 12345,
    });

    expect(result.valid).toBe(false);
    expect(result.errors).toContain('modelPath 必须是字符串');
  });

  it('knowledge 类型错误时应校验失败', () => {
    const result = validateCharacterCard({
      name: 'Test',
      description: 'test',
      style: 'test',
      background: 'test',
      knowledge: 'not-an-array',
      examples: [],
      modelPath: 'test',
    });

    expect(result.valid).toBe(false);
    expect(result.errors).toContain('knowledge 必须是字符串数组');
  });

  it('缺少 style 时应触发 warnings 而非 errors', () => {
    const result = validateCharacterCard({
      name: 'Test',
      description: 'test',
      background: 'test',
      knowledge: [],
      examples: [],
      modelPath: 'test',
    });

    // style 缺少时，name 和 description 存在则 valid 为 true
    expect(result.valid).toBe(true);
    expect(result.warnings).toContain(
      '建议填写 style (说话风格)，有助于 AI 更准确地模拟角色语气',
    );
  });

  it('应同时返回多个错误', () => {
    const result = validateCharacterCard({
      // 缺失 name、description
      style: 'test',
      background: 'test',
    });

    expect(result.valid).toBe(false);
    expect(result.errors.length).toBeGreaterThanOrEqual(2);
    expect(result.errors).toContain('缺少必填字段: name (角色名称)');
    expect(result.errors).toContain('缺少必填字段: description (角色描述)');
  });
});
