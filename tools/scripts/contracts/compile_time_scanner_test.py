#!/usr/bin/env python3
"""
Tests for compile_time_scanner.py and gate_rule_def compilation.
"""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from typing import Any

# Ensure the lib and contracts directories are on the path
_HERE = Path(__file__).resolve().parent
_LIB = _HERE.parent / "lib"
if str(_HERE) not in sys.path:
    sys.path.insert(0, str(_HERE))
if str(_LIB) not in sys.path:
    sys.path.insert(0, str(_LIB))

from compile_time_scanner import (
    _parse_diff,
    _eval_range,
    _eval_compare,
    _eval_in,
    _eval_rule,
    _is_dynamic_ref,
    scan,
)
from contract_schema import validate_gate_rule_def, is_compile_time_rule


# ══════════════════════════════════════════════════════════════════════
# Unit Tests: Git diff parsing
# ══════════════════════════════════════════════════════════════════════
class TestParseDiff(unittest.TestCase):
    def test_parse_yaml_diff(self):
        diff = """diff --git a/config.yaml b/config.yaml
--- a/config.yaml
+++ b/config.yaml
@@ -1,5 +1,5 @@
 server:
   port: 8080
-payment.fee.rate: 0.003
+payment.fee.rate: 0.008
 payment.timeout: 30
 """
        result = _parse_diff(diff)
        self.assertIn("config.yaml", result)
        self.assertEqual(result["config.yaml"].get("payment.fee.rate"), "0.008")

    def test_parse_json_diff(self):
        diff = """diff --git a/appsettings.json b/appsettings.json
--- a/appsettings.json
+++ b/appsettings.json
@@ -1,4 +1,4 @@
 {
-  "max_retry": 3,
+  "max_retry": 5,
   "timeout": 30
 }
"""
        result = _parse_diff(diff)
        self.assertIn("appsettings.json", result)
        self.assertEqual(result["appsettings.json"].get("max_retry"), "5")

    def test_parse_empty_diff(self):
        result = _parse_diff("")
        self.assertEqual(result, {})

    def test_parse_no_additions(self):
        diff = """diff --git a/f.txt b/f.txt
--- a/f.txt
+++ b/f.txt
@@ -1 +1 @@
-old line
"""
        result = _parse_diff(diff)
        self.assertEqual(result, {})

    def test_parse_ini_style_diff(self):
        diff = """diff --git a/config.ini b/config.ini
--- a/config.ini
+++ b/config.ini
@@ -1,3 +1,3 @@
 [database]
-host = localhost
+host = prod-db.example.com
 port = 5432
"""
        result = _parse_diff(diff)
        self.assertIn("config.ini", result)
        self.assertEqual(result["config.ini"].get("host"), "prod-db.example.com")

    def test_parse_multiple_files(self):
        diff = """diff --git a/a.yaml b/a.yaml
--- a/a.yaml
+++ b/a.yaml
@@ -1 +1 @@
-key: old
+key: new
diff --git a/b.yaml b/b.yaml
--- a/b.yaml
+++ b/b.yaml
@@ -1 +1 @@
-other: x
+other: y
"""
        result = _parse_diff(diff)
        self.assertIn("a.yaml", result)
        self.assertIn("b.yaml", result)
        self.assertEqual(result["a.yaml"].get("key"), "new")
        self.assertEqual(result["b.yaml"].get("other"), "y")


# ══════════════════════════════════════════════════════════════════════
# Unit Tests: Rule evaluation
# ══════════════════════════════════════════════════════════════════════
class TestEvalRange(unittest.TestCase):
    def test_within_range(self):
        self.assertTrue(_eval_range("5", "[1,10]"))

    def test_at_lower_bound(self):
        self.assertTrue(_eval_range("1", "[1,10]"))

    def test_at_upper_bound(self):
        self.assertTrue(_eval_range("10", "[1,10]"))

    def test_below_range(self):
        self.assertFalse(_eval_range("0", "[1,10]"))

    def test_above_range(self):
        self.assertFalse(_eval_range("11", "[1,10]"))

    def test_invalid_format(self):
        self.assertFalse(_eval_range("5", "1,10"))

    def test_non_numeric(self):
        self.assertFalse(_eval_range("abc", "[1,10]"))


