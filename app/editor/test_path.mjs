import { createRequire } from 'module';
import { readFileSync, existsSync } from 'fs';
import { join, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));

const appData = process.env.APPDATA;
const baseDir = join(appData, 'livedesign-config-editor', 'BlessStar', 'configs');
const manifestPath = join(baseDir, 'manifest.json');
const configsPath = join(baseDir, 'configs.json');

console.log('manifest:', manifestPath);
console.log('configs:', configsPath);
console.log('manifest exists:', existsSync(manifestPath));
console.log('configs exists:', existsSync(configsPath));
console.log('baseDir exists:', existsSync(baseDir));

// 读取 manifest
const manifest = JSON.parse(readFileSync(manifestPath, 'utf-8'));
console.log('manifest content:', JSON.stringify(manifest));

// 手动解析路径
const dataFile = manifest.data_file;
const manifestDir = dirname(manifestPath);
const resolvedPath = join(manifestDir, dataFile);
console.log('data_file:', dataFile);
console.log('manifest_dir:', manifestDir);
console.log('resolved_path:', resolvedPath);
console.log('resolved exists:', existsSync(resolvedPath));
console.log('paths equal:', resolvedPath === configsPath);
