/**
 * bizKnowledge.ts — 业务知识索引（D38-8-INV-05：初始 ≤3 个概念）
 *
 * 专题八 · 方案2：业务知识倒排索引 + 概念短路。
 *
 * 用于拦截"配置是什么""这个工具有什么用"等纯概念性问题，
 * 用确定性回复替代 LLM（零幻觉），同时通过概念锚点链（relatedConfigKeys）
 * 为降级路径提供候选配置项。
 *
 * 数据结构：
 *   keyword → conceptId (通过 TargetType='concept' 的 RouteEntry 学习)
 *   conceptId → BizKnowledgeEntry (固定的解释 + 路由 + 关联配置)
 *
 * D38-8-INV-02: 配置层优先 — 概念层仅在配置层 miss 后介入
 * D38-8-INV-03: 概念短路 freq≥1 — 一次反问确认即短路
 * D38-8-INV-05: 初始 ≤3 个概念，后续由 confirmRoute 学习
 */

// ── 概念 ID 枚举 ────────────────────────────────────────────────────

/** 冷启动概念 ID（全局 3 个 + 域概念 5 个） */
export const CONCEPT_IDS = {
  // 全局概念（原 3 个）
  CONFIG_INTRO: 'biz:config_intro',
  SYSTEM_INTRO: 'biz:system_intro',
  TOOL_INTRO: 'biz:tool_intro',
  // 域概念（D38-DOMAIN-CONCEPTS: LiveDesign 5 大域）
  DOMAIN_DANMAKU: 'biz:domain_danmaku',
  DOMAIN_LIVE2D: 'biz:domain_live2d',
  DOMAIN_CONNECTION: 'biz:domain_connection',
  DOMAIN_DISPLAY: 'biz:domain_display',
  DOMAIN_AUTH: 'biz:domain_auth',
} as const

export type ConceptId = (typeof CONCEPT_IDS)[keyof typeof CONCEPT_IDS]

// ── 类型 ─────────────────────────────────────────────────────────────

/** 业务知识条目 */
export interface BizKnowledgeEntry {
  /** 概念 ID */
  conceptId: ConceptId
  /** 自然语言解释（直接注入给 LLM 或直接展示） */
  explanation: string
  /** 冷启动边界关键词（后续通过 confirmRoute 学习扩展） */
  boundaryKeywords: string[]
  /**
   * 意图类型 → 工具名映射
   * 引擎根据 UA 产出意图自动选择工具路由
   */
  toolRoutes: Record<string, string[]>
  /** 关联的配置项 key 列表（供降级路径概念锚点链使用） */
  relatedConfigKeys: string[]
  /**
   * 边界命中（freq=0）时的主动反问文案。
   * 占位符 {concept} 会被替换为概念名称。
   */
  clarificationQuestion: string
  /** 概念显示名称 */
  displayName: string
}

// ── 冷启动数据集（不超过 3 个） ─────────────────────────────────────

