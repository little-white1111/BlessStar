/**
 * TaskList 渲染函数测试
 * 纯函数测试：输入 props → 输出 HTML 字符串
 */

import { describe, it, expect } from 'vitest';
import { renderTaskList } from '../render';

describe('renderTaskList', () => {
  // ---- 默认 props ----
  it('使用默认 props 时应渲染标题和"暂无任务"占位', () => {
    const html = renderTaskList({});
    expect(html).toContain('任务列表');
    expect(html).toContain('暂无任务');
  });

  // ---- title 变更 ----
  it('传入自定义 title 时 HTML 应包含新标题', () => {
    const html = renderTaskList({ title: '我的待办', items: '任务A' });
    expect(html).toContain('我的待办');
    expect(html).not.toContain('任务列表');
  });

  // ---- items 为空 ----
  it('items 为空时 HTML 应显示"暂无任务"', () => {
    const html = renderTaskList({ items: '' });
    expect(html).toContain('暂无任务');
  });

  it('items 为逗号分隔字符串时 HTML 应生成对应任务项', () => {
    const html = renderTaskList({ items: '任务A,任务B,任务C' });
    expect(html).toContain('任务A');
    expect(html).toContain('任务B');
    expect(html).toContain('任务C');
    expect(html).toContain('#1');
    expect(html).toContain('#3');
  });

  it('items 中有空项时应被过滤', () => {
    const html = renderTaskList({ items: '任务A,,任务C,' });
    expect(html).toContain('任务A');
    expect(html).toContain('任务C');
    // 空字符串项应被过滤，'任务A' 是 #1，'任务C' 是 #2
    expect(html).toContain('#1');
    expect(html).toContain('#2');
    expect(html).not.toContain('#3');
  });

  // ---- showHeader ----
  it('showHeader=false 时 HTML 不应包含 header 区域', () => {
    const html = renderTaskList({ showHeader: false, items: '任务1' });
    // header 包含标题和"项"统计
    expect(html).not.toContain('1 项');
    // 任务内容仍应存在
    expect(html).toContain('任务1');
  });

  it('showHeader=true（默认）时 HTML 应包含 header', () => {
    const html = renderTaskList({ items: '任务1,任务2' });
    expect(html).toContain('2 项');
  });

  // ---- backgroundColor ----
  it('backgroundColor 应在 HTML 的 style 中生效', () => {
    const html = renderTaskList({ backgroundColor: '#ff0000', items: '任务1' });
    expect(html).toContain('background:#ff0000');
  });

  // ---- borderRadius ----
  it('borderRadius 应在 HTML 的 style 中生效', () => {
    const html = renderTaskList({ borderRadius: 16, items: '任务1' });
    expect(html).toContain('border-radius:16px');
  });

  // ---- fontSize & textColor ----
  it('fontSize 和 textColor 应影响任务项文字样式', () => {
    const html = renderTaskList({ fontSize: 20, textColor: '#00ff00', items: '任务1' });
    expect(html).toContain('font-size:20px');
    expect(html).toContain('color:#00ff00');
  });

  // ---- 返回类型 ----
  it('返回的字符串应以 div 开头', () => {
    const html = renderTaskList({});
    expect(html.trim().startsWith('<div')).toBe(true);
  });
});