class TestEvalCompare(unittest.TestCase):
    def test_gt_pass(self):
        self.assertTrue(_eval_compare("5", "gt", "3"))

    def test_gt_fail(self):
        self.assertFalse(_eval_compare("3", "gt", "5"))

    def test_lt_pass(self):
        self.assertTrue(_eval_compare("2", "lt", "5"))

    def test_lt_fail(self):
        self.assertFalse(_eval_compare("5", "lt", "2"))

    def test_gte_boundary(self):
        self.assertTrue(_eval_compare("5", "gte", "5"))

    def test_lte_boundary(self):
        self.assertTrue(_eval_compare("3", "lte", "3"))

    def test_eq_pass(self):
        self.assertTrue(_eval_compare("42", "eq", "42"))

    def test_eq_fail(self):
        self.assertFalse(_eval_compare("42", "eq", "43"))

    def test_ne_pass(self):
        self.assertTrue(_eval_compare("42", "ne", "43"))

    def test_ne_fail(self):
        self.assertFalse(_eval_compare("42", "ne", "42"))

    def test_string_fallback_eq(self):
        self.assertTrue(_eval_compare("hello", "eq", "hello"))

    def test_string_fallback_ne(self):
        self.assertTrue(_eval_compare("hello", "ne", "world"))


class TestEvalIn(unittest.TestCase):
    def test_in_list(self):
        self.assertTrue(_eval_in("wechat", "wechat,alipay,card"))

    def test_not_in_list(self):
        self.assertFalse(_eval_in("bitcoin", "wechat,alipay"))

    def test_single_item(self):
        self.assertTrue(_eval_in("yes", "yes"))

    def test_trim_whitespace(self):
        self.assertTrue(_eval_in(" a ", "a,b"))


class TestEvalRule(unittest.TestCase):
    def test_range_rule_pass(self):
        rule = {"op": "range", "value": "[1,10]"}
        ok, msg = _eval_rule(rule, "5")
        self.assertTrue(ok)
        self.assertEqual(msg, "")

    def test_range_rule_fail(self):
        rule = {"op": "range", "value": "[1,10]", "error_hint": "must be 1-10"}
        ok, msg = _eval_rule(rule, "20")
        self.assertFalse(ok)
        self.assertEqual(msg, "must be 1-10")

    def test_gt_rule_pass(self):
        rule = {"op": "gt", "value": "0"}
        ok, _ = _eval_rule(rule, "5")
        self.assertTrue(ok)

    def test_gt_rule_fail(self):
        rule = {"op": "gt", "value": "10"}
        ok, _ = _eval_rule(rule, "5")
        self.assertFalse(ok)

    def test_in_rule_pass(self):
        rule = {"op": "in", "value": "a,b,c"}
        ok, _ = _eval_rule(rule, "b")
        self.assertTrue(ok)

    def test_unknown_op_skips(self):
        rule = {"op": "match", "value": "regex:.*"}
        ok, _ = _eval_rule(rule, "anything")
        self.assertTrue(ok)  # unknown ops are skipped


class TestIsDynamicRef(unittest.TestCase):
    def test_simple_ref(self):
        self.assertTrue(_is_dynamic_ref("${payment.fee.min_amount}"))

    def test_static_value(self):
        self.assertFalse(_is_dynamic_ref("0.006"))

    def test_range_with_ref(self):
        self.assertTrue(_is_dynamic_ref("[${min},${max}]"))

    def test_empty_string(self):
        self.assertFalse(_is_dynamic_ref(""))


