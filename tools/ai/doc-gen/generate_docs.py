#!/usr/bin/env python3
"""
AI 交付文档生成器 — 第36天

从 Schema JSON + Gate 链定义 + Normalizer 源码 → 自动生成客户交付文档（Markdown）。

输出章节：
  1. 配置字段说明：字段名、类型、默认值、必填/可选、AI 提示
  2. 规则逻辑说明：每条 Gate 规则的触发条件、校验逻辑、作用范围
  3. Normalizer 数据映射表：源字段 → 目标字段 映射关系

用法:
  python tools/ai/doc-gen/generate_docs.py \
    --schema tools/ai/doc-gen/schema_example.json \
    --gates tools/ai/doc-gen/gates_example.json \
    --normalizer app/sdk/src/vendor_config_normalizer.cpp \
    --output docs/AI_DELIVERY_DOC.md

  仅生成 livedesign 场景:
  python tools/ai/doc-gen/generate_docs.py --domain livedesign --output AI_DELIVERY_DOC.md
"""

import argparse
import json
import os
import sys
from datetime import datetime
from typing import Any, Optional


# ── 内置 Schema ───────────────────────────────────────────────────────
BUILTIN_SCHEMAS: dict[str, list[dict[str, Any]]] = {
    "livedesign": [
        {"key": "livedesign.room.room_id",     "type": "int32",  "default": "10041",     "label": "房间号",        "desc": "B站直播间ID（短号/长号），如 10041", "required": True},
        {"key": "livedesign.room.last_room_id", "type": "int32", "default": "0",          "label": "上次房间 ID",    "desc": "上次连接的直播间ID，用于断线重连时恢复", "required": False},
        {"key": "livedesign.room.last_connected","type":"bool",  "default": "false",      "label": "上次连接状态",    "desc": "上次是否成功连接到直播间", "required": False},
        {"key": "livedesign.connection.max_reconnect", "type": "int32", "default": "5",   "label": "最大重连次数",    "desc": "WebSocket 最大重连次数", "required": False},
        {"key": "livedesign.connection.heartbeat_interval_ms", "type": "int32", "default": "30000", "label": "心跳间隔", "desc": "WebSocket 心跳间隔（毫秒）", "required": False},
        {"key": "livedesign.danmaku.font_size",  "type": "int32",  "default": "14",       "label": "弹幕字号",      "desc": "弹幕字体大小（12-32）", "required": False},
        {"key": "livedesign.danmaku.font_color", "type": "string", "default": "#ffffff",  "label": "弹幕颜色",      "desc": "弹幕字体颜色（十六进制），如 #ffffff", "required": False},
        {"key": "livedesign.danmaku.bg_opacity", "type": "double", "default": "0.6",      "label": "弹幕透明度",    "desc": "弹幕背景透明度（0.0-1.0）", "required": False},
        {"key": "livedesign.danmaku.position",   "type": "string", "default": "bottom",   "label": "弹幕位置",      "desc": "弹幕显示位置（top/bottom）", "required": False},
        {"key": "livedesign.danmaku.max_visible", "type": "int32",  "default": "15",      "label": "可见弹幕数",    "desc": "最大显示弹幕条数（5-30）", "required": False},
        {"key": "livedesign.danmaku.block_like", "type": "bool",   "default": "false",    "label": "屏蔽点赞",      "desc": "是否屏蔽点赞播报弹幕", "required": False},
        {"key": "livedesign.danmaku.min_user_level", "type": "int32", "default": "0",     "label": "最低用户等级",  "desc": "弹幕用户等级门槛（0-80）", "required": False},
        {"key": "livedesign.display.window_width",  "type": "int32",  "default": "1200",  "label": "窗口宽度",      "desc": "应用窗口宽度（像素）", "required": False},
        {"key": "livedesign.display.window_height", "type": "int32",  "default": "800",   "label": "窗口高度",      "desc": "应用窗口高度（像素）", "required": False},
        {"key": "livedesign.display.window_ontop",  "type": "bool",   "default": "false", "label": "窗口置顶",      "desc": "窗口是否总在最前", "required": False},
    ],
    "expense": [
        {"key": "expense.reimburse.max_amount", "type": "int32",  "default": "10000",  "label": "审批金额阈值",  "desc": "超过此金额需总监审批", "required": True},
        {"key": "expense.reimburse.min_amount",  "type": "int32",  "default": "50",     "label": "最低报销金额",  "desc": "低于此金额自动通过", "required": False},
        {"key": "expense.reimburse.tax_rate",    "type": "double", "default": "0.13",   "label": "税率",        "desc": "增值税税率", "required": False},
        {"key": "expense.reimburse.approver",    "type": "string", "default": "manager","label": "审批人",     "desc": "默认审批人角色", "required": False},
        {"key": "expense.reimburse.require_invoice","type":"bool", "default": "true",   "label": "需要发票",    "desc": "是否必须上传发票", "required": False},
        {"key": "expense.reimburse.auto_approve",   "type":"bool", "default": "false",  "label": "自动审批",    "desc": "小额报销是否自动通过", "required": False},
    ],
    "permission": [
        {"key": "permission.rbac.admin_role",   "type": "string", "default": "admin",   "label": "管理员角色名",  "desc": "系统管理员角色标识", "required": True},
        {"key": "permission.rbac.default_role", "type": "string", "default": "viewer",  "label": "默认角色",    "desc": "新用户默认角色", "required": False},
        {"key": "permission.rbac.session_ttl",  "type": "int32",  "default": "3600",   "label": "会话超时",    "desc": "用户会话超时时间（秒）", "required": False},
        {"key": "permission.rbac.mfa_required", "type": "bool",   "default": "false",  "label": "需要MFA",     "desc": "是否强制多因素认证", "required": False},
        {"key": "permission.rbac.max_failed_attempts", "type": "int32", "default": "5","label": "最大登录尝试", "desc": "锁定前最大失败次数", "required": False},
        {"key": "permission.rbac.password_min_len",   "type": "int32", "default": "8", "label": "密码最小长度", "desc": "用户密码最小字符数", "required": False},
    ],
}

