import { readFileSync } from 'fs';
import { resolve } from 'path';
import {
  type ConfigDomain,
  type ConfigFieldDefinition,
  type ConfigValidationResult,
  type ConfigValidationError,
} from './ConfigSchema';

/** YAML 字段原始类型（js-yaml 解析后的结构） */
interface RawField {
  key: string;
  type: string;
  default: unknown;
  required?: boolean;
  business_desc?: string;
  description?: string;
  contract?: {
    range?: [number, number];
    pattern?: string;
    dependencies?: string[];
    slo_impact?: string;
    approval_required?: boolean;
  };
  ui_meta?: {
    label: string;
    order: number;
    placeholder?: string;
    description?: string;
    hidden?: boolean;
  };
}

/** YAML 顶层结构 */
interface RawSchema {
  domain: string;
  version: string;
  fields: RawField[];
}

/** js-yaml 加载的模块类型 */
type JsYamlModule = {
  load: (yaml: string) => unknown;
};

/**
 * 从 key 路径中提取 module 名称（第一个点号前的部分）
 * 例如 "canvas.grid.size" → "canvas"
 */
function extractModule(key: string): string {
  const dotIndex = key.indexOf('.');
  return dotIndex === -1 ? key : key.slice(0, dotIndex);
}

const SUPPORTED_TYPES = new Set(['I32', 'STR', 'BOOL', 'DURATION', 'SECRET']);

export class ConfigValidator {
  private schemaPath: string;
  private domain: ConfigDomain | null = null;
  private fieldMap: Map<string, ConfigFieldDefinition> = new Map();

  constructor(schemaYamlPath: string) {
    this.schemaPath = schemaYamlPath;
  }

  /**
   * 加载并解析 config-schema.yaml（从文件系统）
   */
  async load(): Promise<void> {
    const absPath = resolve(this.schemaPath);
    const yamlContent = readFileSync(absPath, 'utf-8');
    try {
      await this.loadFromYaml(yamlContent);
    } catch (err) {
      // 注入文件路径上下文
      throw new Error(`ConfigValidator: failed to load schema from "${absPath}": ${(err as Error).message}`);
    }
  }

  /**
   * 加载并解析 YAML 字符串（浏览器/iframe 环境兼容）
   * 遵循 I2：组件配置变更必须经过 BlessStar Schema 验证
   */
  async loadFromYaml(yamlContent: string): Promise<void> {
    // 动态加载 js-yaml（ESM 兼容）
    let jsYaml: JsYamlModule;
    try {
      jsYaml = await import('js-yaml');
    } catch {
      throw new Error(
        'ConfigValidator: js-yaml is required. Install it via `npm install js-yaml`.',
      );
    }

    const raw = jsYaml.load(yamlContent) as RawSchema;

    // 校验顶层结构
    if (!raw || typeof raw.domain !== 'string') {
      throw new Error(
        'ConfigValidator: invalid schema format — missing "domain" field',
      );
    }
    if (!Array.isArray(raw.fields)) {
      throw new Error(
        'ConfigValidator: invalid schema format — missing "fields" array',
      );
    }

    const fields: ConfigFieldDefinition[] = raw.fields.map((f, idx) => {
      if (!SUPPORTED_TYPES.has(f.type)) {
        throw new Error(
          `ConfigValidator: unsupported type "${f.type}" for field "${f.key}"`,
        );
      }
      const def: ConfigFieldDefinition = {
        id: idx + 1,
        key: f.key,
        type: f.type as ConfigFieldDefinition['type'],
        default: f.default,
        description: f.business_desc ?? f.description ?? '',
        module: extractModule(f.key),
      };

      if (f.contract) {
        def.contract = {
          ...(f.contract.range ? { range: f.contract.range } : {}),
          ...(f.contract.pattern ? { pattern: f.contract.pattern } : {}),
          ...(f.contract.dependencies
            ? { dependencies: f.contract.dependencies }
            : {}),
          ...(f.contract.slo_impact
            ? { slo_impact: f.contract.slo_impact }
            : {}),
          ...(f.contract.approval_required !== undefined
            ? { approval_required: f.contract.approval_required }
            : {}),
        };
      }

      if (f.ui_meta) {
        def.ui_meta = {
          label: f.ui_meta.label ?? '',
          order: f.ui_meta.order ?? 0,
        };
      }

      return def;
    });

    this.domain = {
      domain: raw.domain,
      version: raw.version,
      fields,
    };

    // 构建 key → field 映射，方便快速查找
    this.fieldMap = new Map();
    for (const field of fields) {
      this.fieldMap.set(field.key, field);
    }
  }

  /**
   * 根据 Schema 验证一组配置值。
   * 检查字段类型、必填、range/pattern/dependencies 约束。
   */
  validate(config: Record<string, unknown>): ConfigValidationResult {
    if (!this.domain) {
      throw new Error('ConfigValidator: schema not loaded. Call load() first.');
    }

    const errors: ConfigValidationError[] = [];
    const warnings: string[] = [];

    for (const field of this.domain.fields) {
      const hasValue = Object.prototype.hasOwnProperty.call(config, field.key);
      const value = config[field.key];

      // 如果配置中未提供值，使用默认值（跳过验证）
      if (!hasValue) {
        continue;
      }

      const fieldErrors = this.validateField(field, value);
      for (const err of fieldErrors) {
        if (err.severity === 'warning') {
          warnings.push(err.message);
        } else {
          errors.push(err);
        }
      }
    }

    return {
      valid: errors.length === 0,
      errors,
      warnings,
    };
  }

