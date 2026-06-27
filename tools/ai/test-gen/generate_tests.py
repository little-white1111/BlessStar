#!/usr/bin/env python3
"""
AI 自动化测试生成器 — 第36天

从 Schema JSON + Gate 链定义 生成五环节 AI 管线测试用例 JSON。
输出格式直接对接 app/editor/src/ai/pipeline/__tests__/pipeline-e2e.test.ts。

五环节测试模型：
  ① 模拟用户描述（原始文本）
  ② L0 → skillMatch / l0Hint / isMultiClause
  ③ 理解Agent → UA 四元组 {subject, operation, value, condition, is_chat}
  ④ 映射层 → operationToTools / L1 per-subject configKey
  ⑤ 回复Agent → wrapUp 含预期关键词

用法:
  python tools/ai/test-gen/generate_tests.py \
    --schema tools/ai/test-gen/schema_example.json \
    --gates tools/ai/test-gen/gates_example.json \
    --output generated-cases.json

  仅输出 livedesign 场景:
  python tools/ai/test-gen/generate_tests.py --domain livedesign --output ai-generated-cases.json
"""

import argparse
import json
import os
import sys
from typing import Any

# ── 15 种 operation 枚举 ─────────────────────────────────────────────
OPERATION_ENUM = {
    "READ":   "list",
    "WRITE":  "write",
    "LIST":   "read",
    "VALIDATE": "gate",
    "ADD_FIELD": "schema",
    "SET_RULE": "gate",
    "CREATE_RULE_CHAIN": "gate",
    "BROWSE": "search",
    "SEARCH": "search",
    "FIND":   "search",
    "VIEW_FILE": "read",
    "EXEC":   "search",
    "DIAGNOSE": "search",
    "GENERATE": "schema",
    "CHAT":   "chat",
}

# ── operation → 预期 tool 名称 ───────────────────────────────────────
OPERATION_TO_TOOLS: dict[str, list[str]] = {
    "READ":   ["read_config_value"],
    "WRITE":  ["read_config_value", "write_config_value"],
    "LIST":   ["list_configs"],
    "VALIDATE": ["validate_config"],
    "ADD_FIELD": ["add_config_field"],
    "SET_RULE": ["update_gate_rule"],
    "CREATE_RULE_CHAIN": ["create_gate_chain"],
    "BROWSE": ["browse_directory"],
    "SEARCH": ["search_files"],
    "FIND":   ["find_file"],
    "VIEW_FILE": ["view_file"],
    "EXEC":   ["execute_command"],
    "DIAGNOSE": ["diagnose"],
    "GENERATE": ["generate_normalizer"],
    "CHAT":   ["chat"],
}

