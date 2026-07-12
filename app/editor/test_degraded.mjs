import { createRequire } from 'module';
import { join, dirname } from 'path';
import { fileURLToPath } from 'url';
import { readFileSync, writeFileSync, existsSync, mkdirSync, appendFileSync } from 'fs';

const __dirname = dirname(fileURLToPath(import.meta.url));
const nativePath = join(__dirname, 'native/index.js');
const addon = createRequire(import.meta.url)(nativePath);

const appData = process.env.APPDATA;
const baseDir = join(appData, 'livedesign-config-editor', 'BlessStar', 'configs');
const manifestPath = join(baseDir, 'manifest.json');
const configsPath = join(baseDir, 'configs.json');
const tmpPath = configsPath + '.tmp';

console.log('=== 环境 ===');
console.log('configs.json:', configsPath);
console.log('file writable:', existsSync(baseDir));

// 验证 Node.js 可以写文件
try {
  writeFileSync(tmpPath, 'test');
  console.log('Node.js write tmp OK');
} catch(e) { console.error('Node.js write tmp FAIL:', e.message); }

// 注册字段
addon.registerSchemaFieldFfi('auth.jwt.token_expiry_seconds', 2, '86400', 'desc', false);
addon.writeBlessStarConfig('auth.jwt.token_expiry_seconds', '12345');

// 测试 configReadAllFfi
const all = addon.configReadAllFfi();
console.log('configReadAllFfi:', all);

// 测试 configPersistWriteFfi
console.log('configPersistWriteFfi:', addon.configPersistWriteFfi(manifestPath));

// 检查 tmp 文件是否存在
console.log('tmp exists after write:', existsSync(tmpPath));

// 恢复
addon.writeBlessStarConfig('auth.jwt.token_expiry_seconds', '60000');
addon.configPersistWriteFfi(manifestPath);
console.log('恢复完成');
