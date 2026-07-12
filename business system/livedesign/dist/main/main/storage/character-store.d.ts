/**
 * 角色卡存储
 *
 * 职责：
 * - 从文件系统读取角色卡 JSON
 * - 使用 validateCharacterCard 校验完整性
 * - 列出 / 切换角色
 * - 角色卡是角色扮演模式的唯一真理源（架构不变量 #7）
 */
import { CharacterCard, CharacterCardValidation } from '../../shared/character-card';
/** 角色卡存储配置 */
export interface CharacterStoreOptions {
    /** 角色卡目录（默认在 resources/characters/） */
    charactersDir?: string;
}
/** 角色卡列表项 */
export interface CharacterListItem {
    id: string;
    name: string;
    description: string;
    style?: string;
    modelPath?: string;
    valid: boolean;
}
export declare class CharacterStore {
    private charactersDir;
    private cache;
    constructor(options?: CharacterStoreOptions);
    /**
     * 获取默认角色卡目录
     */
    private getDefaultCharactersDir;
    /**
     * 确保角色卡目录存在
     */
    private ensureCharactersDir;
    /**
     * 列出所有可用的角色卡
     */
    listCharacters(): CharacterListItem[];
    /**
     * 从文件加载角色卡
     */
    private loadCardFromFile;
    /**
     * 获取指定角色卡
     */
    getCharacter(characterId: string): CharacterCard | null;
    /**
     * 校验角色卡完整性
     */
    validateCharacter(characterId: string): CharacterCardValidation;
    /**
     * 切换当前角色
     * 架构不变量 #7：角色卡是角色扮演模式的唯一真理源
     * 架构不变量 #12：角色切换时清空上下文（由调用方触发 dialogStore.clearCharacterContext）
     */
    switchCharacter(characterId: string): CharacterCard | null;
    /**
     * 获取当前角色
     */
    getCurrentCharacter(): CharacterCard | null;
    /**
     * 获取当前角色 ID
     */
    getCurrentCharacterId(): string | null;
    /**
     * 刷新角色卡缓存
     */
    refreshCache(): void;
    /**
     * 创建默认的角色卡示例文件（如果目录为空）
     */
    createDefaultCharacterIfEmpty(): void;
    /**
     * 获取角色卡存储目录路径
     */
    getCharactersDir(): string;
}
/** 全局单例 */
export declare const characterStore: CharacterStore;
//# sourceMappingURL=character-store.d.ts.map