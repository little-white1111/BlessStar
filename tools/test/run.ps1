<#
.SYNOPSIS
  Run CTest for a BlessStar CMake build directory (Windows-friendly wrapper).

.PARAMETER BuildDir
  Path to the CMake binary directory (default: build).

.PARAMETER Config
  Build configuration for multi-config generators (e.g. Release, Debug).
  If empty, uses environment variable BLESSSTAR_CTEST_CONFIG when set.

.PARAMETER Prepare
  Stop stale python/ctest/bs_test_* processes before ctest (avoids attach_integration lock wait).
#>
param(
  [string]$BuildDir = "build",
  [string]$Config = "",
  [switch]$Prepare
)

$ErrorActionPreference = "Stop"
if ($Prepare) {
  & "$PSScriptRoot/stop_stale_ctest.ps1"
}
$resolved = Resolve-Path -LiteralPath $BuildDir -ErrorAction SilentlyContinue
if (-not $resolved) {
  throw "Build directory not found: $BuildDir"
}

$ctestArgs = @("--test-dir", $resolved.Path, "--output-on-failure")
if ($Config) {
  $ctestArgs += @("-C", $Config)
}
elseif ($env:BLESSSTAR_CTEST_CONFIG) {
  $ctestArgs += @("-C", $env:BLESSSTAR_CTEST_CONFIG)
}

& ctest @ctestArgs
exit $LASTEXITCODE
