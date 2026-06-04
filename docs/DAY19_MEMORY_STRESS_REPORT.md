# Day 19 Memory & Stress Report

> **Status**: Populated for **smoke** / **smoke_fail** GHA (commit `e10630c`); **gha_6h** ubuntu workflow available; **Linux 72h full** deferred.  
> **Authority**: `架构方案选择记录.md` §第19天 · **XIX-MEM-1～13** · **72h-RP**.

## Profile summary

| Profile | Duration | Day reloads (min) | Night batches (min) | Fixture | Pass criterion |
|---------|----------|-------------------|---------------------|---------|----------------|
| ci | 25s | 30 | 5 | 100KB | Outcome ≥99% (happy path) |
| smoke | 900s | 1500 | 30 | 100KB | **XIX-MEM-10**: ≥99.5% `BATCH_ALL_OK` |
| smoke_fail | 900s | 1500 | 30 | 12 good + 3 fault URIs | **XIX-MEM-13**: success rate ≤82%; taxonomy + abort mins |
| smoke_fail_ci | 25s | 30 | 5 | 4 good + 3 fault | Quick negative harness check |
| gha_6h | ~5h50m (21000s) | 34500 | 690 | 100KB | **XIX-MEM-10** smoke thresholds; GitHub ubuntu 360min cap |
| full | 72h | 8640 | 144 | 100KB | **XIX-MEM-10**: ≥99.9%; disk ≤2GB |

## Latest runs (GitHub Actions · `e10630c`)

| Run | Workflow | Profile | Platform | Outcome | Day ok rate | Night ok rate | fail_parse / gate / read | night_abort | RSS slope (MB/h) | RSS delta (MB) |
|-----|----------|---------|----------|---------|-------------|---------------|--------------------------|-------------|------------------|----------------|
| [#26881051953](https://github.com/little-white1111/BlessStar/actions/runs/26881051953) | day19-stress-smoke | smoke | windows-latest | **PASS** | 100% (1500/1500) | 100% (30/30) | 0 / 0 / 0 | 0 | -0.721 | 1.773 |
| [#26879667322](https://github.com/little-white1111/BlessStar/actions/runs/26879667322) | day19-stress-smoke-fail | smoke_fail | windows-latest | **PASS** | 80% (1200/1500) | 50% (15/30) | 100 / 100 / 100 | 15 | -2.672 | 2.910 |

**Interpretation**: **smoke** validates long-run happy path; **smoke_fail** validates deterministic fault injection (parse / ir_gate / read + PER_BATCH abort). Workflow **success** on smoke_fail means thresholds met, not 100% attach success.

### Raw log lines (GHA)

**smoke**

```text
stress,profile=smoke,day=1500/1500(1.0000),night=30/30(1.0000),fail_parse=0,fail_gate=0,fail_read=0,night_abort=0,rss_slope_reg=-0.721,rss_delta_win=1.773,rss_slope_endpoint_ws=-0.985,rss_delta_ws_win=1.773,samples=62,warmup_skip=5,win=10,disk_mb=36
Day19StressReloadLoopTest OK
```

**smoke_fail**

```text
stress,profile=smoke_fail,day=1200/1500(0.8000),night=15/30(0.5000),fail_parse=100,fail_gate=100,fail_read=100,night_abort=15,rss_slope_reg=-2.672,rss_delta_win=2.910,rss_slope_endpoint_ws=0.609,rss_delta_ws_win=2.910,samples=62,warmup_skip=5,win=10,disk_mb=18
Day19StressReloadLoopTest OK
```

### Earlier local smoke (2026-06-01 · Windows Release)

| Run | Platform | Profile | RSS slope (MB/h) | Notes |
|-----|----------|---------|-------------------|--------|
| 2026-06-01 | Windows · Release | smoke | 0.134 | Pre-GHA; slope gate uses **rss_mb** after 19.5 fix |

## Acceptance

### XIX-MEM-10 (smoke / full)

- No crash / OOM
- Outcome rate ≥ 99.5% (smoke) / ≥ 99.9% (full)
- RSS slope ≤ 0.5 MB/h (smoke/full); PR ci profile uses relaxed caps in `day19_profile.json`
- Hermetic disk ≤ 2 GB (full)

### XIX-MEM-13 (smoke_fail)

- No crash / normal exit
- Day and night **success rate ≤ 0.82** (injection effective)
- `fail_parse`, `fail_gate`, `fail_read` each ≥ 80 over 900s
- `night_abort` ≥ 15 (even-index PER_BATCH batches with extra parse-fail path)
- RSS slope ≤ 2.0 MB/h; windowed delta ≤ 100 MB (relaxed vs smoke)

## Commands

```powershell
cmake --build build_ci_test --config Release --target bs_test_day19_memory_baseline bs_test_day19_stress_reload_loop
ctest --test-dir build_ci_test -C Release -L "day19" --output-on-failure
ctest --test-dir build_ci_test -C Release -R bs_test_day19_stress_fail_ci
powershell -File tools/scripts/perf/run_day19_memory_baseline.ps1
powershell -File tools/scripts/perf/run_day19_stress_smoke.ps1 -Profile smoke
powershell -File tools/scripts/perf/run_day19_stress_smoke_fail.ps1 -Profile smoke_fail
```

```bash
BS_DAY19_PROFILE=full bash tools/scripts/perf/run_day19_stress_full_linux.sh
```

## Artifacts

| Artifact | Source |
|----------|--------|
| `docs/day19_stress_smoke_latest.json` | smoke workflow / local script |
| `docs/day19_stress_smoke_fail_latest.json` | smoke_fail workflow artifact (GHA) |
| Linux 72h CSV/JSON | `docs/day19_stress_full_latest.*` after first green **day19-stress-full** run |
