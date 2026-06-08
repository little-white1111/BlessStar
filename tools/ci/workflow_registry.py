"""BlessStar GitHub Actions workflow registry for CI dispatch loop."""

from __future__ import annotations

from typing import Any, TypedDict


class RouteConfig(TypedDict, total=False):
    workflow_file: str
    workflow_name: str
    suite: str
    timeout: int
    mode_a_default_dispatch_ref: str
    supports_branch_input: bool
    extra_inputs: dict[str, str]


class TargetConfig(TypedDict):
    label: str
    via_ci: RouteConfig
    direct: RouteConfig


# Keys match GitHub Actions sidebar / -Target parameter.
TARGETS: dict[str, TargetConfig] = {
    "ci": {
        "label": "Full regression (ci.yml · suite=full)",
        "via_ci": {
            "workflow_file": "ci.yml",
            "workflow_name": "full test",
            "suite": "full",
            "timeout": 3600,
            "mode_a_default_dispatch_ref": "main",
            "supports_branch_input": True,
        },
        "direct": {
            "workflow_file": "ci.yml",
            "workflow_name": "full test",
            "suite": "full",
            "timeout": 3600,
            "supports_branch_input": False,
        },
    },
    "day21": {
        "label": "Day21 kernel_pool + TSan (day21.yml)",
        "via_ci": {
            "workflow_file": "ci.yml",
            "workflow_name": "full test",
            "suite": "day21",
            "timeout": 1800,
            "mode_a_default_dispatch_ref": "main",
            "supports_branch_input": True,
        },
        "direct": {
            "workflow_file": "day21.yml",
            "workflow_name": "day21",
            "timeout": 1800,
            "supports_branch_input": False,
        },
    },
    "day19-smoke": {
        "label": "Day19 smoke 900s (day19-stress-smoke.yml)",
        "via_ci": {
            "workflow_file": "ci.yml",
            "workflow_name": "full test",
            "suite": "day19-smoke",
            "timeout": 1800,
            "mode_a_default_dispatch_ref": "main",
            "supports_branch_input": True,
        },
        "direct": {
            "workflow_file": "day19-stress-smoke.yml",
            "workflow_name": "day19-stress-smoke",
            "timeout": 1800,
            "supports_branch_input": False,
        },
    },
    "day19-smoke-fail": {
        "label": "Day19 smoke_fail 900s (day19-stress-smoke-fail.yml)",
        "via_ci": {
            "workflow_file": "ci.yml",
            "workflow_name": "full test",
            "suite": "day19-smoke-fail",
            "timeout": 1800,
            "mode_a_default_dispatch_ref": "main",
            "supports_branch_input": True,
        },
        "direct": {
            "workflow_file": "day19-stress-smoke-fail.yml",
            "workflow_name": "day19-stress-smoke-fail",
            "timeout": 1800,
            "supports_branch_input": False,
        },
    },
    "day19-gha-6h": {
        "label": "Day19 gha_6h ~5h50m (day19-stress-gha-6h.yml)",
        "via_ci": {
            "workflow_file": "ci.yml",
            "workflow_name": "full test",
            "suite": "day19-gha-6h",
            "timeout": 21600,
            "mode_a_default_dispatch_ref": "main",
            "supports_branch_input": True,
        },
        "direct": {
            "workflow_file": "day19-stress-gha-6h.yml",
            "workflow_name": "day19-stress-gha-6h",
            "timeout": 21600,
            "supports_branch_input": False,
        },
    },
    "day19-full": {
        "label": "Day19 full 72h self-hosted (day19-stress-full.yml)",
        "via_ci": {
            "workflow_file": "ci.yml",
            "workflow_name": "full test",
            "suite": "day19-full",
            "timeout": 270000,
            "mode_a_default_dispatch_ref": "main",
            "supports_branch_input": True,
            "extra_inputs": {"day19_runner": "self-hosted"},
        },
        "direct": {
            "workflow_file": "day19-stress-full.yml",
            "workflow_name": "day19-stress-full",
            "timeout": 270000,
            "supports_branch_input": False,
            "extra_inputs": {"runner": "self-hosted"},
        },
    },
}

# Map workflow filename (or legacy alias) -> target key
FILE_ALIASES: dict[str, str] = {
    "ci.yml": "ci",
    "full": "ci",
    "day21.yml": "day21",
    "day19-stress-smoke.yml": "day19-smoke",
    "day19-stress-smoke-fail.yml": "day19-smoke-fail",
    "day19-stress-gha-6h.yml": "day19-gha-6h",
    "day19-stress-full.yml": "day19-full",
}


def normalize_target(name: str) -> str:
    key = name.strip().lower()
    if key in TARGETS:
        return key
    if key in FILE_ALIASES:
        return FILE_ALIASES[key]
    if key.endswith(".yml"):
        return FILE_ALIASES.get(key, key)
    raise ValueError(
        f"unknown target {name!r}; valid: {', '.join(sorted(TARGETS))}"
    )


def resolve_dispatch(
    *,
    target: str,
    test_ref: str,
    dispatch_ref: str | None,
    route: str,
) -> dict[str, Any]:
    """Return dispatch plan: workflow_file, workflow_name, refs, inputs, timeout."""
    if route not in ("via-ci", "direct"):
        raise ValueError(f"route must be via-ci or direct, got {route!r}")

    key = normalize_target(target)
    cfg = TARGETS[key]
    plan = cfg["via_ci" if route == "via-ci" else "direct"]

    workflow_file = plan["workflow_file"]
    workflow_name = plan["workflow_name"]
    timeout = plan.get("timeout", 3600)

    inputs: dict[str, str] = dict(plan.get("extra_inputs") or {})

    if route == "via-ci":
        suite = plan.get("suite", "full")
        if suite != "full":
            inputs["suite"] = suite
        default_dispatch = plan.get("mode_a_default_dispatch_ref", "main")
        eff_dispatch = dispatch_ref or default_dispatch
        supports_branch = plan.get("supports_branch_input", True)
        if supports_branch and eff_dispatch != test_ref:
            inputs["branch"] = test_ref
            use_branch_input = True
        else:
            use_branch_input = False
            if eff_dispatch == test_ref:
                inputs.pop("branch", None)
        watch_branch = eff_dispatch
    else:
        eff_dispatch = dispatch_ref or test_ref
        watch_branch = eff_dispatch
        use_branch_input = False

    return {
        "target": key,
        "route": route,
        "label": cfg["label"],
        "workflow_file": workflow_file,
        "workflow_name": workflow_name,
        "test_ref": test_ref,
        "dispatch_ref": eff_dispatch,
        "watch_branch": watch_branch,
        "use_branch_input": use_branch_input,
        "inputs": inputs,
        "timeout": timeout,
    }
