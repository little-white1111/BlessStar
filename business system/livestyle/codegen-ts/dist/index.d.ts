/**
 * @blessstar/codegen-ts — 从 config-schema.yaml 生成 TypeScript 类型/Port/Adapter
 * 遵循 I15：config-schema.yaml 为 Codegen 唯一真理源
 * 遵循 I16：Codegen 输出必须通过版本管理
 */
export interface ContractConstraint {
    range?: [number, number];
    pattern?: string;
    dependencies?: string[];
    approval_required?: boolean;
    slo_impact?: string;
}
export interface ConfigFieldDefinition {
    id: number;
    key: string;
    type: string;
    default: unknown;
    required: boolean;
    business_desc: string;
    contract?: ContractConstraint;
    ui_meta?: Record<string, unknown>;
    ai_hint?: string;
    search_keywords?: string[];
}
export interface ConfigSchema {
    domain: string;
    version: string;
    fields: ConfigFieldDefinition[];
    x_extensions?: Record<string, unknown>;
}
/** 从 config-schema.yaml 解析 Schema */
export declare function parseSchema(yamlContent: string): ConfigSchema;
/** 生成 TypeScript 类型定义字符串 */
export declare function generateTypes(schema: ConfigSchema, sourceFile?: string): string;
/** 生成 Port 接口（I2 ConfigPort） */
export declare function generatePort(schema: ConfigSchema, sourceFile?: string): string;
/** 生成 Mock Adapter */
export declare function generateMockAdapter(schema: ConfigSchema, sourceFile?: string): string;
/** 生成 LocalFileAdapter */
export declare function generateLocalFileAdapter(schema: ConfigSchema, sourceFile?: string): string;
