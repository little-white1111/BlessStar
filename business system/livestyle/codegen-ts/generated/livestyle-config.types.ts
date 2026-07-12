/**
 * 此文件由 @blessstar/codegen-ts 自动生成
 * 源文件: ..\..\config-schema.yaml
 * 生成时间: 2026-07-11
 * 请勿手动编辑 — 下次运行 codegen 时将覆盖
 */

// ─── Livestyle 全局配置类型 ───────────────────────────

/**
 * livestyle 领域全部配置项
 * Schema 版本: v1.0.0
 * 字段总数: 22
 */
export interface LivestyleConfig {

  /** 画布网格对齐粒度（px），控制拖拽组件时吸附的网格大小。0 表示关闭网格对齐，值越大吸附越粗糙，影响布局精度和操作手感 */
  /** @default 20 */
  'canvas.grid.size'?: number;

  /** 是否启用网格吸附对齐。开启后拖拽组件会自动吸附到最近的网格交点，帮助用户精确对齐布局；关闭后组件可自由放置 */
  /** @default true */
  'canvas.grid.snap'?: boolean;

  /** 画布默认宽度（px），对应直播输出分辨率宽度。影响所有组件在画布上的布局范围和导出时的分辨率 */
  /** @default 1920 */
  'canvas.default_width'?: number;

  /** 画布默认高度（px），对应直播输出分辨率高度。影响所有组件在画布上的布局范围和导出时的分辨率 */
  /** @default 1080 */
  'canvas.default_height'?: number;

  /** 自动保存间隔（毫秒），控制画布编辑状态自动保存到本地的频率。0 表示关闭自动保存。间隔越短数据越安全，但频繁写入可能影响性能 */
  /** @default 30000 */
  'canvas.autosave_interval_ms'?: number;

  /** 新拖入组件的默认宽度（px）。用户在组件面板将新组件拖入画布时，自动使用此宽度创建占位区域 */
  /** @default 400 */
  'component.default_width'?: number;

  /** 新拖入组件的默认高度（px）。用户在组件面板将新组件拖入画布时，自动使用此高度创建占位区域 */
  /** @default 300 */
  'component.default_height'?: number;

  /** 组件引擎渲染帧率上限（fps），控制直播间组件（弹幕、动画等）的刷新频率。帧率越高动画越流畅，但 CPU/GPU 负载也越高 */
  /** @default 60 */
  'engine.render.fps_limit'?: number;

  /** 是否启用脏矩形增量渲染。开启后引擎只重新绘制有变更的区域，大幅提升多组件场景下的渲染性能；关闭后每次更新全量重绘 */
  /** @default true */
  'engine.render.dirty_rect_enabled'?: boolean;

  /** OBS websocket 主机地址，用于 livestyle 连接 OBS Studio 进行实时推流控制。通常为本地回环地址，远程部署时可配置为局域网 IP */
  /** @default 'localhost' */
  'obs.connection.host': string;

  /** OBS websocket 端口号，对应 OBS Studio 中 websocket 服务器设置的监听端口。修改后需与 OBS 端实际配置一致 */
  /** @default 4455 */
  'obs.connection.port': number;

  /** OBS websocket 连接密码（加密存储）。如果 OBS 端开启了 websocket 认证，需要在此配置对应的密码才能成功建立连接 */
  /** @default '' */
  'obs.connection.password'?: string;

  /** 是否启用 OBS 自动重连。开启后当 OBS 连接意外断开时，系统会自动按配置的间隔尝试重新连接，无需手动干预 */
  /** @default true */
  'obs.connection.auto_reconnect'?: boolean;

  /** OBS 自动重连间隔（毫秒），控制每次重连尝试之间的等待时间。间隔太短会频繁发起连接请求，太长则断连恢复慢 */
  /** @default 5000 */
  'obs.connection.retry_interval_ms'?: number;

  /** OBS 同步防抖时间（毫秒），避免频繁向 OBS 推送配置变更。当用户连续操作时，只有最后一次操作后的等待期内无新操作才会触发同步 */
  /** @default 200 */
  'obs.sync.debounce_ms'?: number;

  /** TaskList 组件标题文字，显示在组件顶部的标题栏区域。主播可根据直播主题自定义标题内容 */
  /** @default '任务列表' */
  'component.task-list.title'?: string;

  /** TaskList 组件背景色，支持 hex 格式颜色值。影响组件的整体视觉风格，需与直播间主题色调协调 */
  /** @default '#1a1a2e' */
  'component.task-list.backgroundColor'?: string;

  /** TaskList 字体大小（px），控制组件内任务文字的显示大小。需考虑直播画面分辨率和观众观看距离 */
  /** @default 14 */
  'component.task-list.fontSize'?: number;

  /** TaskList 文字颜色，支持 hex 格式颜色值。控制任务列表中所有文字（包括标题和任务项）的显示颜色 */
  /** @default '#ffffff' */
  'component.task-list.textColor'?: string;

