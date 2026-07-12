/**
 * 端到端测试骨架
 * 使用 @playwright/test 测试 Electron 应用启动后的基本流程
 *
 * 运行方式: npm run test:e2e
 * 前置条件: Electron 应用已构建 (npm run build)
 */
import { test, expect } from '@playwright/test';
import { _electron as electron } from 'playwright';

/**
 * Electron 应用路径（可根据构建输出调整）
 */
const ELECTRON_APP_PATH = './dist/main/index.js';

test.describe('LiveDesign 应用基本流程', () => {
  let electronApp: import('playwright').ElectronApplication;
  let mainWindow: import('playwright').Page;

  test.beforeAll(async () => {
    // 启动 Electron 应用
    electronApp = await electron.launch({
      args: [ELECTRON_APP_PATH],
      // 可根据需要设置环境变量
      env: {
        NODE_ENV: 'test',
      },
    });

    // 等待主窗口出现
    mainWindow = await electronApp.firstWindow();
    await mainWindow.waitForLoadState('domcontentloaded');
  });

  test.afterAll(async () => {
    // 关闭应用
    if (electronApp) {
      await electronApp.close();
    }
  });

  test('应用启动后悬浮球应显示', async () => {
    // 悬浮球应包含一个浮动的圆形元素
    // 选择器需要根据实际渲染组件的 DOM 结构调整
    const floatBall = mainWindow.locator('[data-testid="float-ball"]');
    await expect(floatBall).toBeVisible({ timeout: 10000 });

    // 悬浮球应可见且位于屏幕边缘
    const boundingBox = await floatBall.boundingBox();
    expect(boundingBox).not.toBeNull();
    if (boundingBox) {
      // 悬浮球应靠近屏幕右侧边缘
      expect(boundingBox.x).toBeGreaterThan(0);
    }
  });

  test('点击悬浮球应弹出对话面板', async () => {
    // 点击悬浮球
    const floatBall = mainWindow.locator('[data-testid="float-ball"]');
    await floatBall.click();

    // 对话面板应弹出
    const dialogPanel = mainWindow.locator('[data-testid="dialog-panel"]');
    await expect(dialogPanel).toBeVisible({ timeout: 5000 });

    // 验证面板包含消息输入框
    const inputArea = mainWindow.locator('[data-testid="chat-input"]');
    await expect(inputArea).toBeVisible();

    // 验证面板包含消息列表区域
    const messageList = mainWindow.locator('[data-testid="message-list"]');
    await expect(messageList).toBeVisible();
  });

  test('对话面板应能发送消息并显示回复', async () => {
    // 确保对话面板已打开
    const floatBall = mainWindow.locator('[data-testid="float-ball"]');
    await floatBall.click();

    const dialogPanel = mainWindow.locator('[data-testid="dialog-panel"]');
    await expect(dialogPanel).toBeVisible();

    // 在输入框中输入消息
    const inputArea = mainWindow.locator('[data-testid="chat-input"]');
    await inputArea.fill('你好！');

    // 点击发送按钮
    const sendButton = mainWindow.locator('[data-testid="send-button"]');
    await sendButton.click();

    // 等待回复出现（LLM 回复可能需要一些时间）
    // 注意：实际测试中可能需要 mock LLM 服务
    const assistantMessage = mainWindow.locator(
      '[data-testid="message-item"][data-role="assistant"]',
    );
    await expect(assistantMessage).toBeVisible({ timeout: 30000 });
  });

  test('角色切换功能应正常工作', async () => {
    // 打开对话面板
    const floatBall = mainWindow.locator('[data-testid="float-ball"]');
    await floatBall.click();

    // 点击角色切换按钮
    const characterSwitch = mainWindow.locator('[data-testid="character-switch"]');
    await characterSwitch.click();

    // 角色选择面板应显示
    const characterList = mainWindow.locator('[data-testid="character-list"]');
    await expect(characterList).toBeVisible();

    // 选择一个角色
    const firstCharacter = characterList.locator('[data-testid="character-item"]').first();
    await firstCharacter.click();

    // 验证角色已切换（角色名称应更新）
    const currentCharacter = mainWindow.locator('[data-testid="current-character"]');
    await expect(currentCharacter).toBeVisible();
  });

  test('应用窗口应包含正确的标题', async () => {
    const title = await mainWindow.title();
    expect(title).toContain('LiveDesign');
  });
});
