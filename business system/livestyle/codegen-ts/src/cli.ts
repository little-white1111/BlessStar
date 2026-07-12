#!/usr/bin/env node

/**
 * @blessstar/codegen-ts CLI 入口
 * 从 config-schema.yaml 生成 TypeScript 类型/Port/Adapter
 *
 * 用法: codegen-ts --schema <path> --output <dir>
 */

import * as fs from 'fs';
import * as path from 'path';
import { parseSchema, generateTypes, generatePort, generateMockAdapter, generateLocalFileAdapter } from './index.js';

interface CliArgs {
  schema: string;
  output: string;
}

function parseArgs(): CliArgs {
  const args = process.argv.slice(2);
  const result: CliArgs = { schema: '', output: '' };

  for (let i = 0; i < args.length; i++) {
    switch (args[i]) {
      case '--schema':
        result.schema = args[++i] ?? '';
        break;
      case '--output':
        result.output = args[++i] ?? '';
        break;
      case '--help':
      case '-h':
        printHelp();
        process.exit(0);
    }
  }

  if (!result.schema || !result.output) {
    console.error('错误: 必须指定 --schema 和 --output 参数');
    printHelp();
    process.exit(1);
  }

  return result;
}

function printHelp(): void {
  console.log(`
@blessstar/codegen-ts — 从 config-schema.yaml 生成 TypeScript 代码

用法:
  codegen-ts --schema <config-schema.yaml 路径> --output <输出目录>

选项:
  --schema <path>     config-schema.yaml 文件路径 (必需)
  --output <dir>      生成文件的输出目录 (必需)
  --help, -h          显示帮助信息

示例:
  codegen-ts --schema ../config-schema.yaml --output ./generated
`);
}

function ensureDir(dir: string): void {
  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, { recursive: true });
  }
}

function main(): void {
  const { schema: schemaPath, output: outputDir } = parseArgs();

  const schemaAbsPath = path.resolve(schemaPath);
  const outputAbsDir = path.resolve(outputDir);
  const sourceFileName = path.relative(outputAbsDir, schemaAbsPath);

  console.log(`[codegen-ts] 读取 schema: ${schemaAbsPath}`);

  if (!fs.existsSync(schemaAbsPath)) {
    console.error(`错误: schema 文件不存在: ${schemaAbsPath}`);
    process.exit(1);
  }

  const yamlContent = fs.readFileSync(schemaAbsPath, 'utf-8');
  const schema = parseSchema(yamlContent);

  console.log(`[codegen-ts] 解析完成: domain=${schema.domain}, version=${schema.version}, fields=${schema.fields.length}`);

  ensureDir(outputAbsDir);

  // 生成 4 个文件
  const files: Array<{ name: string; content: string }> = [
    { name: 'livestyle-config.types.ts', content: generateTypes(schema, sourceFileName) },
    { name: 'livestyle-config.port.ts', content: generatePort(schema, sourceFileName) },
    { name: 'livestyle-config.mock.ts', content: generateMockAdapter(schema, sourceFileName) },
    { name: 'livestyle-config.adapter.ts', content: generateLocalFileAdapter(schema, sourceFileName) },
  ];

  for (const file of files) {
    const filePath = path.join(outputAbsDir, file.name);
    fs.writeFileSync(filePath, file.content, 'utf-8');
    console.log(`[codegen-ts] 生成: ${filePath}`);
  }

  console.log(`[codegen-ts] 完成! 共生成 ${files.length} 个文件到 ${outputAbsDir}`);
}

main();
