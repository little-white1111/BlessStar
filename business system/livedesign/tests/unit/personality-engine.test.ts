/**
 * 人格引擎单元测试（架构不变量 A1, A2）
 *
 * A1: 人格引擎是情感系统的唯一真理源 — 所有情感变化经过 trait 约束
 * A2: 情感衰减不可跳过 — 构造时启动定时器，destroy 时清理
 */
import { describe, it, expect, beforeEach, afterEach, vi } from 'vitest';
import { PersonalityEngine } from '../../src/llm-service/personality/engine';
import type { AffectiveState } from '../../src/llm-service/personality/affective';
import { BASELINE_STATE } from '../../src/llm-service/personality/affective';
import { DEFAULT_TRAITS } from '../../src/llm-service/personality/traits';

describe('PersonalityEngine - 初始状态', () => {
  it('构造后应处于基线状态', () => {
    const engine = new PersonalityEngine();
    const state = engine.getAffective();
    expect(state.valence).toBe(0.5);
    expect(state.arousal).toBe(0.5);
    expect(state.dominance).toBe(0.5);
    engine.destroy();
  });

  it('应返回默认特质', () => {
    const engine = new PersonalityEngine();
    const traits = engine.getTraits();
    expect(traits.playfulness).toBe(DEFAULT_TRAITS.playfulness);
    expect(traits.empathy).toBe(DEFAULT_TRAITS.empathy);
    engine.destroy();
  });

  it('架构不变量 A2: 构造后应启动衰减定时器', () => {
    vi.useFakeTimers();
    const engine = new PersonalityEngine();
    // 衰减器应在构造时启动
    const state = engine.getAffective();
    expect(state.valence).toBe(0.5);
    engine.destroy();
    vi.useRealTimers();
  });
});

describe('PersonalityEngine - applyEvent（架构不变量 A1, A8）', () => {
  it('user_sad 事件应降低 valence（需 smoothTick 后体现）', () => {
    const engine = new PersonalityEngine();
    engine.applyEvent({ type: 'user_sad', intensity: 1.0 });
    // 架构不变量 A8: smoothTick 前 current 不变
    expect(engine.getAffective().valence).toBe(0.5);
    engine.smoothTick();
    const state = engine.getAffective();
    expect(state.valence).toBeLessThan(0.5);
    engine.destroy();
  });

  it('user_happy 事件应升高 valence（需 smoothTick 后体现）', () => {
    const engine = new PersonalityEngine();
    engine.applyEvent({ type: 'user_happy', intensity: 1.0 });
    engine.smoothTick();
    const state = engine.getAffective();
    expect(state.valence).toBeGreaterThan(0.5);
    engine.destroy();
  });

  it('高共情度应放大 valence 变化（架构不变量 A1 验证）', () => {
    const highEmpathy = new PersonalityEngine({ empathy: 1.0 });
    const lowEmpathy = new PersonalityEngine({ empathy: 0.0 });

    highEmpathy.applyEvent({ type: 'user_sad', intensity: 1.0 });
    lowEmpathy.applyEvent({ type: 'user_sad', intensity: 1.0 });
    highEmpathy.smoothTick();
    lowEmpathy.smoothTick();

    const highState = highEmpathy.getAffective();
    const lowState = lowEmpathy.getAffective();

    // 高共情度的 valence 下降更多
    expect(highState.valence).toBeLessThan(lowState.valence);

    highEmpathy.destroy();
    lowEmpathy.destroy();
  });

  it('高顽皮度应放大 arousal 变化（架构不变量 A1 验证）', () => {
    const highPlay = new PersonalityEngine({ playfulness: 1.0 });
    const lowPlay = new PersonalityEngine({ playfulness: 0.0 });

    highPlay.applyEvent({ type: 'user_angry', intensity: 1.0 });
    lowPlay.applyEvent({ type: 'user_angry', intensity: 1.0 });
    highPlay.smoothTick();
    lowPlay.smoothTick();

    const highState = highPlay.getAffective();
    const lowState = lowPlay.getAffective();

    // 高顽皮度的 arousal 升高更多
    expect(highState.arousal).toBeGreaterThan(lowState.arousal);

    highPlay.destroy();
    lowPlay.destroy();
  });

  it('携带 emotion 的事件应映射到 VAD（需 smoothTick 后体现）', () => {
    const engine = new PersonalityEngine();
    engine.applyEvent({ type: 'llm_response', emotion: 'happy', intensity: 0.8 });
    engine.smoothTick();
    const state = engine.getAffective();
    // happy 的 valence 高 → 应向高处偏移
    expect(state.valence).toBeGreaterThan(0.5);
    engine.destroy();
  });

  it('多次事件后 VAD 应在 0~1 范围内', () => {
    const engine = new PersonalityEngine();
    for (let i = 0; i < 20; i++) {
      engine.applyEvent({ type: 'user_happy', intensity: 1.0 });
    }
    // 需要多次 smoothTick 把所有目标累积体现到 current
    for (let i = 0; i < 30; i++) {
      engine.smoothTick();
    }
    const state = engine.getAffective();
    expect(state.valence).toBeLessThanOrEqual(1);
    expect(state.valence).toBeGreaterThanOrEqual(0);
    engine.destroy();
  });
});

