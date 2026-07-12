"use strict";
/**
 * 角色卡存储
 *
 * 职责：
 * - 从文件系统读取角色卡 JSON
 * - 使用 validateCharacterCard 校验完整性
 * - 列出 / 切换角色
 * - 角色卡是角色扮演模式的唯一真理源（架构不变量 #7）
 */
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.characterStore = exports.CharacterStore = void 0;
const fs = __importStar(require("fs"));
const path = __importStar(require("path"));
const electron_1 = require("electron");
const character_card_1 = require("../../shared/character-card");
const config_store_1 = require("./config-store");
class CharacterStore {
    charactersDir;
    cache = new Map();
    constructor(options = {}) {
        // 默认为 resources/characters/ 目录
        this.charactersDir = options.charactersDir || this.getDefaultCharactersDir();
        this.ensureCharactersDir();
    }
    /**
     * 获取默认角色卡目录
     */
    getDefaultCharactersDir() {
        // 开发模式：resources/characters/
        // 生产模式：process.resourcesPath + '/resources/characters/'
        if (process.env.NODE_ENV === 'development' || !electron_1.app.isPackaged) {
            return path.join(__dirname, '..', '..', '..', 'resources', 'characters');
        }
        return path.join(process.resourcesPath, 'resources', 'characters');
    }
    /**
     * 确保角色卡目录存在
     */
    ensureCharactersDir() {
        if (!fs.existsSync(this.charactersDir)) {
            try {
                fs.mkdirSync(this.charactersDir, { recursive: true });
                console.log(`[CharacterStore] 创建角色卡目录: ${this.charactersDir}`);
            }
            catch (err) {
                console.error(`[CharacterStore] 创建角色卡目录失败:`, err);
            }
        }
    }
    /**
     * 列出所有可用的角色卡
     */
    listCharacters() {
        const items = [];
        try {
            if (!fs.existsSync(this.charactersDir)) {
                console.warn(`[CharacterStore] 角色卡目录不存在: ${this.charactersDir}`);
                return items;
            }
            const files = fs.readdirSync(this.charactersDir);
            const jsonFiles = files.filter((f) => f.endsWith('.json'));
            for (const file of jsonFiles) {
                const filePath = path.join(this.charactersDir, file);
                const characterId = path.basename(file, '.json');
                try {
                    const card = this.loadCardFromFile(filePath);
                    const validation = (0, character_card_1.validateCharacterCard)(card);
                    items.push({
                        id: characterId,
                        name: card.name,
                        description: card.description,
                        style: card.style,
                        modelPath: card.modelPath,
                        valid: validation.valid,
                    });
                    // 缓存有效的角色卡
                    if (validation.valid) {
                        this.cache.set(characterId, card);
                    }
                }
                catch (err) {
                    console.warn(`[CharacterStore] 加载角色卡失败: ${file}`, err);
                    items.push({
                        id: characterId,
                        name: path.basename(file, '.json'),
                        description: '（加载失败）',
                        valid: false,
                    });
                }
            }
        }
        catch (err) {
            console.error(`[CharacterStore] 列出角色卡失败:`, err);
        }
        return items;
    }
    /**
     * 从文件加载角色卡
     */
    loadCardFromFile(filePath) {
        const raw = fs.readFileSync(filePath, 'utf-8');
        const parsed = JSON.parse(raw);
        return parsed;
    }
    /**
     * 获取指定角色卡
     */
    getCharacter(characterId) {
        // 先从缓存中查找
        const cached = this.cache.get(characterId);
        if (cached)
            return cached;
        // 从文件系统加载
        const filePath = path.join(this.charactersDir, `${characterId}.json`);
        if (!fs.existsSync(filePath)) {
            console.warn(`[CharacterStore] 角色卡文件不存在: ${filePath}`);
            return null;
        }
        try {
            const card = this.loadCardFromFile(filePath);
            const validation = (0, character_card_1.validateCharacterCard)(card);
            if (!validation.valid) {
                console.warn(`[CharacterStore] 角色卡校验失败: ${characterId}`, validation.errors);
                // 即使校验失败也返回卡片，由调用方处理
            }
            // 缓存
            this.cache.set(characterId, card);
            return card;
        }
        catch (err) {
            console.error(`[CharacterStore] 读取角色卡失败: ${characterId}`, err);
            return null;
        }
    }
    /**
     * 校验角色卡完整性
     */
    validateCharacter(characterId) {
        const card = this.getCharacter(characterId);
        if (!card) {
            return {
                valid: false,
                errors: [`角色卡不存在: ${characterId}`],
                warnings: [],
            };
        }
        return (0, character_card_1.validateCharacterCard)(card);
    }
    /**
     * 切换当前角色
     * 架构不变量 #7：角色卡是角色扮演模式的唯一真理源
     * 架构不变量 #12：角色切换时清空上下文（由调用方触发 dialogStore.clearCharacterContext）
     */
    switchCharacter(characterId) {
        const card = this.getCharacter(characterId);
        if (!card) {
            console.error(`[CharacterStore] 切换角色失败，角色卡不存在: ${characterId}`);
            return null;
        }
        // 校验角色卡完整性
        const validation = (0, character_card_1.validateCharacterCard)(card);
        if (!validation.valid) {
            console.error(`[CharacterStore] 切换角色失败，角色卡校验不通过: ${characterId}`, validation.errors);
            return null;
        }
        // 持久化当前角色（架构不变量 #5）
        config_store_1.configStore.set('character.current', characterId);
        console.log(`[CharacterStore] 切换到角色: ${card.name} (${characterId})`);
        return card;
    }
    /**
     * 获取当前角色
     */
    getCurrentCharacter() {
        const currentId = config_store_1.configStore.get('character.current');
        if (!currentId)
            return null;
        return this.getCharacter(currentId);
    }
    /**
     * 获取当前角色 ID
     */
    getCurrentCharacterId() {
        return config_store_1.configStore.get('character.current') ?? null;
    }
    /**
     * 刷新角色卡缓存
     */
    refreshCache() {
        this.cache.clear();
        this.listCharacters(); // 重新加载并填充缓存
    }
    /**
     * 创建默认的角色卡示例文件（如果目录为空）
     */
    createDefaultCharacterIfEmpty() {
        if (!fs.existsSync(this.charactersDir))
            return;
        const files = fs.readdirSync(this.charactersDir).filter((f) => f.endsWith('.json'));
        if (files.length > 0)
            return; // 已有角色卡
        // 创建一个默认角色卡
        const defaultCard = {
            name: '助手',
            description: '一个友好而乐于助人的 AI 助手',
            style: '温和、专业、细致',
            background: '你是一个全能的 AI 助手，擅长帮助用户解决各种问题。',
            knowledge: ['通用知识', '编程', '写作'],
            examples: [
                {
                    user: '你好！',
                    assistant: '你好！我是你的 AI 助手，有什么可以帮助你的吗？',
                    emotion: 'happy',
                },
            ],
            modelPath: 'default/model.json',
        };
        const filePath = path.join(this.charactersDir, 'assistant.json');
        try {
            fs.writeFileSync(filePath, JSON.stringify(defaultCard, null, 2), 'utf-8');
            console.log(`[CharacterStore] 已创建默认角色卡: ${filePath}`);
        }
        catch (err) {
            console.error(`[CharacterStore] 创建默认角色卡失败:`, err);
        }
    }
    /**
     * 获取角色卡存储目录路径
     */
    getCharactersDir() {
        return this.charactersDir;
    }
}
exports.CharacterStore = CharacterStore;
/** 全局单例 */
exports.characterStore = new CharacterStore();
//# sourceMappingURL=character-store.js.map