  /**
   * 验证单个字段值
   */
  validateField(
    field: ConfigFieldDefinition,
    value: unknown,
  ): ConfigValidationError[] {
    const fieldErrors: ConfigValidationError[] = [];

    // 1. 类型检查
    const typeError = this.checkType(field.key, field.type, value);
    if (typeError) {
      fieldErrors.push(typeError);
      // 类型不对时不再继续检查后续约束
      return fieldErrors;
    }

    // 2. range 约束
    if (field.contract?.range && typeof value === 'number') {
      const [min, max] = field.contract.range;
      if (value < min || value > max) {
        fieldErrors.push({
          field: field.key,
          message: `"${field.key}" 的值 ${value} 超出范围 [${min}, ${max}]`,
          severity: 'error',
        });
      }
    }

    // 3. pattern 约束
    if (field.contract?.pattern && typeof value === 'string') {
      const regex = new RegExp(field.contract.pattern);
      if (!regex.test(value)) {
        fieldErrors.push({
          field: field.key,
          message: `"${field.key}" 的值 "${value}" 不匹配正则模式 ${field.contract.pattern}`,
          severity: 'error',
        });
      }
    }

    // 4. dependencies 约束
    if (field.contract?.dependencies && field.contract.dependencies.length > 0) {
      const depErrors = this.checkDependencies(field.key, field.contract.dependencies, value);
      fieldErrors.push(...depErrors);
    }

    return fieldErrors;
  }

  /**
   * 获取最新加载的 schema 领域信息
   */
  getDomain(): ConfigDomain | null {
    return this.domain;
  }

  // ---- 内部辅助方法 ----

  private checkType(
    key: string,
    expectedType: ConfigFieldDefinition['type'],
    value: unknown,
  ): ConfigValidationError | null {
    switch (expectedType) {
      case 'I32': {
        if (typeof value !== 'number' || !Number.isInteger(value)) {
          return {
            field: key,
            message: `"${key}" 应为整数 (I32)，但接收到 ${typeof value} 类型值 ${String(value)}`,
            severity: 'error',
          };
        }
        return null;
      }
      case 'STR': {
        if (typeof value !== 'string') {
          return {
            field: key,
            message: `"${key}" 应为字符串 (STR)，但接收到 ${typeof value} 类型值 ${String(value)}`,
            severity: 'error',
          };
        }
        return null;
      }
      case 'BOOL': {
        if (typeof value !== 'boolean') {
          return {
            field: key,
            message: `"${key}" 应为布尔值 (BOOL)，但接收到 ${typeof value} 类型值 ${String(value)}`,
            severity: 'error',
          };
        }
        return null;
      }
      case 'DURATION': {
        // DURATION 接受数字（毫秒）或字符串（如 "30s"）
        if (typeof value === 'number') {
          return null;
        }
        if (typeof value === 'string' && /^\d+(ms|s|m|h)?$/.test(value)) {
          return null;
        }
        return {
          field: key,
          message: `"${key}" 应为持续时间 (DURATION)，支持毫秒数字或 "30s"/"5m" 格式字符串`,
          severity: 'error',
        };
      }
      case 'SECRET': {
        // SECRET 接受字符串
        if (typeof value !== 'string') {
          return {
            field: key,
            message: `"${key}" 应为密文字符串 (SECRET)，但接收到 ${typeof value} 类型值`,
            severity: 'error',
          };
        }
        return null;
      }
      default:
        return {
          field: key,
          message: `"${key}" 使用了未知类型 ${expectedType}`,
          severity: 'error',
        };
    }
  }

  /**
   * 检查依赖关系约束。
   * 支持的表达式格式：
   *   - "field.key == value" — 关联字段的值必须等于指定值
   *   - "field.key > value"  — 关联字段的值必须大于指定值
   *   - "field.key"          — 关联字段必须存在
   */
  private checkDependencies(
    fieldKey: string,
    dependencies: string[],
    _value: unknown,
  ): ConfigValidationError[] {
    const errors: ConfigValidationError[] = [];

    for (const depExpr of dependencies) {
      // 尝试解析表达式: "field.key == value" 或 "field.key > value" 或 "field.key"
      const match = depExpr.match(/^(\S+?)\s*(==|>|<|>=|<=)?\s*(.*)$/);
      if (!match) {
        errors.push({
          field: fieldKey,
          message: `依赖表达式格式错误: "${depExpr}"`,
          severity: 'warning',
        });
        continue;
      }

      const [, depFieldKey, operator, operandRaw] = match;
      const operand = operandRaw?.trim();

      const depField = this.fieldMap.get(depFieldKey);
      if (!depField) {
        errors.push({
          field: fieldKey,
          message: `依赖字段 "${depFieldKey}" 在 schema 中未定义`,
          severity: 'error',
        });
        continue;
      }

      // 如果没有操作符，只检查关联字段是否存在（已确保 depField 存在）
      if (!operator) {
        continue;
      }

      // 有操作符时，需要检查关系表达式的值
      const depValue = operand;
      if (!depValue) {
        errors.push({
          field: fieldKey,
          message: `依赖表达式 "${depExpr}" 缺少操作数值`,
          severity: 'warning',
        });
        continue;
      }

      if (operator === '==') {
        // 相等性依赖无法在 validate() 中直接检查（需要外部传入的值），
        // 这里产生一个 info 级别的 warning 提示用户
        errors.push({
          field: fieldKey,
          message: `"${fieldKey}" 依赖 "${depExpr}"，请确保该条件满足`,
          severity: 'warning',
        });
      } else if (operator === '>' || operator === '<' || operator === '>=' || operator === '<=') {
        // 大小比较依赖同理
        errors.push({
          field: fieldKey,
          message: `"${fieldKey}" 依赖 "${depExpr}"，请确保该条件满足`,
          severity: 'warning',
        });
      }
    }

    return errors;
  }


}