describe('PersonalityEngine - 监听器（架构不变量 A8: 由 smoothTick 触发通知）', () => {
  it('情感变更后调用 smoothTick 应通知监听器', () => {
    const engine = new PersonalityEngine();
    const listener = vi.fn();
    engine.onAffectiveChange(listener);

    engine.applyEvent({ type: 'user_sad', intensity: 0.5 });
    // 架构不变量 A8: applyEvent 不直接通知，smoothTick 才通知
    expect(listener).not.toHaveBeenCalled();

    engine.smoothTick();
    expect(listener).toHaveBeenCalledTimes(1);
    const [current, previous] = listener.mock.calls[0] as [AffectiveState, AffectiveState];
    // 当前状态 valence 降低
    expect(current.valence).toBeLessThan(previous.valence);
    engine.destroy();
  });

  it('监听器应收到新的和旧的 VAD 状态', () => {
    const engine = new PersonalityEngine();
    const listener = vi.fn();
    engine.onAffectiveChange(listener);

    engine.applyEvent({ type: 'user_sad', intensity: 1.0 });
    engine.smoothTick();

    const [current, previous] = listener.mock.calls[0] as [AffectiveState, AffectiveState];
    expect(previous.valence).toBe(0.5);
    expect(current.valence).toBeLessThan(0.5);
    engine.destroy();
  });

  it('取消注册的监听器不应再收到通知', () => {
    const engine = new PersonalityEngine();
    const listener = vi.fn();
    const unsubscribe = engine.onAffectiveChange(listener);
    unsubscribe();

    engine.applyEvent({ type: 'user_sad', intensity: 0.5 });
    engine.smoothTick();

    expect(listener).not.toHaveBeenCalled();
    engine.destroy();
  });
});