# ── 内置 Schema（3 个场景）──────────────────────────────────────────
BUILTIN_SCHEMAS: dict[str, list[dict[str, Any]]] = {
    "livedesign": [
        {"key": "livedesign.room.room_id",     "type": "int32",  "default": "10041",  "label": "房间号",        "desc": "B站直播间ID（短号/长号）", "required": True},
        {"key": "livedesign.connection.max_reconnect", "type": "int32",  "default": "5",       "label": "最大重连次数",    "desc": "WebSocket 最大重连次数", "required": False},
        {"key": "livedesign.connection.heartbeat_interval_ms", "type": "int32", "default": "30000", "label": "心跳间隔", "desc": "WebSocket 心跳间隔（毫秒）", "required": False},
        {"key": "livedesign.danmaku.font_size",  "type": "int32",  "default": "14",      "label": "弹幕字号",      "desc": "弹幕字体大小（12-32）", "required": False},
        {"key": "livedesign.danmaku.font_color", "type": "string", "default": "#ffffff", "label": "弹幕颜色",      "desc": "弹幕字体颜色（十六进制）", "required": False},
        {"key": "livedesign.danmaku.block_like", "type": "bool",   "default": "false",   "label": "屏蔽点赞",      "desc": "是否屏蔽点赞播报弹幕", "required": False},
        {"key": "livedesign.danmaku.min_user_level", "type": "int32", "default": "0",    "label": "最低用户等级",    "desc": "弹幕用户等级门槛（0-80）", "required": False},
        {"key": "livedesign.display.window_width",  "type": "int32",  "default": "1200",   "label": "窗口宽度",      "desc": "应用窗口宽度（像素）", "required": False},
        {"key": "livedesign.display.window_height", "type": "int32",  "default": "800",    "label": "窗口高度",      "desc": "应用窗口高度（像素）", "required": False},
        {"key": "livedesign.display.window_ontop",  "type": "bool",   "default": "false",  "label": "窗口置顶",      "desc": "窗口是否总在最前", "required": False},
    ],
    "expense": [
        {"key": "expense.reimburse.max_amount", "type": "int32",  "default": "10000",  "label": "审批金额阈值",  "desc": "超过此金额需总监审批", "required": True},
        {"key": "expense.reimburse.min_amount",  "type": "int32",  "default": "50",     "label": "最低报销金额",  "desc": "低于此金额自动通过", "required": False},
        {"key": "expense.reimburse.tax_rate",    "type": "double", "default": "0.13",   "label": "税率",        "desc": "增值税税率", "required": False},
        {"key": "expense.reimburse.approver",    "type": "string", "default": "manager", "label": "审批人",     "desc": "默认审批人角色", "required": False},
        {"key": "expense.reimburse.dept_code",   "type": "string", "default": "FIN",    "label": "部门代码",    "desc": "财务部门代码", "required": True},
        {"key": "expense.reimburse.currency",    "type": "string", "default": "CNY",    "label": "货币类型",    "desc": "报销货币 ISO 4217 代码", "required": False},
        {"key": "expense.reimburse.require_invoice", "type": "bool", "default": "true", "label": "需要发票",  "desc": "是否必须上传发票", "required": False},
        {"key": "expense.reimburse.auto_approve",    "type": "bool", "default": "false", "label": "自动审批", "desc": "小额报销是否自动通过", "required": False},
        {"key": "expense.reimburse.max_days",    "type": "int32",  "default": "30",     "label": "报销有效期",  "desc": "报销申请有效天数", "required": False},
        {"key": "expense.reimburse.notify_email","type": "string", "default": "admin@example.com", "label": "通知邮箱", "desc": "审批结果通知邮箱", "required": False},
    ],
    "permission": [
        {"key": "permission.rbac.admin_role",   "type": "string", "default": "admin",   "label": "管理员角色名",  "desc": "系统管理员角色标识", "required": True},
        {"key": "permission.rbac.default_role", "type": "string", "default": "viewer",  "label": "默认角色",    "desc": "新用户默认角色", "required": False},
        {"key": "permission.rbac.session_ttl",  "type": "int32",  "default": "3600",   "label": "会话超时",    "desc": "用户会话超时时间（秒）", "required": False},
        {"key": "permission.rbac.mfa_required", "type": "bool",   "default": "false",  "label": "需要MFA",     "desc": "是否强制多因素认证", "required": False},
        {"key": "permission.rbac.max_failed_attempts", "type": "int32", "default": "5", "label": "最大登录尝试", "desc": "锁定前最大失败次数", "required": False},
        {"key": "permission.rbac.lockout_minutes",    "type": "int32", "default": "15", "label": "锁定时间",   "desc": "账号锁定时间（分钟）", "required": False},
        {"key": "permission.rbac.password_min_len",   "type": "int32", "default": "8",  "label": "密码最小长度", "desc": "用户密码最小字符数", "required": False},
        {"key": "permission.rbac.require_uppercase",  "type": "bool",  "default": "true", "label": "需要大写字母", "desc": "密码是否必须包含大写字母", "required": False},
        {"key": "permission.rbac.token_refresh_sec",  "type": "int32", "default": "900", "label": "Token刷新间隔","desc": "JWT Token 刷新间隔（秒）", "required": False},
        {"key": "permission.rbac.ip_whitelist",       "type": "string", "default": "",   "label": "IP白名单",   "desc": "允许访问的 IP 或 CIDR（逗号分隔）", "required": False},
    ],
}

