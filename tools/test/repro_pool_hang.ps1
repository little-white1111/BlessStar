<#
.SYNOPSIS
  Run a pool-path test with BS_WAIT_TRACE hang sampling (stderr to ci_logs/).

.EXAMPLE
  .\tools\test\repro_pool_hang.ps1
  .\tools\test\repro_pool_hang.ps1 -TestName bs_test_attach_day19_shortcoming_regression -TimeoutSec 120
#>
param(
  [string]$BuildDir = "build_ci_test",
  [string]$Config = "Release",
  [string]$TestName = "bs_test_reload_per_batch_parallel_exec",
  [int]$TimeoutSec = 60
)

$ErrorActionPreference = "Stop"
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "../..")
$bin = Join-Path $repoRoot "$BuildDir/$Config"
$exe = Join-Path $bin "$TestName.exe"
if (-not (Test-Path -LiteralPath $exe)) { throw "Missing $exe" }

$logDir = Join-Path $repoRoot "ci_logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$traceLog = Join-Path $logDir "pool_hang_${TestName}_trace.log"
$stdoutLog = "${traceLog}.stdout"

if (-not $env:BS_WAIT_TRACE) { $env:BS_WAIT_TRACE = "hang" }
Write-Host "==> $TestName timeout=${TimeoutSec}s BS_WAIT_TRACE=$($env:BS_WAIT_TRACE)"
Write-Host "    trace: $traceLog"

$proc = Start-Process -FilePath $exe -WorkingDirectory $bin `
  -RedirectStandardOutput $stdoutLog `
  -RedirectStandardError $traceLog `
  -PassThru -NoNewWindow

if (-not $proc.WaitForExit($TimeoutSec * 1000)) {
  $proc.Kill(); $proc.WaitForExit() | Out-Null
  Write-Host "HANG_OR_TIMEOUT after ${TimeoutSec}s"
  if (Test-Path $traceLog) { Get-Content $traceLog -Tail 40 }
  exit 124
}

Write-Host "exit=$($proc.ExitCode)"
if (Test-Path $traceLog) { Get-Content $traceLog -Tail 15 -ErrorAction SilentlyContinue }
exit $proc.ExitCode