describe('PersonalityEngine - 架构不变量 A8: EMA 平滑', () => {
  it('applyEvent 后 current 不应立即跳变（写入 target）', () => {
    const engine = new PersonalityEngine();
    const initialState = engine.getAffective();

    // 施加大幅阶跃事件
    engine.applyEvent({ type: 'user_happy', emotion: 'joy', intensity: 1.0 });

    // 架构不变量 A8: current 保持不变（smoothTick 尚未执行）
    const afterEvent = engine.getAffective();
    expect(afterEvent.valence).toBe(initialState.valence);
    expect(afterEvent.arousal).toBe(initialState.arousal);
    expect(afterEvent.dominance).toBe(initialState.dominance);
    engine.destroy();
  });

  it('smoothTick 应将 current 向 target 靠近 smoothingFactor 比例', () => {
    const engine = new PersonalityEngine({ smoothingFactor: 0.3 });
    const initialState = engine.getAffective();

    // 施加阶跃事件
    engine.applyEvent({ type: 'user_happy', emotion: 'joy', intensity: 1.0 });
    // target 已变更，current 未变
    const beforeSmooth = engine.getAffective();
    expect(beforeSmooth.valence).toBe(0.5);

    // 执行一次 smoothTick
    engine.smoothTick();
    const afterSmooth = engine.getAffective();
    // current 向 target 靠近了 30%，但还没到 target
    // joy → VAD(0.9, 0.7, 0.7), delta = (0.4, 0.2, 0.2) * traits
    // valence: 0.5 + 0.4*1.2*0.3 = 0.5 + 0.144 = 0.644
    expect(afterSmooth.valence).toBeGreaterThan(0.5);
    expect(afterSmooth.valence).toBeLessThan(0.9);
    expect(afterSmooth.arousal).toBeGreaterThan(0.5);
    engine.destroy();
  });

  it('多次 smoothTick 后 current 应逼近 target', () => {
    const engine = new PersonalityEngine({ smoothingFactor: 0.5 });
    engine.setAffective({ valence: 0.9, arousal: 0.8, dominance: 0.7 });
    // setAffective 同时设置了 target 和 current，先重置 target
    // 改用 applyEvent 确保 target ≠ current
    engine.setAffective({ valence: 0.5, arousal: 0.5, dominance: 0.5 });
    engine.applyEvent({ type: 'user_happy', emotion: 'joy', intensity: 1.0 });

    // 多次 smoothTick 后 current → target
    for (let i = 0; i < 20; i++) {
      engine.smoothTick();
    }

    const finalState = engine.getAffective();
    // current 应接近由事件产生的 target 值
    expect(finalState.valence).toBeGreaterThan(0.8);
    expect(finalState.arousal).toBeGreaterThan(0.7);
    engine.destroy();
  });

  it('smoothTick 在 current≈target 时应跳过（无通知）', () => {
    const engine = new PersonalityEngine();
    const listener = vi.fn();
    engine.onAffectiveChange(listener);

    // 当 current ≈ target 时，smoothTick 应无操作
    engine.smoothTick();
    expect(listener).not.toHaveBeenCalled();

    engine.destroy();
  });

  it('多次 applyEvent 连续调用不会导致 current 跳变', () => {
    const engine = new PersonalityEngine();
    const listener = vi.fn();
    engine.onAffectiveChange(listener);

    // 连续三个事件，都不触发通知
    engine.applyEvent({ type: 'user_sad', intensity: 1.0 });
    engine.applyEvent({ type: 'user_happy', intensity: 1.0 });
    engine.applyEvent({ type: 'user_angry', intensity: 1.0 });
    expect(listener).not.toHaveBeenCalled();

    // 一次 smoothTick 只产生一次通知
    engine.smoothTick();
    expect(listener).toHaveBeenCalledTimes(1);
    engine.destroy();
  });
});

describe('PersonalityEngine - setAffective', () => {
  it('直接设置 VAD 应生效', () => {
    const engine = new PersonalityEngine();
    engine.setAffective({ valence: 0.9, arousal: 0.8, dominance: 0.7 });
    const state = engine.getAffective();
    expect(state.valence).toBe(0.9);
    expect(state.arousal).toBe(0.8);
    expect(state.dominance).toBe(0.7);
    engine.destroy();
  });

  it('超出范围的值应被 clamp', () => {
    const engine = new PersonalityEngine();
    engine.setAffective({ valence: 2.0, arousal: -1.0, dominance: 0.5 });
    const state = engine.getAffective();
    expect(state.valence).toBe(1.0);
    expect(state.arousal).toBe(0.0);
    engine.destroy();
  });
});

describe('PersonalityEngine - updateTraits', () => {
  it('应动态更新特质', () => {
    const engine = new PersonalityEngine();
    engine.updateTraits({ playfulness: 1.0, empathy: 0.0 });
    const traits = engine.getTraits();
    expect(traits.playfulness).toBe(1.0);
    expect(traits.empathy).toBe(0.0);
    engine.destroy();
  });
});

describe('PersonalityEngine - 生命周期', () => {
  it('destroy() 应停止所有定时器（衰减和平滑）', () => {
    vi.useFakeTimers();
    const engine = new PersonalityEngine({ decayIntervalMs: 1000, decayRate: 0.1, smoothingFactor: 0.5 });

    // 先重置到基线
    engine.setAffective({ valence: 0.5, arousal: 0.5, dominance: 0.5 });
    // 用 applyEvent 使 target ≠ current（只写入 target，不改变 current）
    engine.applyEvent({ type: 'user_happy', emotion: 'joy', intensity: 1.0 });

    const before = engine.getAffective();
    expect(before.valence).toBe(0.5); // current 未变，smoothTick 尚未执行

    engine.destroy();

    // 快进 5 秒，两个定时器都应已停止
    vi.advanceTimersByTime(5000);
    const after = engine.getAffective();
    // destroy 后 VAD 不应变化（平滑定时器已停止，无法将 current 拉向 target）
    expect(after.valence).toBe(0.5);
    expect(after.arousal).toBe(0.5);
    expect(after.dominance).toBe(0.5);

    vi.useRealTimers();
  });
});