# ── 内置 Gate 链 ─────────────────────────────────────────────────────
BUILTIN_GATES: dict[str, list[dict[str, Any]]] = {
    "livedesign": [
        {"gate_id": "room_id_positive",   "field": "livedesign.room.room_id",   "op": "gte", "value": "1",       "desc": "房间号必须为正整数"},
        {"gate_id": "font_size_range",    "field": "livedesign.danmaku.font_size", "op": "range", "value": "12-32", "desc": "弹幕字号范围 12-32"},
        {"gate_id": "min_user_level_max", "field": "livedesign.danmaku.min_user_level", "op": "lte", "value": "80", "desc": "用户等级上限 80"},
    ],
    "expense": [
        {"gate_id": "max_amount_upper",  "field": "expense.reimburse.max_amount",  "op": "lte", "value": "1000000", "desc": "单笔审批上限"},
        {"gate_id": "tax_rate_range",    "field": "expense.reimburse.tax_rate",    "op": "range", "value": "0.0-1.0", "desc": "税率范围 0-1"},
        {"gate_id": "max_days_range",    "field": "expense.reimburse.max_days",    "op": "range", "value": "1-365",  "desc": "有效期 1-365 天"},
    ],
    "permission": [
        {"gate_id": "session_ttl_min",       "field": "permission.rbac.session_ttl",       "op": "gte", "value": "60",   "desc": "会话超时至少 60 秒"},
        {"gate_id": "max_attempts_range",    "field": "permission.rbac.max_failed_attempts","op": "range","value": "1-20","desc": "登录尝试 1-20 次"},
        {"gate_id": "password_min_len_min",  "field": "permission.rbac.password_min_len",  "op": "gte", "value": "6",    "desc": "密码最小长度 >= 6"},
    ],
}


def load_schema(domain: str) -> list[dict[str, Any]]:
    """从内置 Schema 或 JSON 文件加载字段定义。"""
    if domain in BUILTIN_SCHEMAS:
        return BUILTIN_SCHEMAS[domain]
    raise ValueError(f"未知 domain: {domain}，可用: {list(BUILTIN_SCHEMAS.keys())}")


def load_gates(domain: str) -> list[dict[str, Any]]:
    """从内置 Gate 链或 JSON 文件加载 Gate 定义。"""
    if domain in BUILTIN_GATES:
        return BUILTIN_GATES[domain]
    return []


def _label_to_keys(fields: list[dict[str, Any]]) -> dict[str, str]:
    """构建中文 label → configKey 映射。"""
    return {f["label"]: f["key"] for f in fields}


def _all_labels(fields: list[dict[str, Any]]) -> list[str]:
    return [f["label"] for f in fields]


def gen_write_cases(fields: list[dict[str, Any]], domain: str) -> list[dict[str, Any]]:
    """为每个字段生成 WRITE 操作测试用例。"""
    label_map = _label_to_keys(fields)
    cases: list[dict[str, Any]] = []
    idx = 1
    for f in fields:
        default_val = f.get("default", "")
        label = f["label"]
        config_key = f["key"]
        # 为不同类型构造合适的测试值
        if f["type"] in ("int32", "int64"):
            new_val = str(int(default_val) * 2) if default_val.isdigit() else "123"
        elif f["type"] == "bool":
            new_val = "false" if default_val == "true" else "true"
        elif f["type"] == "double":
            new_val = str(float(default_val) * 2) if default_val.replace(".", "", 1).isdigit() else "1.5"
        else:
            new_val = '"测试值"'
        # 构造自然语言输入
        input_text = f"把{label}改成{new_val.strip('\"')}"
        cases.append({
            "id": f"AI-TC-AUTO-{domain.upper()}-WRITE-{idx:02d}",
            "input": input_text,
            "scenario": f"自动生成: WRITE {label} → {config_key}",
            "expectTools": True,
            "minToolCalls": 2,
            "mustIncludeTools": ["write_config_value"],
            "intentCount": 1,
            "expectChatReply": False,
            "mustContainPlanKeywords": ["WRITE", label],
            "mustContainResultKeywords": [f"已将", label],
            "mustNotContainMarkers": [],
            "manualOnly": False,
            "note": f"自动生成 · domain={domain} · configKey={config_key} · type={f['type']}",
        })
        idx += 1
    return cases


