param(
    [string]$BuildDir = "build_ci_test",
    [string]$Config = "Release",
    [string]$OutputFile = "docs/day19_memory_baseline_latest.txt"
)

$ErrorActionPreference = "Stop"

$exe = Join-Path $BuildDir "$Config\bs_test_day19_memory_baseline.exe"
if (-not (Test-Path $exe)) {
    throw "Test executable not found: $exe"
}

$env:BS_DAY19_BASELINE_OUT = $OutputFile
Write-Host "[day19] running memory baseline: $exe"
& $exe 2>&1 | Tee-Object -FilePath $OutputFile
if ($LASTEXITCODE -ne 0) {
    throw "Baseline test failed with exit code $LASTEXITCODE"
}
Write-Host "[day19] baseline saved: $OutputFile"