# ══════════════════════════════════════════════════════════════════════
# Unit Tests: Schema validation
# ══════════════════════════════════════════════════════════════════════
class TestValidateGateRuleDef(unittest.TestCase):
    def test_valid_rule(self):
        rules = [
            {
                "field_key": "payment.fee.rate",
                "field_type": "FLOAT64",
                "op": "range",
                "value": "[0.001,0.006]",
                "scenario": "payment_fee_rate_config_COMPILE_TIME",
                "layer": 0,
                "sub_category": "threshold",
                "stable_key": "pay:fee:rate:0:threshold:range",
                "error_hint": "rate must be 0.001-0.006",
            }
        ]
        errors = validate_gate_rule_def(rules)
        self.assertEqual(errors, [])

    def test_missing_required_fields(self):
        rules = [{"field_key": "test"}]
        errors = validate_gate_rule_def(rules)
        self.assertGreater(len(errors), 0)
        self.assertTrue(any("missing required" in e for e in errors))

    def test_duplicate_stable_key(self):
        rules = [
            {
                "field_key": "a",
                "field_type": "INT32",
                "op": "gt",
                "value": "0",
                "scenario": "a_COMPILE_TIME",
                "layer": 0,
                "sub_category": "threshold",
                "stable_key": "dup",
            },
            {
                "field_key": "b",
                "field_type": "INT32",
                "op": "lt",
                "value": "100",
                "scenario": "b_COMPILE_TIME",
                "layer": 0,
                "sub_category": "threshold",
                "stable_key": "dup",
            },
        ]
        errors = validate_gate_rule_def(rules)
        self.assertTrue(any("duplicate stable_key" in e for e in errors))

    def test_invalid_op(self):
        rules = [
            {
                "field_key": "test",
                "field_type": "STRING",
                "op": "invalid_op",
                "value": "x",
                "scenario": "test_COMPILE_TIME",
                "layer": 0,
                "sub_category": "threshold",
                "stable_key": "t:1",
            }
        ]
        errors = validate_gate_rule_def(rules)
        self.assertTrue(any("invalid op" in e for e in errors))

    def test_invalid_layer_subcat_combination(self):
        rules = [
            {
                "field_key": "test",
                "field_type": "INT32",
                "op": "gt",
                "value": "0",
                "scenario": "test",
                "layer": 1,
                "sub_category": "threshold",  # layer=1 should use approval
                "stable_key": "t:2",
            }
        ]
        errors = validate_gate_rule_def(rules)
        self.assertTrue(any("POLICY" in e and "approval" in e for e in errors))

    def test_unsupported_op_for_compile_time(self):
        rules = [
            {
                "field_key": "test",
                "field_type": "STRING",
                "op": "match",
                "value": "pattern",
                "scenario": "test_COMPILE_TIME",
                "layer": 0,
                "sub_category": "format",
                "stable_key": "t:3",
            }
        ]
        errors = validate_gate_rule_def(rules)
        self.assertTrue(any("not supported at compile time" in e for e in errors))


class TestIsCompileTimeRule(unittest.TestCase):
    def test_compile_time_rule(self):
        self.assertTrue(is_compile_time_rule({"scenario": "check_COMPILE_TIME"}))

    def test_runtime_rule(self):
        self.assertFalse(is_compile_time_rule({"scenario": "check"}))

    def test_missing_scenario(self):
        self.assertFalse(is_compile_time_rule({}))