def gen_read_cases(fields: list[dict[str, Any]], domain: str) -> list[dict[str, Any]]:
    """为字段生成 READ 操作测试用例。"""
    cases: list[dict[str, Any]] = []
    idx = 1
    for f in fields[:5]:  # 前 5 个字段
        label = f["label"]
        config_key = f["key"]
        input_text = f"查看{label}"
        cases.append({
            "id": f"AI-TC-AUTO-{domain.upper()}-READ-{idx:02d}",
            "input": input_text,
            "scenario": f"自动生成: READ {label} → {config_key}",
            "expectTools": True,
            "minToolCalls": 1,
            "mustIncludeTools": ["read_config_value"],
            "intentCount": 1,
            "expectChatReply": False,
            "mustContainPlanKeywords": ["READ", label],
            "mustContainResultKeywords": [],
            "mustNotContainMarkers": ["证据不足"],
            "manualOnly": False,
            "note": f"自动生成 · domain={domain} · configKey={config_key}",
        })
        idx += 1
    return cases


def gen_gate_cases(gates: list[dict[str, Any]], domain: str) -> list[dict[str, Any]]:
    """为 Gate 规则生成测试用例。"""
    cases: list[dict[str, Any]] = []
    idx = 1
    for g in gates:
        desc = g.get("desc", g["gate_id"])
        input_text = f"给{g['field'].split('.')[-1]}加个校验，{desc}"
        cases.append({
            "id": f"AI-TC-AUTO-{domain.upper()}-GATE-{idx:02d}",
            "input": input_text,
            "scenario": f"自动生成: GATE {g['gate_id']} → {g['field']} {g['op']} {g['value']}",
            "expectTools": True,
            "minToolCalls": 1,
            "mustIncludeTools": ["create_gate_chain"],
            "intentCount": 1,
            "expectChatReply": False,
            "mustContainPlanKeywords": ["CREATE_RULE_CHAIN"],
            "mustContainResultKeywords": [],
            "mustNotContainMarkers": ["证据不足"],
            "manualOnly": False,
            "note": f"自动生成 · domain={domain} · gate_id={g['gate_id']} · {g['op']}({g['field']}, {g['value']})",
        })
        idx += 1
    return cases


def gen_list_case(domain: str, fields: list[dict[str, Any]]) -> list[dict[str, Any]]:
    """生成 LIST 操作测试用例。"""
    return [{
        "id": f"AI-TC-AUTO-{domain.upper()}-LIST-01",
        "input": "当前有哪些配置",
        "scenario": f"自动生成: LIST {domain} 全部配置",
        "expectTools": True,
        "minToolCalls": 1,
        "mustIncludeTools": ["list_configs"],
        "intentCount": 1,
        "expectChatReply": False,
        "mustContainPlanKeywords": ["LIST", "所有配置项"],
        "mustContainResultKeywords": [],
        "mustNotContainMarkers": ["证据不足"],
        "manualOnly": False,
        "note": f"自动生成 · domain={domain} · {len(fields)} 字段",
    }]


def gen_mixed_cases(fields: list[dict[str, Any]], domain: str) -> list[dict[str, Any]]:
    """生成混合意图测试用例（LIST + WRITE + CHAT）。"""
    if len(fields) < 2:
        return []
    label_map = _label_to_keys(fields)
    labels = _all_labels(fields)
    case = {
        "id": f"AI-TC-AUTO-{domain.upper()}-MIXED-01",
        "input": f"当前有哪些配置，帮我把{labels[0]}改成42，gate是什么",
        "scenario": f"自动生成: MIXED LIST+WRITE+CHAT ({domain})",
        "expectTools": True,
        "minToolCalls": 3,
        "mustIncludeTools": ["list_configs", "write_config_value"],
        "intentCount": 3,
        "expectChatReply": True,
        "mustContainPlanKeywords": ["LIST", "WRITE"],
        "mustContainResultKeywords": [f"已列出", f"已将", "门禁"],
        "mustNotContainMarkers": ["证据不足", "无 Registry"],
        "manualOnly": False,
        "note": f"自动生成 · domain={domain} · per-item is_chat 分流验证",
    }
    return [case]