const BIZ_KNOWLEDGE: Record<ConceptId, BizKnowledgeEntry> = {
  [CONCEPT_IDS.CONFIG_INTRO]: {
    conceptId: CONCEPT_IDS.CONFIG_INTRO,
    displayName: '配置概念',
    explanation:
      'BlessStar 是一个配置驱动引擎。所有可通过修改来改变业务系统行为的参数都称为"配置项"（Config）。' +
      '每个配置项有唯一的键（key）和当前值（value），你可以通过以下操作管理它们：\n' +
      '  • 查询：查看所有配置项或某个配置的当前值\n' +
      '  • 修改：将某个配置项设置为新值\n' +
      '  • 操作：执行基于配置的工具行为\n\n' +
      '当前系统包含 40+ 个配置项，涵盖直播间设置、弹幕样式、连接参数、文件路径等不同领域。' +
      '你可以直接说出配置名称（如"房间号""弹幕颜色"）来查询或修改。',
    boundaryKeywords: ['什么是配置', '配置是什么', '配置概念', '什么叫配置', '配置的定义'],
    toolRoutes: {},
    relatedConfigKeys: [
      'livedesign.room.room_id',
      'livedesign.danmaku.font_size',
      'livedesign.danmaku.font_color',
      'livedesign.connection.heartbeat_interval',
      'livedesign.live2d.model_scale',
      'livedesign.display.layout_mode',
    ],
    clarificationQuestion: '您指的是「{concept}」还是某个具体的配置项（如"房间号""弹幕颜色"）？',
  },

  [CONCEPT_IDS.SYSTEM_INTRO]: {
    conceptId: CONCEPT_IDS.SYSTEM_INTRO,
    displayName: '系统介绍',
    explanation:
      'BlessStar 是一款浏览器扩展工具，用于实时管理直播间的各项配置。\n\n' +
      '核心功能：\n' +
      '  • 配置管理：查看和修改所有直播相关配置项（房间号、弹幕样式、连接参数等）\n' +
      '  • 规则引擎：通过 Gate 链规则自动响应配置变更\n' +
      '  • 实时生效：修改配置后立即生效，无需重启\n\n' +
      '有"房间号""弹幕颜色""心跳间隔"等 40+ 个配置项可供调整。',
    boundaryKeywords: ['这个工具是什么', '系统有什么用', '系统介绍', '这是什么工具', '什么是BlessStar', 'BlessStar是什么'],
    toolRoutes: {},
    relatedConfigKeys: [
      'livedesign.room.room_id',
      'livedesign.danmaku.font_size',
      'livedesign.display.layout_mode',
    ],
    clarificationQuestion: '您是想了解「{concept}」，还是想查看某个具体配置项？',
  },

  [CONCEPT_IDS.TOOL_INTRO]: {
    conceptId: CONCEPT_IDS.TOOL_INTRO,
    displayName: '工具能力',
    explanation:
      '我可以帮你完成以下操作：\n\n' +
      '  1. 查询配置 — "当前有哪些配置""房间号是多少"\n' +
      '  2. 修改配置 — "把弹幕颜色改成红色""房间号设为 10041"\n' +
      '  3. 浏览文件 — "查看模型目录""读取日志文件"\n' +
      '  4. 搜索内容 — "在文档中搜索 直播"\n' +
      '  5. 创建 Gate 规则 — "新增规则：如果房间号是 10041，弹幕字号设为 20"\n' +
      '  6. 概念咨询 — "配置是什么""有什么工具可用"\n\n' +
      '你可以直接说出具体需求，我会智能选择对应的工具来处理。',
    boundaryKeywords: ['你能做什么', '工具有什么功能', '支持哪些功能', '你有什么能力', '你可以做什么'],
    toolRoutes: {},
    relatedConfigKeys: [],
    clarificationQuestion: '您是想了解「{concept}」，还是有具体的配置需要我帮忙处理？',
  },

  // ── 域概念（D38-DOMAIN-CONCEPTS） ─────────────────────────────────

  [CONCEPT_IDS.DOMAIN_DANMAKU]: {
    conceptId: CONCEPT_IDS.DOMAIN_DANMAKU,
    displayName: '弹幕系统',
    explanation:
      '弹幕系统负责实时显示和过滤直播间弹幕。你可以调整弹幕样式（字号、颜色、透明度、位置）和过滤规则（屏蔽词、用户等级、礼物/进场/点赞屏蔽）。',
    boundaryKeywords: ['弹幕是什么', '弹幕有哪些设置', '弹幕配置', '弹幕怎么设置', '弹幕功能', '弹幕有哪些配置'],
    toolRoutes: { '/danmaku': ['read_config_value'] },
    relatedConfigKeys: [
      'livedesign.danmaku.font_size',
      'livedesign.danmaku.font_color',
      'livedesign.danmaku.bg_opacity',
      'livedesign.danmaku.position',
      'livedesign.danmaku.max_visible',
      'livedesign.danmaku.animation',
      'livedesign.danmaku.colorful_username',
      'livedesign.danmaku.block_keywords',
      'livedesign.danmaku.min_user_level',
      'livedesign.danmaku.block_gift',
      'livedesign.danmaku.block_enter',
      'livedesign.danmaku.block_like',
    ],
    clarificationQuestion: '您想了解弹幕系统的整体功能，还是想调整某个具体的弹幕配置项（如字号、颜色、屏蔽规则）？',
  },

  [CONCEPT_IDS.DOMAIN_LIVE2D]: {
    conceptId: CONCEPT_IDS.DOMAIN_LIVE2D,
    displayName: 'Live2D 模型',
    explanation:
      'Live2D 模型是你桌面上显示的虚拟角色。你可以设置模型路径、缩放比例、待机动作、点击反馈等。模型可以是本地的（.model3.json 文件）或在线的（URL）。',
    boundaryKeywords: ['模型是什么', '模型有哪些设置', '模型配置', '模型怎么设置', 'live2d是什么'],
    toolRoutes: { '/model': ['read_config_value', 'list_directory'] },
    relatedConfigKeys: [
      'livedesign.live2d.model_path',
      'livedesign.live2d.model_directory',
      'livedesign.live2d.model_scale',
      'livedesign.live2d.memory_size_mb',
      'livedesign.live2d.default_test_model_url',
      'livedesign.live2d.idle_motion_enabled',
      'livedesign.live2d.idle_interval_ms',
      'livedesign.live2d.action_queue_enabled',
      'livedesign.live2d.click_feedback_enabled',
    ],
    clarificationQuestion: '您想了解 Live2D 模型的整体功能，还是想查看/修改某个具体的模型配置项（如模型路径、缩放比例、待机动作）？',
  },

  [CONCEPT_IDS.DOMAIN_CONNECTION]: {
    conceptId: CONCEPT_IDS.DOMAIN_CONNECTION,
    displayName: '直播间连接',
    explanation:
      '直播间连接负责与 B站直播间建立 WebSocket 连接以获取实时弹幕。你可以设置房间号、重连策略、心跳间隔和认证 Cookie。',
    boundaryKeywords: ['连接是什么', '连接怎么设置', '直播间怎么连接', '连接配置', '有哪些连接设置', '连接有哪些配置'],
    toolRoutes: { '/room': ['read_config_value'] },
    relatedConfigKeys: [
      'livedesign.room.room_id',
      'livedesign.room.last_room_id',
      'livedesign.room.last_connected',
      'livedesign.connection.max_reconnect',
      'livedesign.connection.reconnect_max_delay_ms',
      'livedesign.connection.heartbeat_interval_ms',
      'livedesign.connection.user_agent',
      'livedesign.connection.wbi_fetch_timeout_ms',
      'livedesign.connection.bili_api_timeout_ms',
    ],
    clarificationQuestion: '您想了解直播间连接的原理，还是想查看/修改具体的连接配置（如房间号、重连次数、心跳间隔）？',
  },

  [CONCEPT_IDS.DOMAIN_DISPLAY]: {
    conceptId: CONCEPT_IDS.DOMAIN_DISPLAY,
    displayName: '显示与窗口',
    explanation:
      '显示与窗口控制应用的界面和渲染行为。你可以设置布局模式、窗口尺寸与置顶、弹幕覆盖层、WebGL 渲染后端等。',
    boundaryKeywords: ['窗口怎么设置', '显示怎么设置', '布局怎么设置', '有哪些显示设置', '渲染是什么', '显示有哪些配置'],
    toolRoutes: { '/display': ['read_config_value', 'validate_config'] },
    relatedConfigKeys: [
      'livedesign.display.layout_mode',
      'livedesign.display.danmaku_overlay',
      'livedesign.display.window_ontop',
      'livedesign.display.window_width',
      'livedesign.display.window_height',
      'livedesign.display.window_min_width',
      'livedesign.display.window_min_height',
      'livedesign.rendering.webgl_backend',
      'livedesign.rendering.ignore_gpu_blocklist',
    ],
    clarificationQuestion: '您想了解显示与窗口功能，还是想调整某个具体的显示配置（如窗口尺寸、布局模式、渲染后端）？',
  },

  [CONCEPT_IDS.DOMAIN_AUTH]: {
    conceptId: CONCEPT_IDS.DOMAIN_AUTH,
    displayName: '认证与Cookie',
    explanation:
      '认证配置保存你的 B站登录 Cookie，用于访问需要身份验证的直播间数据。Cookie 是敏感信息，会加密存储在本地文件中。',
    boundaryKeywords: ['登录怎么设置', 'Cookie是什么', '怎么登录', '认证怎么设置', '有哪些认证配置'],
    toolRoutes: { '/scancookie': ['read_config_value', 'read_file'] },
    relatedConfigKeys: [
      'livedesign.auth.cookie',
    ],
    clarificationQuestion: '您想了解认证与 Cookie 的工作原理，还是需要查看当前的 Cookie 状态？',
  },
}