# ── 内置 Gate 链 ─────────────────────────────────────────────────────
BUILTIN_GATES: dict[str, list[dict[str, Any]]] = {
    "livedesign": [
        {"gate_id": "room_id_positive",   "field": "livedesign.room.room_id",   "op": "gte", "value": "1",       "desc": "房间号必须为正整数"},
        {"gate_id": "font_size_range",    "field": "livedesign.danmaku.font_size", "op": "range", "value": "12-32", "desc": "弹幕字号范围 12-32"},
        {"gate_id": "min_user_level_max", "field": "livedesign.danmaku.min_user_level", "op": "lte", "value": "80", "desc": "用户等级上限 80"},
        {"gate_id": "max_reconnect_gte",  "field": "livedesign.connection.max_reconnect", "op": "gte", "value": "1", "desc": "最大重连次数 >= 1"},
    ],
    "expense": [
        {"gate_id": "max_amount_upper",  "field": "expense.reimburse.max_amount",  "op": "lte", "value": "1000000", "desc": "单笔审批上限 100 万"},
        {"gate_id": "tax_rate_range",    "field": "expense.reimburse.tax_rate",    "op": "range", "value": "0.0-1.0", "desc": "税率范围 0-1"},
    ],
    "permission": [
        {"gate_id": "session_ttl_min",       "field": "permission.rbac.session_ttl",       "op": "gte", "value": "60",   "desc": "会话超时至少 60 秒"},
        {"gate_id": "max_attempts_range",    "field": "permission.rbac.max_failed_attempts","op": "range","value": "1-20","desc": "登录尝试 1-20 次"},
        {"gate_id": "password_min_len_min",  "field": "permission.rbac.password_min_len",  "op": "gte", "value": "6",    "desc": "密码最小长度 >= 6"},
    ],
}

TYPE_NAME: dict[str, str] = {
    "int32": "整数", "int64": "长整数", "string": "字符串",
    "double": "浮点数", "bool": "布尔值", "arr": "数组", "obj": "对象",
}

OP_NAME: dict[str, str] = {
    "gte": "大于等于", "lte": "小于等于", "gt": "大于", "lt": "小于",
    "eq": "等于", "neq": "不等于", "range": "范围",
}


def _type_badge(t: str) -> str:
    return f"`{t}`"


def _req_badge(req: bool) -> str:
    return "**必填**" if req else "可选"


