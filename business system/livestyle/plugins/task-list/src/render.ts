/**
 * TaskList 组件渲染函数
 * 纯函数：接收 props 返回 HTML 字符串
 * 遵循架构不变量 I8：零依赖 — 不依赖任何 UI 框架
 */

export interface TaskListProps {
  title?: string;
  backgroundColor?: string;
  fontSize?: number;
  textColor?: string;
  items?: string;
  showHeader?: boolean;
  borderRadius?: number;
}

/**
 * 任务列表渲染函数
 * 将组件 props 渲染为内联样式的 HTML 字符串
 */
export function renderTaskList(props: TaskListProps): string {
  const {
    title = '任务列表',
    backgroundColor = '#1a1a2e',
    fontSize = 14,
    textColor = '#ffffff',
    items = '',
    showHeader = true,
    borderRadius = 8,
  } = props;

  // 解析任务列表
  const taskItems = items
    .split(',')
    .map((item) => item.trim())
    .filter((item) => item.length > 0);

  const taskHtml = taskItems
    .map(
      (task, index) => `
    <div style="
      display:flex;
      align-items:center;
      gap:8px;
      padding:6px 10px;
      background:rgba(255,255,255,0.05);
      border-radius:4px;
      margin-bottom:4px;
    ">
      <input type="checkbox" style="accent-color:#4a9eff;" />
      <span style="color:${textColor};font-size:${fontSize}px;flex:1;">
        ${task}
      </span>
      <span style="color:rgba(255,255,255,0.3);font-size:${fontSize - 2}px;">
        #${index + 1}
      </span>
    </div>
  `,
    )
    .join('');

  return `
    <div style="
      width:100%;
      height:100%;
      background:${backgroundColor};
      border-radius:${borderRadius}px;
      padding:12px;
      box-sizing:border-box;
      overflow:auto;
      font-family:system-ui,-apple-system,sans-serif;
    ">
      ${showHeader ? `
        <div style="
          display:flex;
          justify-content:space-between;
          align-items:center;
          margin-bottom:10px;
          padding-bottom:8px;
          border-bottom:1px solid rgba(255,255,255,0.1);
        ">
          <span style="
            color:${textColor};
            font-size:${Math.min(fontSize + 4, 24)}px;
            font-weight:600;
          ">
            ${title}
          </span>
          <span style="
            color:rgba(255,255,255,0.4);
            font-size:${fontSize - 2}px;
          ">
            ${taskItems.length} 项
          </span>
        </div>
      ` : ''}
      ${taskItems.length > 0
        ? taskHtml
        : `<div style="color:rgba(255,255,255,0.3);font-size:${fontSize}px;text-align:center;padding:20px;">
            暂无任务
          </div>`
      }
    </div>
  `;
}