// ── 查找接口 ─────────────────────────────────────────────────────────

/**
 * 根据用户输入查找匹配的概念条目。
 *
 * 匹配方式：精确 + 包含匹配 boundaryKeywords。
 * 不涉及 TF-IDF 或 BM25，纯 O(n) 查表。
 *
 * @param userInput 用户原始输入
 * @returns 命中的概念条目，无命中返回 null
 */
export function findConceptByInput(userInput: string): BizKnowledgeEntry | null {
  if (!userInput || !userInput.trim()) return null

  const lowerInput = userInput.toLowerCase().trim()

  for (const entry of Object.values(BIZ_KNOWLEDGE)) {
    for (const kw of entry.boundaryKeywords) {
      const lowerKw = kw.toLowerCase()
      if (lowerKw === lowerInput || lowerKw.includes(lowerInput) || lowerInput.includes(lowerKw)) {
        return entry
      }
    }
  }

  return null
}

/**
 * 根据 conceptId 获取概念条目。
 */
export function getConceptById(conceptId: ConceptId): BizKnowledgeEntry | undefined {
  return BIZ_KNOWLEDGE[conceptId]
}

/**
 * 获取所有概念条目。
 */
export function getAllConcepts(): BizKnowledgeEntry[] {
  return Object.values(BIZ_KNOWLEDGE)
}