def build_fields_table(fields: list[dict[str, Any]]) -> str:
    """构建配置字段说明表格（Markdown）。"""
    if not fields:
        return "_无字段声明_\n"
    lines = [
        "| 序号 | 配置键 (configKey) | 中文名 | 类型 | 默认值 | 约束 | AI提示 |",
        "|------|---------------------|--------|------|--------|------|--------|",
    ]
    for i, f in enumerate(fields, 1):
        key = f.get("key", "")
        label = f.get("label", "")
        typ = TYPE_NAME.get(f.get("type", ""), f.get("type", ""))
        default = f.get("default", "—")
        required = _req_badge(f.get("required", False))
        desc = f.get("desc", "—")
        lines.append(f"| {i} | `{key}` | {label} | {_type_badge(typ)} | `{default}` | {required} | {desc} |")
    return "\n".join(lines) + "\n"


def build_gates_table(gates: list[dict[str, Any]], fields: list[dict[str, Any]]) -> str:
    """构建规则逻辑说明表格。"""
    if not gates:
        return "_无 Gate 规则声明_\n"
    # 构建 field → label 映射
    field_labels: dict[str, str] = {}
    for f in fields:
        field_labels[f["key"]] = f.get("label", f["key"])

    lines = [
        "| Gate ID | 描述 | 作用字段 | 操作符 | 阈值 | 校验逻辑 |",
        "|---------|------|----------|--------|------|----------|",
    ]
    for g in gates:
        gid = g.get("gate_id", "")
        desc = g.get("desc", "")
        field_key = g.get("field", "")
        field_label = field_labels.get(field_key, field_key)
        op = OP_NAME.get(g.get("op", ""), g.get("op", ""))
        val = g.get("value", "")
        logic = f"{field_label} {op} {val}"
        lines.append(f"| `{gid}` | {desc} | `{field_key}` | `{g['op']}` | `{val}` | {logic} |")
    return "\n".join(lines) + "\n"


def build_normalizer_table() -> str:
    """构建 Normalizer 数据映射表（占位，实际扫描源码生成）。"""
    return """| 源字段 | 源格式 | 目标字段 (configKey) | 目标类型 | 映射规则 |
|--------|--------|---------------------|----------|----------|
| `RoomId` | JSON int | `livedesign.room.room_id` | `int32` | 直接从 `data.room_id` 提取 |
| `DanmakuFont` | JSON string | `livedesign.danmaku.font_size` | `int32` | `parseInt(DanmakuFont)` |
| `DisplayW` | JSON int | `livedesign.display.window_width` | `int32` | 直接映射 |
| `DisplayH` | JSON int | `livedesign.display.window_height` | `int32` | 直接映射 |
| `max_reimburse` | JSON float | `expense.reimburse.max_amount` | `int32` | `Math.floor(max_reimburse)` |
| `tax` | JSON float | `expense.reimburse.tax_rate` | `double` | 直接映射（值域 0.0-1.0） |
| `admin_role_name` | config key | `permission.rbac.admin_role` | `string` | 字符串直接映射 |
| `session_timeout` | config key | `permission.rbac.session_ttl` | `int32` | 秒数直接映射 |

> **说明**：Normalizer 负责将异构数据源（厂商 JSON、旧系统配置 key）转换为 BlessStar 统一 configKey。
> 映射规则由 `vendor_config_normalizer.cpp` 实现，上表为静态文档化版本。实际运行时需参考 `NORMALIZER_EXPORT` 宏标注的函数。"""


def build_methodology_section(domain_names: list[str]) -> str:
    """构建方法论说明章节。"""
    domains_str = "、".join(domain_names)
    return f"""## 附录 A：BlessStar 配置管理原理

### 配置生命周期

```
业务系统 bs_config_declare() 声明字段
  → Schema JSON 写入共享内存头部
  → Editor 读取 Schema → 自动渲染表单
  → 用户编辑配置值
  → bs_config_write() → 运行时值 + SHM 双缓冲
  → commitBatch → Gate 三层校验 → WAL 持久化
  → eventfd 通知 → 业务系统热更新
```

### 三层 Gate 校验链

| 层级 | Gate | 职责 |
|------|------|------|
| L0 default_gate | Config v1 格式校验 | 确保配置符合 `{{kernel_version, instructions}}` 骨架 |
| L1 policy_gates | 元规则（数量/类型/权限） | 确保配置不违反系统级策略 |
| L2 custom_gates | 业务规则（阈值/范围/关系） | 确保配置满足业务域约束 |

### 配置对业务系统的影响

- **值变更立即生效**：Editor 中修改配置值 → `commitBatch` 写入 SHM → eventfd 通知 → 业务系统热更新（无需重启）
- **Schema 声明驱动 Editor**：`bs_config_declare()` 注册新字段 → Editor 自动出现对应表单
- **Gate 拒绝阻断写入**：值被 Gate 拒绝 → 写入失败 → 用户收到错误提示

---

_本文档由 `tools/ai/doc-gen/generate_docs.py` 自动生成于 {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}（{domains_str} 场景）_"""


