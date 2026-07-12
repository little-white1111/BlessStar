/** 配置域定义 */
export interface ConfigDomain {
  domain: string;
  version: string;
  fields: ConfigFieldDefinition[];
}

/** 配置字段定义 */
export interface ConfigFieldDefinition {
  id: number;
  key: string;
  type: 'I32' | 'STR' | 'BOOL' | 'DURATION' | 'SECRET';
  default: unknown;
  description: string;
  module: string;
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
  };
}

/** 验证结果 */
export interface ConfigValidationResult {
  valid: boolean;
  errors: ConfigValidationError[];
  warnings: string[];
}

export interface ConfigValidationError {
  field: string;
  message: string;
  severity: 'error' | 'warning';
}