# ══════════════════════════════════════════════════════════════════════
# Integration Tests: Scan function
# ══════════════════════════════════════════════════════════════════════
class TestScan(unittest.TestCase):
    def setUp(self):
        self.tmp_dir = Path(tempfile.mkdtemp())

    def tearDown(self):
        import shutil
        shutil.rmtree(self.tmp_dir, ignore_errors=True)

    def _make_gate_rule_def(self, rules: list[dict[str, Any]]) -> Path:
        path = self.tmp_dir / "gate_rule_def.json"
        path.write_text(json.dumps(rules, indent=2), encoding="utf-8")
        return path

    def test_scan_all_pass(self):
        rules = [
            {
                "field_key": "payment.fee.rate",
                "field_type": "FLOAT64",
                "op": "range",
                "value": "[0.001,0.006]",
                "scenario": "check_COMPILE_TIME",
                "layer": 0,
                "sub_category": "threshold",
                "stable_key": "pay:fee:rate:0:thr:range",
                "error_hint": "rate must be 0.001-0.006",
            }
        ]
        gate_rule_def_path = self._make_gate_rule_def(rules)
        diff_text = """diff --git a/config.yaml b/config.yaml
+++ b/config.yaml
@@ -1 +1 @@
-payment.fee.rate: 0.003
+payment.fee.rate: 0.005
"""
        report = scan(gate_rule_def_path, diff_text, self.tmp_dir)
        self.assertEqual(report["result"], "PASS")
        self.assertEqual(report["passed"], 1)
        self.assertEqual(report["failed"], 0)

    def test_scan_violation(self):
        rules = [
            {
                "field_key": "payment.fee.rate",
                "field_type": "FLOAT64",
                "op": "range",
                "value": "[0.001,0.006]",
                "scenario": "check_COMPILE_TIME",
                "layer": 0,
                "sub_category": "threshold",
                "stable_key": "pay:fee:rate:0:thr:range",
                "error_hint": "rate must be 0.001-0.006",
            }
        ]
        gate_rule_def_path = self._make_gate_rule_def(rules)
        diff_text = """diff --git a/config.yaml b/config.yaml
+++ b/config.yaml
@@ -1 +1 @@
-payment.fee.rate: 0.003
+payment.fee.rate: 0.008
"""
        report = scan(gate_rule_def_path, diff_text, self.tmp_dir)
        self.assertEqual(report["result"], "FAIL")
        self.assertEqual(report["failed"], 1)
        self.assertEqual(len(report["violations"]), 1)
        self.assertIn("0.001-0.006", report["violations"][0]["error_hint"])

    def test_scan_dynamic_ref_warning(self):
        rules = [
            {
                "field_key": "payment.fee.rate",
                "field_type": "FLOAT64",
                "op": "gte",
                "value": "${payment.fee.min_amount}",
                "scenario": "dep_check_COMPILE_TIME",
                "layer": 0,
                "sub_category": "threshold",
                "stable_key": "pay:fee:rate:0:thr:gte",
                "error_hint": "rate must be >= min_amount",
            }
        ]
        gate_rule_def_path = self._make_gate_rule_def(rules)
        diff_text = """diff --git a/config.yaml b/config.yaml
+++ b/config.yaml
@@ -1 +1 @@
-payment.fee.rate: 0.003
+payment.fee.rate: 0.005
"""
        report = scan(gate_rule_def_path, diff_text, self.tmp_dir)
        self.assertEqual(report["result"], "PASS")
        self.assertEqual(len(report["warnings"]), 1)
        self.assertIn("Dynamic reference", report["warnings"][0]["message"])

    def test_scan_skips_runtime_rules(self):
        """Rules without _COMPILE_TIME suffix should be skipped."""
        rules = [
            {
                "field_key": "payment.fee.rate",
                "field_type": "FLOAT64",
                "op": "range",
                "value": "[0.001,0.006]",
                "scenario": "runtime_check",  # No _COMPILE_TIME suffix
                "layer": 0,
                "sub_category": "threshold",
                "stable_key": "pay:fee:rate:0:thr:range",
                "error_hint": "rate must be 0.001-0.006",
            }
        ]
        gate_rule_def_path = self._make_gate_rule_def(rules)
        report = scan(gate_rule_def_path, "", self.tmp_dir)
        self.assertEqual(report["result"], "PASS")
        self.assertEqual(report["compile_time_rules"], 0)

    def test_scan_no_compile_time_rules(self):
        rules = [
            {
                "field_key": "test",
                "field_type": "INT32",
                "op": "gt",
                "value": "0",
                "scenario": "runtime_rule",
                "layer": 0,
                "sub_category": "threshold",
                "stable_key": "t:rt",
            }
        ]
        gate_rule_def_path = self._make_gate_rule_def(rules)
        report = scan(gate_rule_def_path, "", self.tmp_dir)
        self.assertEqual(report["result"], "PASS")
        self.assertEqual(report["compile_time_rules"], 0)

    def test_scan_missing_file(self):
        path = self.tmp_dir / "nonexistent.json"
        report = scan(path, "", self.tmp_dir)
        self.assertEqual(report["result"], "FAIL")
        self.assertIn("error", report)

    def test_scan_gte_rule(self):
        rules = [
            {
                "field_key": "timeout",
                "field_type": "INT32",
                "op": "gte",
                "value": "10",
                "scenario": "timeout_check_COMPILE_TIME",
                "layer": 0,
                "sub_category": "threshold",
                "stable_key": "t:gte",
            }
        ]
        gate_rule_def_path = self._make_gate_rule_def(rules)
        diff_text = """diff --git a/c.yaml b/c.yaml
+++ b/c.yaml
@@ -1 +1 @@
-timeout: 5
+timeout: 15
"""
        report = scan(gate_rule_def_path, diff_text, self.tmp_dir)
        self.assertEqual(report["result"], "PASS")
        self.assertEqual(report["passed"], 1)

    def test_scan_enum_rule(self):
        rules = [
            {
                "field_key": "payment.method",
                "field_type": "STRING",
                "op": "in",
                "value": "wechat,alipay,card",
                "scenario": "method_check_COMPILE_TIME",
                "layer": 0,
                "sub_category": "enum_check",
                "stable_key": "pay:method:enum",
            }
        ]
        gate_rule_def_path = self._make_gate_rule_def(rules)
        diff_text = """diff --git a/c.yaml b/c.yaml
+++ b/c.yaml
@@ -1 +1 @@
-payment.method: wechat
+payment.method: bitcoin
"""
        report = scan(gate_rule_def_path, diff_text, self.tmp_dir)
        self.assertEqual(report["result"], "FAIL")
        self.assertEqual(report["failed"], 1)