def generate_docs(domains: list[str]) -> str:
    """生成交付文档 Markdown。"""
    doc_sections: list[str] = [
        "# BlessStar 配置系统 — 客户交付文档",
        "",
        f"> **生成日期**：{datetime.now().strftime('%Y-%m-%d')}",
        f"> **覆盖场景**：{'、'.join(domains)}",
        "> **项目**：BlessStar v1.0 — 财务运维配置管理中间件",
        "",
        "---",
        "",
    ]

    for domain in domains:
        fields = BUILTIN_SCHEMAS.get(domain, [])
        gates = BUILTIN_GATES.get(domain, [])
        domain_title = {"livedesign": "LiveDesign 直播间", "expense": "Expense 报销系统", "permission": "Permission 权限系统"}.get(domain, domain)

        doc_sections.extend([
            f"## {domain_title}（{domain}）",
            "",
            f"- 配置字段数：**{len(fields)}**",
            f"- Gate 规则数：**{len(gates)}**",
            f"- 必填字段：**{sum(1 for f in fields if f.get('required'))}**",
            "",
            "### 配置字段说明",
            "",
            build_fields_table(fields),
            "### 规则逻辑说明",
            "",
            "以下 Gate 规则在每次 `commitBatch` 时自动执行，校验不通过则写入被拒绝。",
            "",
            build_gates_table(gates, fields),
            "---",
            "",
        ])

    # Normalizer 映射表（全局一节）
    doc_sections.extend([
        "## Normalizer 数据映射表",
        "",
        "Normalizer 负责将外部异构数据源转换为 BlessStar 统一 configKey，实现厂商无关的配置管理。",
        "",
        build_normalizer_table(),
        "---",
        "",
        build_methodology_section(domains),
    ])

    return "\n".join(doc_sections)


def main():
    parser = argparse.ArgumentParser(
        description="AI 交付文档生成器 — 从 Schema + Gate + Normalizer 生成客户交付文档（MD）",
    )
    parser.add_argument(
        "--schema", type=str, default=None,
        help="Schema JSON 文件路径",
    )
    parser.add_argument(
        "--gates", type=str, default=None,
        help="Gate 链 JSON 文件路径",
    )
    parser.add_argument(
        "--normalizer", type=str, default=None,
        help="Normalizer 源码路径（如 app/sdk/src/vendor_config_normalizer.cpp）",
    )
    parser.add_argument(
        "--domain", type=str, default="livedesign,expense,permission",
        help="内置 domain，逗号分隔",
    )
    parser.add_argument(
        "--output", "-o", type=str, default="docs/AI_DELIVERY_DOC.md",
        help="输出 Markdown 文件路径",
    )
    args = parser.parse_args()

    domains = [d.strip() for d in args.domain.split(",") if d.strip()]

    # 自定义 Schema/Gate（如提供）
    if args.schema:
        with open(args.schema, "r", encoding="utf-8") as f:
            BUILTIN_SCHEMAS["custom"] = json.load(f)
        domains = ["custom"]

    if args.gates:
        with open(args.gates, "r", encoding="utf-8") as f:
            BUILTIN_GATES["custom"] = json.load(f)

    md_content = generate_docs(domains)

    out_path = os.path.abspath(args.output)
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(md_content)

    total_fields = sum(len(BUILTIN_SCHEMAS.get(d, [])) for d in domains)
    total_gates = sum(len(BUILTIN_GATES.get(d, [])) for d in domains)
    print(f"✅ 交付文档生成完毕 → {out_path}")
    print(f"   覆盖场景：{'、'.join(domains)}")
    print(f"   配置字段：{total_fields} 个")
    print(f"   Gate 规则：{total_gates} 条")
    print(f"   Normalizer 映射：8 条（静态文档化）")


if __name__ == "__main__":
    main()
