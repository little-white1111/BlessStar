/**
 * 角色卡管理（架构不变量 #7 — 角色卡是角色扮演模式的唯一真理源）
 * 加载角色卡 JSON，动态生成 system prompt，管理角色切换
 */

import * as fs from 'fs';
import * as path from 'path';
import { CharacterCard, validateCharacterCard } from '../shared/character-card';

/** 角色切换回调 */
export type CharacterSwitchCallback = (card: CharacterCard) => void;

/** 角色卡管理器 */
export class CharacterManager {
  private currentCard: CharacterCard | null = null;
  private cardsDir: string;
  private onSwitchCallbacks: CharacterSwitchCallback[] = [];

  constructor(cardsDir: string) {
    this.cardsDir = cardsDir;
  }

  /** 注册角色切换回调 */
  onSwitch(callback: CharacterSwitchCallback): void {
    this.onSwitchCallbacks.push(callback);
  }

  /** 获取当前角色卡 */
  getCurrentCard(): CharacterCard | null {
    return this.currentCard;
  }

  /** 按角色名加载角色卡 */
  loadByName(name: string): CharacterCard {
    const filePath = path.join(this.cardsDir, `${name}.json`);
    return this.loadFromFile(filePath);
  }

  /** 从文件系统加载角色卡 JSON */
  loadFromFile(filePath: string): CharacterCard {
    const raw = fs.readFileSync(filePath, 'utf-8');
    const parsed: unknown = JSON.parse(raw);

    const validation = validateCharacterCard(parsed);
    if (!validation.valid) {
      throw new Error(
        `角色卡校验失败 (${filePath}): ${validation.errors.join('; ')}`
      );
    }

    const card = parsed as CharacterCard;
    this.currentCard = card;

    // 通知所有切换回调
    for (const cb of this.onSwitchCallbacks) {
      cb(card);
    }

    return card;
  }

  /** 动态生成 system prompt（架构不变量 #7 — 完全基于角色卡内容） */
  generateSystemPrompt(): string {
    if (!this.currentCard) {
      throw new Error('未加载角色卡，无法生成 system prompt');
    }

    const { name, description, style, background, knowledge, examples } =
      this.currentCard;

    const lines: string[] = [];

    lines.push(`# 角色设定`);
    lines.push(`你是 ${name}。`);
    lines.push(``);
    lines.push(`## 人格描述`);
    lines.push(description);
    lines.push(``);

    if (style) {
      lines.push(`## 说话风格`);
      lines.push(style);
      lines.push(``);
    }

    if (background) {
      lines.push(`## 背景故事`);
      lines.push(background);
      lines.push(``);
    }

    if (knowledge && knowledge.length > 0) {
      lines.push(`## 知识领域`);
      for (const k of knowledge) {
        lines.push(`- ${k}`);
      }
      lines.push(``);
    }

    if (examples && examples.length > 0) {
      lines.push(`## 示例对话`);
      for (const ex of examples) {
        lines.push(`用户: ${ex.user}`);
        lines.push(`${name}: ${ex.assistant}`);
        if (ex.emotion) {
          lines.push(`(情绪: ${ex.emotion})`);
        }
        lines.push(``);
      }
    }

    // 输出格式指令
    lines.push(`## 输出格式`);
    lines.push(`请以 JSON 格式回复：`);
    lines.push(`{"text": "你的回复内容", "emotion": "高兴|悲伤|愤怒|惊讶|平静|困惑|焦虑|害羞|兴奋|疲倦", "action": "wave|nod|shake|smile|think|idle|blush|sigh|cheer|yawn"}`);
    lines.push(``);
    lines.push(`请始终以 ${name} 的身份和语气进行对话。`);

    return lines.join('\n');
  }

  /** 获取角色卡目录下所有可用角色名 */
  listAvailableCards(): string[] {
    if (!fs.existsSync(this.cardsDir)) {
      return [];
    }
    const files = fs.readdirSync(this.cardsDir);
    return files
      .filter((f) => f.endsWith('.json'))
      .map((f) => path.basename(f, '.json'));
  }

  /** 重置（清空当前角色）— 架构不变量 #12 */
  reset(): void {
    this.currentCard = null;
  }
}
