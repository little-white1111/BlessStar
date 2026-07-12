console.log('process.arch:', process.arch);
console.log('process.platform:', process.platform);
console.log('APPDATA:', process.env.APPDATA);
console.log('USERPROFILE:', process.env.USERPROFILE);

// 测试直接写 APPDATA 路径
import { writeFileSync, readFileSync, existsSync } from 'fs';
import { join } from 'path';

const testFile = join(process.env.APPDATA, 'livedesign-config-editor', 'BlessStar', 'configs', 'node_test_write.txt');
console.log('test file:', testFile);

try {
  writeFileSync(testFile, 'node.js write test');
  console.log('write OK, content:', readFileSync(testFile, 'utf-8'));
} catch(e) {
  console.error('write FAIL:', e.message);
}
