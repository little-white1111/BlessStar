param(
    [string]$BuildDir = "build_ci_test",
    [string]$Config = "Release",
    [string]$Profile = "smoke_fail",
    [string]$OutputJson = "docs/day19_stress_smoke_fail_latest.json"
)

$ErrorActionPreference = "Stop"

$exe = Join-Path $BuildDir "$Config\bs_test_day19_stress_reload_loop.exe"
if (-not (Test-Path $exe)) {
    throw "Stress test executable not found: $exe"
}

$env:BS_DAY19_PROFILE = $Profile
$env:BS_DAY19_STRESS_OUT = $OutputJson
Write-Host "[day19] failure-stress profile=$Profile exe=$exe"
& $exe --profile=$Profile
if ($LASTEXITCODE -ne 0) {
    throw "Failure-stress run failed with exit code $LASTEXITCODE"
}
Write-Host "[day19] failure-stress output: $OutputJson"
