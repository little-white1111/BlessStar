import { createRequire } from 'module';
import { join, dirname } from 'path';
import { fileURLToPath } from 'url';
import { readFileSync } from 'fs';

const __dirname = dirname(fileURLToPath(import.meta.url));
const addon = createRequire(import.meta.url)(join(__dirname, 'native/index.js'));

const appData = process.env.APPDATA;
const baseDir = join(appData, 'livedesign-config-editor', 'BlessStar', 'configs');
const manifestPath = join(baseDir, 'manifest.json');
const configsPath = join(baseDir, 'configs.json');

// 注册字段并写入值
addon.registerSchemaFieldFfi('test.persist_direct', 2, '', 'test', false);
addon.writeBlessStarConfig('test.persist_direct', 'rust_write_ok');

console.log('=== 测试 Rust 层持久化 ===');
console.log('configReadAll:', addon.configReadAllFfi());

// 测试新的 direct write
const result = addon.configPersistWriteDirectFfi(manifestPath);
console.log('configPersistWriteDirectFfi:', result);

// 读取文件验证
const configs = JSON.parse(readFileSync(configsPath, 'utf-8'));
console.log('configs.json:', JSON.stringify(configs, null, 2));

if (configs['test.persist_direct'] === 'rust_write_ok') {
  console.log('✓ Rust 持久化成功!');
} else {
  console.log('✗ 持久化失败');
}

// 清理
addon.writeBlessStarConfig('test.persist_direct', '');
addon.configPersistWriteDirectFfi(manifestPath);
console.log('清理完成');