def gen_l1_unresolved_case(fields: list[dict[str, Any]], domain: str) -> list[dict[str, Any]]:
    """生成 L1 未命中边界用例。"""
    return [{
        "id": f"AI-TC-AUTO-{domain.upper()}-L1UNR-01",
        "input": "把弹幕字体大小改成20",
        "scenario": f"自动生成: L1_UNRESOLVED 未命中 ({domain})",
        "expectTools": False,
        "minToolCalls": 0,
        "mustIncludeTools": [],
        "intentCount": 1,
        "expectChatReply": False,
        "mustContainPlanKeywords": [],
        "mustContainResultKeywords": ["未找到"],
        "mustNotContainMarkers": [],
        "manualOnly": False,
        "note": f"自动生成 · domain={domain} · L1 三步兜底全失败 → l1_unresolved 回问",
    }]


def generate_cases(domains: list[str]) -> list[dict[str, Any]]:
    """为指定 domain 列表生成完整测试用例集。"""
    all_cases: list[dict[str, Any]] = []
    for domain in domains:
        fields = load_schema(domain)
        gates = load_gates(domain)
        all_cases.extend(gen_list_case(domain, fields))
        all_cases.extend(gen_read_cases(fields, domain))
        all_cases.extend(gen_write_cases(fields, domain))
        all_cases.extend(gen_gate_cases(gates, domain))
        all_cases.extend(gen_mixed_cases(fields, domain))
        all_cases.extend(gen_l1_unresolved_case(fields, domain))
    return all_cases


def main():
    parser = argparse.ArgumentParser(
        description="AI 管线自动化测试生成器 — 从 Schema + Gate 链生成五环节测试用例 JSON",
    )
    parser.add_argument(
        "--schema", type=str, default=None,
        help="Schema JSON 文件路径（不指定则使用内置 livedesign+expense+permission）",
    )
    parser.add_argument(
        "--gates", type=str, default=None,
        help="Gate 链 JSON 文件路径（不指定则使用内置 Gate 定义）",
    )
    parser.add_argument(
        "--domain", type=str, default="livedesign,expense,permission",
        help="内置 domain，逗号分隔（默认 livedesign,expense,permission）",
    )
    parser.add_argument(
        "--output", "-o", type=str, required=True,
        help="输出 JSON 文件路径",
    )
    args = parser.parse_args()

    # 确定 domain 列表
    domains = [d.strip() for d in args.domain.split(",") if d.strip()]

    # 加载自定义 Schema/Gate（如提供）
    if args.schema:
        with open(args.schema, "r", encoding="utf-8") as f:
            custom_schema = json.load(f)
        BUILTIN_SCHEMAS["custom"] = custom_schema
        domains = ["custom"]

    if args.gates:
        with open(args.gates, "r", encoding="utf-8") as f:
            custom_gates = json.load(f)
        BUILTIN_GATES["custom"] = custom_gates

    cases = generate_cases(domains)

    output = {
        "generator": "tools/ai/test-gen/generate_tests.py",
        "generatedAt": "",
        "domains": domains,
        "totalCases": len(cases),
        "description": "AI 管线五环节自动化测试集 — 对接 pipeline-e2e.test.ts",
        "testCases": cases,
    }

    # 写入输出文件
    out_path = os.path.abspath(args.output)
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(output, f, ensure_ascii=False, indent=2)

    # 摘要
    by_category: dict[str, int] = {}
    for c in cases:
        cat = c["id"].split("-")[4] if len(c["id"].split("-")) >= 5 else "OTHER"
        by_category[cat] = by_category.get(cat, 0) + 1

    print(f"✅ 生成 {len(cases)} 条测试用例 → {out_path}")
    print(f"   Domain: {', '.join(domains)}")
    for cat, cnt in sorted(by_category.items()):
        print(f"     {cat}: {cnt} 条")


if __name__ == "__main__":
    main()
