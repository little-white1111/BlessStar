-- 用户叙事与动态画像系统 — SQLite 建表 DDL
-- 架构不变量 B1/B5/B6

-- L1: 原始观察层（B1: 所有 Agent 输出必须先写入此层）
CREATE TABLE IF NOT EXISTS observations (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id TEXT NOT NULL,
    user_id TEXT NOT NULL DEFAULT 'default',
    content TEXT NOT NULL,
    emotion TEXT NOT NULL DEFAULT 'neutral',
    intensity REAL NOT NULL DEFAULT 0.5,
    valence REAL NOT NULL DEFAULT 0.5,
    arousal REAL NOT NULL DEFAULT 0.5,
    dominance REAL NOT NULL DEFAULT 0.5,
    significant_mark INTEGER NOT NULL DEFAULT 0,
    created_at INTEGER NOT NULL
);

-- L2: 量化指标（B2: 由程序自动计算）
CREATE TABLE IF NOT EXISTS quantitative_metrics (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id TEXT NOT NULL DEFAULT 'default',
    metric_type TEXT NOT NULL,
    metric_name TEXT NOT NULL,
    value REAL NOT NULL,
    window_size INTEGER NOT NULL DEFAULT 0,
    period_start INTEGER NOT NULL,
    period_end INTEGER NOT NULL,
    created_at INTEGER NOT NULL
);

-- L3: 反思假设（B3: 必须标注 confidence；B6: 携带 version 时间戳）
CREATE TABLE IF NOT EXISTS reflective_hypotheses (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id TEXT NOT NULL DEFAULT 'default',
    version TEXT NOT NULL,
    domain TEXT NOT NULL,
    hypothesis TEXT NOT NULL,
    evidence TEXT DEFAULT '',
    confidence REAL NOT NULL,
    observation_refs TEXT NOT NULL DEFAULT '[]',
    created_at INTEGER NOT NULL
);

-- L4: 动态画像快照
CREATE TABLE IF NOT EXISTS profile_snapshots (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id TEXT NOT NULL DEFAULT 'default',
    version TEXT NOT NULL,
    profile_json TEXT NOT NULL,
    created_at INTEGER NOT NULL
);

-- 索引
CREATE INDEX IF NOT EXISTS idx_observations_user_id ON observations(user_id);
CREATE INDEX IF NOT EXISTS idx_observations_created_at ON observations(created_at);
CREATE INDEX IF NOT EXISTS idx_observations_significant ON observations(significant_mark);
CREATE INDEX IF NOT EXISTS idx_quantitative_user_id ON quantitative_metrics(user_id);
CREATE INDEX IF NOT EXISTS idx_quantitative_type ON quantitative_metrics(metric_type);
CREATE INDEX IF NOT EXISTS idx_hypotheses_user_id ON reflective_hypotheses(user_id);
CREATE INDEX IF NOT EXISTS idx_hypotheses_version ON reflective_hypotheses(version);
CREATE INDEX IF NOT EXISTS idx_profile_user_id ON profile_snapshots(user_id);
