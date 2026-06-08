# DAY17: Convergence Report

## Summary

Day17 implements **contract-driven style convergence** (Scheme B + `C-ST-*` freeze points):

- T0: `style.contracts.json`, registry/template updates, verification matrix, inconsistency checklist
- T1: Python verify gates registered as `ctest -L day17`
- T2: `C-ST-7` contract blocks inserted in 61 public headers; ctest label baseline tightened
- T3: Regression evidence for **C-ST-13**

## Test results (2026-05-28, `build_ci_test` Release)

| Suite | Result |
|-------|--------|
| `-L day17` | **10/10** |
| `-L day14\|day15\|day16` | **6/6** |
| `-L integration` | **7/7** |
| `-L regression` | **77/77** |

## Notes

- **C-ST-5** (`clang-format`): checker **SKIPs** when `clang-format` is absent from `PATH` (local Windows dev). CI/Linux should install `clang-format` and run `run_clang_format_check.py --batch include|src --apply` before merge.
- **C-ST-1**: MVP scope is `adapter/persistence`, `adapter/io`, and `app/sdk` public headers (kernel legacy `ir_*` / `plugin_*` deferred).
- **C-ST-6 / C-ST-8**: remain `draft` in `style.contracts.json` (warning-first / manual).
