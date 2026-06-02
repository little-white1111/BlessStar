param(
    [string]$BuildDir = "build_ci_test",
    [string]$Config = "Release",
    [string]$OutputFile = "docs/day15_watch_baseline_latest.txt"
)

$ErrorActionPreference = "Stop"

$exe = Join-Path $BuildDir "$Config\bs_benchmark_attach_watch.exe"
if (-not (Test-Path $exe)) {
    throw "Benchmark executable not found: $exe"
}

Write-Host "[day15] running watch baseline: $exe"
$out = & $exe
$out | Tee-Object -FilePath $OutputFile | Out-Host

if ($LASTEXITCODE -ne 0) {
    throw "Benchmark failed with exit code $LASTEXITCODE"
}

Write-Host "[day15] baseline output saved: $OutputFile"