  /** 逗号分隔的任务列表文本，定义组件中显示的任务条目。每个逗号分隔的值对应一个任务项，支持中英文混合 */
  /** @default '任务1,任务2,任务3' */
  'component.task-list.items'?: string;

  /** 是否显示 TaskList 组件标题栏。开启后在组件顶部显示标题区域，关闭后只显示任务列表内容，提供更紧凑的布局 */
  /** @default true */
  'component.task-list.showHeader'?: boolean;

  /** TaskList 边框圆角大小（px），控制组件四个角的圆润程度。圆角越大视觉效果越柔和，0 表示直角 */
  /** @default 8 */
  'component.task-list.borderRadius'?: number;
}


/** canvas 模块配置 */
export interface CanvasConfig {
  /** 画布网格对齐粒度（px），控制拖拽组件时吸附的网格大小。0 表示关闭网格对齐，值越大吸附越粗糙，影响布局精度和操作手感 */
  grid.size?: number;
  /** 是否启用网格吸附对齐。开启后拖拽组件会自动吸附到最近的网格交点，帮助用户精确对齐布局；关闭后组件可自由放置 */
  grid.snap?: boolean;
  /** 画布默认宽度（px），对应直播输出分辨率宽度。影响所有组件在画布上的布局范围和导出时的分辨率 */
  default_width?: number;
  /** 画布默认高度（px），对应直播输出分辨率高度。影响所有组件在画布上的布局范围和导出时的分辨率 */
  default_height?: number;
  /** 自动保存间隔（毫秒），控制画布编辑状态自动保存到本地的频率。0 表示关闭自动保存。间隔越短数据越安全，但频繁写入可能影响性能 */
  autosave_interval_ms?: number;
}

/** component 模块配置 */
export interface ComponentConfig {
  /** 新拖入组件的默认宽度（px）。用户在组件面板将新组件拖入画布时，自动使用此宽度创建占位区域 */
  default_width?: number;
  /** 新拖入组件的默认高度（px）。用户在组件面板将新组件拖入画布时，自动使用此高度创建占位区域 */
  default_height?: number;
}

/** engine 模块配置 */
export interface EngineConfig {
  /** 组件引擎渲染帧率上限（fps），控制直播间组件（弹幕、动画等）的刷新频率。帧率越高动画越流畅，但 CPU/GPU 负载也越高 */
  render.fps_limit?: number;
  /** 是否启用脏矩形增量渲染。开启后引擎只重新绘制有变更的区域，大幅提升多组件场景下的渲染性能；关闭后每次更新全量重绘 */
  render.dirty_rect_enabled?: boolean;
}

/** obs 模块配置 */
export interface ObsConfig {
  /** OBS websocket 主机地址，用于 livestyle 连接 OBS Studio 进行实时推流控制。通常为本地回环地址，远程部署时可配置为局域网 IP */
  connection.host: string;
  /** OBS websocket 端口号，对应 OBS Studio 中 websocket 服务器设置的监听端口。修改后需与 OBS 端实际配置一致 */
  connection.port: number;
  /** OBS websocket 连接密码（加密存储）。如果 OBS 端开启了 websocket 认证，需要在此配置对应的密码才能成功建立连接 */
  connection.password?: string;
  /** 是否启用 OBS 自动重连。开启后当 OBS 连接意外断开时，系统会自动按配置的间隔尝试重新连接，无需手动干预 */
  connection.auto_reconnect?: boolean;
  /** OBS 自动重连间隔（毫秒），控制每次重连尝试之间的等待时间。间隔太短会频繁发起连接请求，太长则断连恢复慢 */
  connection.retry_interval_ms?: number;
  /** OBS 同步防抖时间（毫秒），避免频繁向 OBS 推送配置变更。当用户连续操作时，只有最后一次操作后的等待期内无新操作才会触发同步 */
  sync.debounce_ms?: number;
}

/** component.task-list 模块配置 */
export interface ComponentTaskListConfig {
  /** TaskList 组件标题文字，显示在组件顶部的标题栏区域。主播可根据直播主题自定义标题内容 */
  title?: string;
  /** TaskList 组件背景色，支持 hex 格式颜色值。影响组件的整体视觉风格，需与直播间主题色调协调 */
  backgroundColor?: string;
  /** TaskList 字体大小（px），控制组件内任务文字的显示大小。需考虑直播画面分辨率和观众观看距离 */
  fontSize?: number;
  /** TaskList 文字颜色，支持 hex 格式颜色值。控制任务列表中所有文字（包括标题和任务项）的显示颜色 */
  textColor?: string;
  /** 逗号分隔的任务列表文本，定义组件中显示的任务条目。每个逗号分隔的值对应一个任务项，支持中英文混合 */
  items?: string;
  /** 是否显示 TaskList 组件标题栏。开启后在组件顶部显示标题区域，关闭后只显示任务列表内容，提供更紧凑的布局 */
  showHeader?: boolean;
  /** TaskList 边框圆角大小（px），控制组件四个角的圆润程度。圆角越大视觉效果越柔和，0 表示直角 */
  borderRadius?: number;
}
