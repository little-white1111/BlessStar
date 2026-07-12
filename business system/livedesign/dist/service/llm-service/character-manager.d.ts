/**
 * 角色卡管理（架构不变量 #7 — 角色卡是角色扮演模式的唯一真理源）
 * 加载角色卡 JSON，动态生成 system prompt，管理角色切换
 */
import { CharacterCard } from '../shared/character-card';
/** 角色切换回调 */
export type CharacterSwitchCallback = (card: CharacterCard) => void;
/** 角色卡管理器 */
export declare class CharacterManager {
    private currentCard;
    private cardsDir;
    private onSwitchCallbacks;
    constructor(cardsDir: string);
    /** 注册角色切换回调 */
    onSwitch(callback: CharacterSwitchCallback): void;
    /** 获取当前角色卡 */
    getCurrentCard(): CharacterCard | null;
    /** 按角色名加载角色卡 */
    loadByName(name: string): CharacterCard;
    /** 从文件系统加载角色卡 JSON */
    loadFromFile(filePath: string): CharacterCard;
    /** 动态生成 system prompt（架构不变量 #7 — 完全基于角色卡内容） */
    generateSystemPrompt(): string;
    /** 获取角色卡目录下所有可用角色名 */
    listAvailableCards(): string[];
    /** 重置（清空当前角色）— 架构不变量 #12 */
    reset(): void;
}
//# sourceMappingURL=character-manager.d.ts.map