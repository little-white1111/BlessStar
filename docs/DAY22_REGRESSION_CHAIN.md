# Day 22 Regression Chain

## Scope

Day 22 uses the R-A gate-first regression chain for PR blocking checks and keeps Day 19 memory stress as a non-blocking staging signal. Crash recovery enters the chain only after `bs_test_attach_recover_cold` is implemented and passing.

## R-A Stage Chain

| Stage | Entry | Purpose |
|-------|-------|---------|
| `preflight` | `GATE-TEST-HERM`, tier/scope checks | Repository hygiene and test-environment boundaries |
| `dev_ci` | `GATE-DEV-L1` / `ctest -L day17` | L1 developer gate |
| `ci` | `GATE-INTEGRATION` then `GATE-REGRESSION` | PR blocking integration and regression |
| `ci` recover | `GATE-RECOVER` / `ctest -L recover -j 1` | Registered only after T-REC.4 passes |
| `staging` | Day 19 stress workflows | Non-blocking memory soak evidence |

Local PR entry:

```powershell
python tools/scripts/contracts/contract_gate_runner.py --through-stage ci
```

## Local Preparation

Windows local regression should clear stale attach integration locks first:

```powershell
powershell -ExecutionPolicy Bypass -File tools/test/run.ps1 -Prepare
ctest --test-dir build_ci_test -C Release -L regression -LE day17 -j 1 --output-on-failure
```

Linux equivalent:

```bash
cmake --preset ci
ctest --test-dir build_ci_test -C Release -L regression -LE day17 -j 1 --output-on-failure
```

## Staging Vs CI Boundary

`C-TST-MEM-1` records the Day 19 memory thresholds and workflow evidence as a rule-only testing contract. It intentionally has no `gate_refs` and does not create a blocking `GATE-TEST-DAY19-SMOKE`. The executable PR path remains `contract_gate_runner.py --through-stage ci`; staging keeps long-running memory smoke and full soak workflows outside the PR lock plan.

## Recover Integration

`bs_test_attach_recover_cold` is the Phase 1 recover test target. Once it passes, `GATE-RECOVER` runs:

```powershell
ctest --test-dir build_ci_test -C Release -L recover -j 1 --output-on-failure
```

The test uses `RESOURCE_LOCK attach_integration` because it exercises manifest persistence, IO read, reload gate, and ConfigManager sync on one attach context.
