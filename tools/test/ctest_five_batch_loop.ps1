# Loop Windows CI five-test attach_integration batch in strict serial order.
# Logs per-test timing, inter-test gaps, and stale bs_test_* processes (common hang cause).
param(
    [int]$Loop = 10,
    [string]$BuildDir = "build_ci_test",
    [string]$Config = "Release",
    [switch]$Trace,
    [switch]$KillStaleBetweenTests
)

$ErrorActionPreference = "Continue"
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "../..")
Set-Location $repoRoot

if ($Trace) {
    $env:BS_ATTACH_RELOAD_TRACE = "1"
    $env:BS_WAIT_TRACE = "hang"
    $env:BS_WAIT_TRACE_HANG_MS = "500"
    $env:BS_SHORTCOMING_VERBOSE = "1"
}

# CI cmake order: #40 -> #79 -> #80 -> #83 -> #91 (RESOURCE_LOCK attach_integration).
$FiveBatchTests = @(
    "bs_test_reload_per_batch_parallel_exec",
    "bs_test_day19_stress_reload_loop",
    "bs_test_day19_stress_fail_ci",
    "bs_test_attach_day19_shortcoming_regression",
    "bs_test_attach_recover_cold"
)

$logDir = Join-Path $repoRoot "ci_logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$boundaryCsv = Join-Path $logDir "five_batch_boundaries.csv"
"iter,after_test,test_sec,gap_sec,stale_bs_test" | Set-Content -Encoding utf8 $boundaryCsv

function Get-BsTestProcessSummary {
    $procs = Get-Process -Name "bs_test*" -ErrorAction SilentlyContinue
    if (-not $procs) { return "" }
    return ($procs | ForEach-Object { "$($_.ProcessName):$($_.Id)" }) -join ";"
}

function Invoke-StaleCleanup {
    param([string]$Reason)
    $stale = Get-BsTestProcessSummary
    if ($stale) {
        Write-Host "  [boundary] stale bs_test_* after $Reason : $stale" -ForegroundColor Yellow
        & (Join-Path $PSScriptRoot "stop_stale_ctest.ps1")
        return $stale
    }
    return ""
}

for ($i = 1; $i -le $Loop; $i++) {
    Write-Host "======== BATCH ITER $i / $Loop (strict order) ========" -ForegroundColor Magenta
    & (Join-Path $PSScriptRoot "stop_stale_ctest.ps1") | Out-Null

    $batchSw = [System.Diagnostics.Stopwatch]::StartNew()
    $prevEnd = [DateTime]::UtcNow
    $batchFailed = $false

    foreach ($testName in $FiveBatchTests) {
        $gapSec = [math]::Round(([DateTime]::UtcNow - $prevEnd).TotalSeconds, 3)
        $preStale = Invoke-StaleCleanup "pre-$testName"
        if ($KillStaleBetweenTests -and $preStale) {
            Start-Sleep -Seconds 1
        }

        Write-Host "  -> $testName (gap since prev end: ${gapSec}s)" -ForegroundColor Cyan
        $testSw = [System.Diagnostics.Stopwatch]::StartNew()
        $logFile = Join-Path $logDir "five_batch_iter${i}_${testName}.log"

        ctest --test-dir $BuildDir -C $Config -R "^$([regex]::Escape($testName))$" `
            --output-on-failure -V 2>&1 | Tee-Object -FilePath $logFile | Out-Null
        $testSec = [math]::Round($testSw.Elapsed.TotalSeconds, 2)
        $exit = $LASTEXITCODE
        $postStale = Invoke-StaleCleanup "post-$testName"

        "$i,$testName,$testSec,$gapSec,$postStale" | Add-Content -Encoding utf8 $boundaryCsv
        $prevEnd = [DateTime]::UtcNow

        if ($exit -ne 0) {
            Write-Host "  FAILED $testName after ${testSec}s (log: $logFile)" -ForegroundColor Red
            $batchFailed = $true
            break
        }
        Write-Host "  OK $testName ${testSec}s" -ForegroundColor Green
    }

    $batchSec = [math]::Round($batchSw.Elapsed.TotalSeconds, 2)
    if ($batchFailed) {
        Write-Host "BATCH FAIL iter $i after ${batchSec}s — boundaries: $boundaryCsv" -ForegroundColor Red
        exit 1
    }
    Write-Host "BATCH OK iter $i ${batchSec}s" -ForegroundColor Green
}

Write-Host "All $Loop batches passed. Boundary log: $boundaryCsv" -ForegroundColor Green
