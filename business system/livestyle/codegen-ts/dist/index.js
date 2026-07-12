/**
 * @blessstar/codegen-ts — 从 config-schema.yaml 生成 TypeScript 类型/Port/Adapter
 * 遵循 I15：config-schema.yaml 为 Codegen 唯一真理源
 * 遵循 I16：Codegen 输出必须通过版本管理
 */
import * as yaml from 'js-yaml';
/** 从 config-schema.yaml 解析 Schema */
export function parseSchema(yamlContent) {
    const raw = yaml.load(yamlContent);
    if (!raw || !raw.fields) {
        throw new Error('无效的 config-schema.yaml：缺少 fields 字段');
    }
    return {
        domain: raw.domain ?? 'unknown',
        version: raw.version ?? 'v0.0.0',
        fields: raw.fields.map((f, idx) => ({
            id: idx + 1,
            key: f.key,
            type: f.type,
            default: f.default,
            required: f.required ?? false,
            business_desc: f.business_desc ?? f.description ?? '',
            contract: f.contract,
            ui_meta: f.ui_meta,
            ai_hint: f.ai_hint,
            search_keywords: f.search_keywords,
        })),
        x_extensions: raw['x-extensions'],
    };
}
// ─── 类型映射 ───────────────────────────────────────────────
const TYPE_MAP = {
    I32: 'number',
    BOOL: 'boolean',
    STR: 'string',
    SECRET: 'string',
};
function mapType(rawType) {
    return TYPE_MAP[rawType] ?? 'unknown';
}
// ─── 生成文件头 ────────────────────────────────────────────
function buildHeader(sourceFile) {
    return [
        '/**',
        ' * 此文件由 @blessstar/codegen-ts 自动生成',
        ` * 源文件: ${sourceFile}`,
        ' * 生成时间: 2026-07-11',
        ' * 请勿手动编辑 — 下次运行 codegen 时将覆盖',
        ' */',
        '',
    ].join('\n');
}
// ─── 生成 TypeScript 类型 ──────────────────────────────────
/** 生成 TypeScript 类型定义字符串 */
export function generateTypes(schema, sourceFile = 'config-schema.yaml') {
    const lines = [];
    lines.push(buildHeader(sourceFile));
    lines.push('// ─── Livestyle 全局配置类型 ───────────────────────────');
    lines.push('');
    lines.push('/**');
    lines.push(` * ${schema.domain} 领域全部配置项`);
    lines.push(` * Schema 版本: ${schema.version}`);
    lines.push(` * 字段总数: ${schema.fields.length}`);
    lines.push(' */');
    lines.push(`export interface LivestyleConfig {`);
    for (const field of schema.fields) {
        const tsType = mapType(field.type);
        const optional = field.required ? '' : '?';
        const defaultStr = formatDefaultValue(field.default, tsType);
        const comment = field.business_desc.replace(/\n/g, ' ');
        lines.push('');
        lines.push(`  /** ${comment} */`);
        if (defaultStr) {
            lines.push(`  /** @default ${defaultStr} */`);
        }
        // Use quoted key for dotted paths
        const propKey = field.key.includes('.') ? `'${field.key}'` : field.key;
        lines.push(`  ${propKey}${optional}: ${tsType};`);
    }
    lines.push('}');
    lines.push('');
    // Also generate per-module sub-types for convenience
    const modules = groupByModule(schema.fields);
    for (const [moduleName, moduleFields] of Object.entries(modules)) {
        const ifaceName = toPascalCase(moduleName) + 'Config';
        lines.push('');
        lines.push(`/** ${moduleName} 模块配置 */`);
        lines.push(`export interface ${ifaceName} {`);
        for (const field of moduleFields) {
            const tsType = mapType(field.type);
            const optional = field.required ? '' : '?';
            const localKey = field.key.replace(`${moduleName}.`, '');
            lines.push(`  /** ${field.business_desc.replace(/\n/g, ' ')} */`);
            lines.push(`  ${localKey}${optional}: ${tsType};`);
        }
        lines.push('}');
    }
    lines.push('');
    return lines.join('\n');
}
// ─── 生成 Port 接口 ────────────────────────────────────────
/** 生成 Port 接口（I2 ConfigPort） */
export function generatePort(schema, sourceFile = 'config-schema.yaml') {
    const lines = [];
    lines.push(buildHeader(sourceFile));
    lines.push('import type { LivestyleConfig } from \'./livestyle-config.types\';');
    lines.push('');
    lines.push('/**');
    lines.push(' * ConfigPort — 配置端口接口');
    lines.push(' * 遵循 I2：通过 Port 抽象解耦配置来源');
    lines.push(' */');
    lines.push('export interface ConfigPort {');
    lines.push('  /**');
    lines.push('   * 获取指定 key 的配置值');
    lines.push('   * @param key - 配置键路径，如 "canvas.grid.size"');
    lines.push('   */');
    lines.push('  get<K extends keyof LivestyleConfig>(key: K): LivestyleConfig[K];');
    lines.push('');
    lines.push('  /**');
    lines.push('   * 批量获取配置快照');
    lines.push('   */');
    lines.push('  getAll(): LivestyleConfig;');
    lines.push('');
    lines.push('  /**');
    lines.push('   * 设置配置值');
    lines.push('   * @param key - 配置键路径');
    lines.push('   * @param value - 配置值');
    lines.push('   */');
    lines.push('  set<K extends keyof LivestyleConfig>(key: K, value: LivestyleConfig[K]): void;');
    lines.push('');
    lines.push('  /**');
    lines.push('   * 订阅配置变更');
    lines.push('   * @param key - 配置键路径（可选，不传则监听所有变更）');
    lines.push('   * @param listener - 变更回调');
    lines.push('   * @returns 取消订阅函数');
    lines.push('   */');
    lines.push(`  subscribe<K extends keyof LivestyleConfig>(`);
    lines.push(`    key: K | '*',`);
    lines.push('    listener: (newValue: LivestyleConfig[K], oldValue: LivestyleConfig[K]) => void,');
    lines.push('  ): () => void;');
    lines.push('}');
    lines.push('');
    return lines.join('\n');
}
// ─── 生成 Mock Adapter ─────────────────────────────────────
/** 生成 Mock Adapter */
export function generateMockAdapter(schema, sourceFile = 'config-schema.yaml') {
    const lines = [];
    lines.push(buildHeader(sourceFile));
    lines.push(`import type { LivestyleConfig } from './livestyle-config.types';`);
    lines.push(`import type { ConfigPort } from './livestyle-config.port';`);
    lines.push('');
    lines.push('/**');
    lines.push(' * MockConfigAdapter — 测试用 Mock 实现');
    lines.push(' * 所有配置项使用 config-schema.yaml 中定义的默认值');
    lines.push(' */');
    lines.push('export class MockConfigAdapter implements ConfigPort {');
    lines.push('  private config: LivestyleConfig;');
    lines.push('  private listeners: Map<string, Array<(newVal: unknown, oldVal: unknown) => void>>;');
    lines.push('');
    lines.push('  constructor() {');
    lines.push('    this.listeners = new Map();');
    lines.push('    this.config = this.createDefaultConfig();');
    lines.push('  }');
    lines.push('');
    // createDefaultConfig method
    lines.push('  private createDefaultConfig(): LivestyleConfig {');
    lines.push('    return {');
    for (const field of schema.fields) {
        const propKey = field.key.includes('.') ? `'${field.key}'` : field.key;
        const defaultVal = formatDefaultValueLiteral(field.default, field.type);
        lines.push(`      ${propKey}: ${defaultVal},`);
    }
    lines.push('    };');
    lines.push('  }');
    lines.push('');
    // get method
    lines.push('  get<K extends keyof LivestyleConfig>(key: K): LivestyleConfig[K] {');
    lines.push('    return this.config[key];');
    lines.push('  }');
    lines.push('');
    // getAll method
    lines.push('  getAll(): LivestyleConfig {');
    lines.push('    return { ...this.config };');
    lines.push('  }');
    lines.push('');
    // set method
    lines.push('  set<K extends keyof LivestyleConfig>(key: K, value: LivestyleConfig[K]): void {');
    lines.push('    const oldValue = this.config[key];');
    lines.push('    this.config[key] = value;');
    lines.push('    this.notify(key as string, value, oldValue);');
    lines.push('  }');
    lines.push('');
    // subscribe method
    lines.push('  subscribe<K extends keyof LivestyleConfig>(');
    lines.push('    key: K | \'*\',');
    lines.push('    listener: (newValue: LivestyleConfig[K], oldValue: LivestyleConfig[K]) => void,');
    lines.push('  ): () => void {');
    lines.push('    const key_ = key as string;');
    lines.push('    if (!this.listeners.has(key_)) {');
    lines.push('      this.listeners.set(key_, []);');
    lines.push('    }');
    lines.push('    this.listeners.get(key_)!.push(listener as (newVal: unknown, oldVal: unknown) => void);');
    lines.push('    return () => {');
    lines.push('      const arr = this.listeners.get(key_);');
    lines.push('      if (arr) {');
    lines.push('        const idx = arr.indexOf(listener as (newVal: unknown, oldVal: unknown) => void);');
    lines.push('        if (idx >= 0) arr.splice(idx, 1);');
    lines.push('      }');
    lines.push('    };');
    lines.push('  }');
    lines.push('');
    // private notify
    lines.push('  private notify(key: string, newValue: unknown, oldValue: unknown): void {');
    lines.push('    const exact = this.listeners.get(key);');
    lines.push('    if (exact) {');
    lines.push('      exact.forEach(fn => fn(newValue, oldValue));');
    lines.push('    }');
    lines.push('    const wildcard = this.listeners.get(\'*\');');
    lines.push('    if (wildcard) {');
    lines.push('      wildcard.forEach(fn => fn(newValue, oldValue));');
    lines.push('    }');
    lines.push('  }');
    lines.push('}');
    lines.push('');
    return lines.join('\n');
}
// ─── 生成 LocalFileAdapter ─────────────────────────────────
/** 生成 LocalFileAdapter */
export function generateLocalFileAdapter(schema, sourceFile = 'config-schema.yaml') {
    const lines = [];
    lines.push(buildHeader(sourceFile));
    lines.push(`import * as fs from 'fs';`);
    lines.push(`import * as path from 'path';`);
    lines.push(`import type { LivestyleConfig } from './livestyle-config.types';`);
    lines.push(`import type { ConfigPort } from './livestyle-config.port';`);
    lines.push('');
    lines.push('/**');
    lines.push(' * LocalFileConfigAdapter — 基于本地 JSON 文件的配置适配器');
    lines.push(' * 配置存储路径可通过构造函数参数指定，默认在 process.cwd() 下');
    lines.push(' */');
    lines.push('export class LocalFileConfigAdapter implements ConfigPort {');
    lines.push('  private config: LivestyleConfig;');
    lines.push('  private filePath: string;');
    lines.push('  private listeners: Map<string, Array<(newVal: unknown, oldVal: unknown) => void>>;');
    lines.push('');
    lines.push('  constructor(filePath?: string) {');
    lines.push('    this.listeners = new Map();');
    lines.push('    this.filePath = filePath ?? path.resolve(process.cwd(), \'livestyle-config.json\');');
    lines.push('    this.config = this.loadFromFile();');
    lines.push('  }');
    lines.push('');
    // createDefaultConfig
    lines.push('  private createDefaultConfig(): LivestyleConfig {');
    lines.push('    return {');
    for (const field of schema.fields) {
        const propKey = field.key.includes('.') ? `'${field.key}'` : field.key;
        const defaultVal = formatDefaultValueLiteral(field.default, field.type);
        lines.push(`      ${propKey}: ${defaultVal},`);
    }
    lines.push('    };');
    lines.push('  }');
    lines.push('');
    // loadFromFile
    lines.push('  private loadFromFile(): LivestyleConfig {');
    lines.push('    try {');
    lines.push('      if (fs.existsSync(this.filePath)) {');
    lines.push('        const raw = fs.readFileSync(this.filePath, \'utf-8\');');
    lines.push('        const parsed = JSON.parse(raw);');
    lines.push('        return { ...this.createDefaultConfig(), ...parsed };');
    lines.push('      }');
    lines.push('    } catch {');
    lines.push('      // 文件读取失败时使用默认值');
    lines.push('    }');
    lines.push('    return this.createDefaultConfig();');
    lines.push('  }');
    lines.push('');
    // persist
    lines.push('  private persist(): void {');
    lines.push('    try {');
    lines.push('      const dir = path.dirname(this.filePath);');
    lines.push('      if (!fs.existsSync(dir)) {');
    lines.push('        fs.mkdirSync(dir, { recursive: true });');
    lines.push('      }');
    lines.push('      fs.writeFileSync(this.filePath, JSON.stringify(this.config, null, 2), \'utf-8\');');
    lines.push('    } catch (err) {');
    lines.push('      console.error(\'[LocalFileConfigAdapter] 配置持久化失败:\', err);');
    lines.push('    }');
    lines.push('  }');
    lines.push('');
    // get
    lines.push('  get<K extends keyof LivestyleConfig>(key: K): LivestyleConfig[K] {');
    lines.push('    return this.config[key];');
    lines.push('  }');
    lines.push('');
    // getAll
    lines.push('  getAll(): LivestyleConfig {');
    lines.push('    return { ...this.config };');
    lines.push('  }');
    lines.push('');
    // set
    lines.push('  set<K extends keyof LivestyleConfig>(key: K, value: LivestyleConfig[K]): void {');
    lines.push('    const oldValue = this.config[key];');
    lines.push('    this.config[key] = value;');
    lines.push('    this.persist();');
    lines.push('    this.notify(key as string, value, oldValue);');
    lines.push('  }');
    lines.push('');
    // subscribe
    lines.push('  subscribe<K extends keyof LivestyleConfig>(');
    lines.push('    key: K | \'*\',');
    lines.push('    listener: (newValue: LivestyleConfig[K], oldValue: LivestyleConfig[K]) => void,');
    lines.push('  ): () => void {');
    lines.push('    const key_ = key as string;');
    lines.push('    if (!this.listeners.has(key_)) {');
    lines.push('      this.listeners.set(key_, []);');
    lines.push('    }');
    lines.push('    this.listeners.get(key_)!.push(listener as (newVal: unknown, oldVal: unknown) => void);');
    lines.push('    return () => {');
    lines.push('      const arr = this.listeners.get(key_);');
    lines.push('      if (arr) {');
    lines.push('        const idx = arr.indexOf(listener as (newVal: unknown, oldVal: unknown) => void);');
    lines.push('        if (idx >= 0) arr.splice(idx, 1);');
    lines.push('      }');
    lines.push('    };');
    lines.push('  }');
    lines.push('');
    // notify
    lines.push('  private notify(key: string, newValue: unknown, oldValue: unknown): void {');
    lines.push('    const exact = this.listeners.get(key);');
    lines.push('    if (exact) {');
    lines.push('      exact.forEach(fn => fn(newValue, oldValue));');
    lines.push('    }');
    lines.push('    const wildcard = this.listeners.get(\'*\');');
    lines.push('    if (wildcard) {');
    lines.push('      wildcard.forEach(fn => fn(newValue, oldValue));');
    lines.push('    }');
    lines.push('  }');
    lines.push('}');
    lines.push('');
    return lines.join('\n');
}
// ─── 辅助函数 ───────────────────────────────────────────────
function formatDefaultValue(value, tsType) {
    if (value === undefined || value === null)
        return '';
    if (tsType === 'string')
        return `'${String(value)}'`;
    return String(value);
}
function formatDefaultValueLiteral(value, rawType) {
    if (value === undefined || value === null) {
        if (rawType === 'BOOL')
            return 'false';
        if (rawType === 'I32')
            return '0';
        return "''";
    }
    if (typeof value === 'string')
        return `'${value}'`;
    return String(value);
}
function groupByModule(fields) {
    const groups = {};
    for (const field of fields) {
        // Extract module prefix: "canvas.grid.size" → "canvas"
        // For component fields: "component.task-list.title" → "component.task-list"
        const parts = field.key.split('.');
        let moduleName;
        if (parts[0] === 'component' && parts.length >= 3) {
            moduleName = parts.slice(0, 2).join('.');
        }
        else {
            moduleName = parts[0];
        }
        if (!groups[moduleName]) {
            groups[moduleName] = [];
        }
        groups[moduleName].push(field);
    }
    return groups;
}
function toPascalCase(str) {
    return str
        .split(/[.\-_]/)
        .map(part => part.charAt(0).toUpperCase() + part.slice(1))
        .join('');
}