# ══════════════════════════════════════════════════════════════════════
# Integration Tests: contract_compile.py gate_rule_def compilation
# ══════════════════════════════════════════════════════════════════════
class TestContractCompileGateRuleDef(unittest.TestCase):
    def setUp(self):
        self.tmp_dir = Path(tempfile.mkdtemp())

    def tearDown(self):
        import shutil
        shutil.rmtree(self.tmp_dir, ignore_errors=True)

    def test_compile_gate_rule_def_to_lock_plan(self):
        """Test that _compile_gate_rule_def produces correct lock plan entries."""
        from contract_compile import _compile_gate_rule_def

        rules = [
            {
                "field_key": "payment.fee.rate",
                "field_type": "FLOAT64",
                "op": "range",
                "value": "[0.001,0.006]",
                "scenario": "check_COMPILE_TIME",
                "layer": 0,
                "sub_category": "threshold",
                "stable_key": "pay:fee:rate:0:thr:range",
                "error_hint": "rate must be 0.001-0.006",
            }
        ]
        gate_rule_def_path = self.tmp_dir / "gate_rule_def.json"
        gate_rule_def_path.write_text(json.dumps(rules), encoding="utf-8")

        contracts, errors = _compile_gate_rule_def(gate_rule_def_path, self.tmp_dir)
        self.assertEqual(errors, [])
        self.assertEqual(len(contracts), 1)

        c = contracts[0]
        self.assertEqual(c["id"], "payment.fee.rate_COMPILE")
        self.assertEqual(c["type"], "gate_rule_def")
        self.assertEqual(c["stage"], "compile_time")
        self.assertTrue(c["blocking"])
        self.assertEqual(c["gate_refs"], ["compile_time_scanner"])
        self.assertEqual(c["rule"]["op"], "range")
        self.assertEqual(c["rule"]["value"], "[0.001,0.006]")

    def test_compile_non_compile_time_rules_skipped(self):
        """Runtime rules should not appear in lock plan."""
        from contract_compile import _compile_gate_rule_def

        rules = [
            {
                "field_key": "test",
                "field_type": "INT32",
                "op": "gt",
                "value": "0",
                "scenario": "runtime_rule",  # No _COMPILE_TIME suffix
                "layer": 0,
                "sub_category": "threshold",
                "stable_key": "t:rt",
            }
        ]
        gate_rule_def_path = self.tmp_dir / "gate_rule_def.json"
        gate_rule_def_path.write_text(json.dumps(rules), encoding="utf-8")

        contracts, errors = _compile_gate_rule_def(gate_rule_def_path, self.tmp_dir)
        self.assertEqual(contracts, [])  # No compile-time rules = no contracts

    def test_compile_invalid_schema(self):
        """Invalid gate_rule_def should produce schema errors."""
        from contract_compile import _compile_gate_rule_def

        rules = [{"field_key": "incomplete"}]  # Missing required fields
        gate_rule_def_path = self.tmp_dir / "gate_rule_def.json"
        gate_rule_def_path.write_text(json.dumps(rules), encoding="utf-8")

        contracts, errors = _compile_gate_rule_def(gate_rule_def_path, self.tmp_dir)
        self.assertEqual(contracts, [])
        self.assertGreater(len(errors), 0)


if __name__ == "__main__":
    unittest.main()
