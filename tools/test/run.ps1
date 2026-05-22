<#
.SYNOPSIS
  Run CTest for a BlessStar CMake build directory (Windows-friendly wrapper).

.PARAMETER BuildDir
  Path to the CMake binary directory (default: build).

.PARAMETER Config
  Build configuration for multi-config generators (e.g. Release, Debug).
  If empty, uses environment variable BLESSSTAR_CTEST_CONFIG when set.
#>
param(
  [string]$BuildDir = "build",
  [string]$Config = ""
)

$ErrorActionPreference = "Stop"
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
