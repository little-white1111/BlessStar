# Day 19 Memory & Stress Report

> **Status**: Template — populate after `run_day19_stress_smoke.ps1` / Linux full run.  
> **Authority**: `架构方案选择记录.md` §第19天 · **XIX-MEM-1～12** · **72h-RP**.

## Profile summary

| Profile | Duration | Day reloads (min) | Night batches (min) | Fixture |
|---------|----------|-------------------|---------------------|---------|
| ci | 25s | 30 | 5 | 100KB |
| smoke | 900s | 1500 | 30 | 100KB |
| full | 72h | 8640 | 144 | 100KB |

## Latest runs

| Run | Platform | Profile | Outcome | RSS slope (MB/h) | RSS delta (MB) | Artifact |
|-----|----------|---------|---------|------------------|----------------|----------|
| 2026-06-01 | Windows · Release | smoke | **功能通过**（1500/1500 日，30/30 夜，100% `BATCH_ALL_OK`）；RSS 斜率（regression rss）0.134 <= 0.5 | 0.134 | 1.883 | `day19_stress_smoke_latest.json` 已写出 |

**最近一次 smoke 原始摘要**

```text
Day19Stress profile=smoke duration_max=900 day_reload_min=1500 night_batch_min=30
stress,profile=smoke,day=1500/1500(1.0000),night=30/30(1.0000),rss_slope_reg=0.134,rss_delta_win=1.883,rss_slope_endpoint_ws=2.391,rss_delta_ws_win=1.883,samples=62,warmup_skip=5,win=10,disk_mb=36
Day19StressReloadLoopTest OK
```

## Acceptance (XIX-MEM-10)

- No crash / OOM
- Outcome rate ≥ 99.5% (smoke) / ≥ 99.9% (full)
- RSS slope ≤ 0.5 MB/h (smoke/full); PR ci profile uses relaxed caps in `day19_profile.json`
- Hermetic disk ≤ 2 GB (full)

## Commands

```powershell
cmake --build build_ci_test --config Release --target bs_test_day19_memory_baseline bs_test_day19_stress_reload_loop
ctest --test-dir build_ci_test -C Release -L "day19" --output-on-failure
powershell -File tools/scripts/perf/run_day19_memory_baseline.ps1
powershell -File tools/scripts/perf/run_day19_stress_smoke.ps1 -Profile smoke
```

```bash
BS_DAY19_PROFILE=full bash tools/scripts/perf/run_day19_stress_full_linux.sh
```
