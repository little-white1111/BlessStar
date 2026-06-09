# Isolate day19 shortcoming sub-stages after stress precondition.
# Usage: .\tools\test\ctest_shortcoming_isolate.ps1 [-AfterStress] [-Trace] [-Loop N]
param(
    [switch]$AfterStress,
    [switch]$Trace,
    [int]$Loop = 1,
    [string]$BuildDir = "build_ci_test",
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Set-Location $root

if ($Trace) {
    $env:BS_ATTACH_RELOAD_TRACE = "1"
    $env:BS_WAIT_TRACE = "hang"
    $env:BS_WAIT_TRACE_HANG_MS = "500"
} else {
    Remove-Item Env:BS_ATTACH_RELOAD_TRACE -ErrorAction SilentlyContinue
    Remove-Item Env:BS_WAIT_TRACE -ErrorAction SilentlyContinue
}

$stages = @(
    "manifest-fsync",
    "wal-purge",
    "ctx-store-budget",
    "pool-warmup",
    "rs-reset",
    "rs-store",
    "rs-oneshot"
)

$logDir = Join-Path $root "ci_logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$summary = Join-Path $logDir "shortcoming_stage_timing.csv"
"iteration,precondition,stage,seconds,exit" | Set-Content -Encoding utf8 $summary

for ($i = 1; $i -le $Loop; $i++) {
    if ($AfterStress) {
        Write-Host "=== iter $i precondition: stress_fail_ci ===" -ForegroundColor Cyan
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        ctest --test-dir $BuildDir -C $Config -R "^bs_test_day19_stress_fail_ci$" --output-on-failure
        if ($LASTEXITCODE -ne 0) {
            Write-Host "stress precondition failed; aborting iter $i" -ForegroundColor Red
            break
        }
        Write-Host "stress precondition OK in $($sw.Elapsed.TotalSeconds)s"
    }

    foreach ($stage in $stages) {
        $testName = "bs_test_attach_day19_shortcoming_$stage"
        Write-Host "=== iter $i stage=$stage ===" -ForegroundColor Yellow
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        ctest --test-dir $BuildDir -C $Config -R "^$testName$" --output-on-failure -V 2>&1 |
            Tee-Object -FilePath (Join-Path $logDir "shortcoming_${stage}_iter${i}.log")
        $sec = [math]::Round($sw.Elapsed.TotalSeconds, 2)
        $exit = $LASTEXITCODE
        "$i,$(if ($AfterStress) { 'stress' } else { 'none' }),$stage,$sec,$exit" |
            Add-Content -Encoding utf8 $summary
        if ($exit -ne 0) {
            Write-Host "FAILED stage=$stage after ${sec}s (see ci_logs)" -ForegroundColor Red
            exit $exit
        }
        Write-Host "OK stage=$stage ${sec}s"
    }
}

Write-Host "All stages passed. Summary: $summary" -ForegroundColor Green
