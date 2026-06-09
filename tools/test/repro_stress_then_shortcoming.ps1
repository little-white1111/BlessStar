<#
.SYNOPSIS
  Reproduce flaky batch: stress profiles then shortcoming (stderr to ci_logs/).
#>
param(
  [string]$BuildDir = "build_ci_test",
  [string]$Config = "Release",
  [int]$ShortcomingTimeoutSec = 120
)

$ErrorActionPreference = "Stop"
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "../..")
$bin = Join-Path $repoRoot "$BuildDir/$Config"
$logDir = Join-Path $repoRoot "ci_logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

Get-Process bs_test*,ctest -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
if (-not $env:BS_WAIT_TRACE) { $env:BS_WAIT_TRACE = "hang" }

$stressExe = Join-Path $bin "bs_test_day19_stress_reload_loop.exe"
foreach ($spec in @(
    @{ label = "bs_test_day19_stress_reload_loop"; args = @("--profile=ci") },
    @{ label = "bs_test_day19_stress_fail_ci"; args = @("--profile=smoke_fail_ci") }
)) {
  $name = $spec.label
  $args = $spec.args
  $err = Join-Path $logDir "repro_${name}.stderr"
  $out = "${err}.stdout"
  Write-Host "==> $name"
  $p = Start-Process -FilePath $stressExe -ArgumentList $args `
    -WorkingDirectory $bin -RedirectStandardError $err -RedirectStandardOutput $out -Wait -PassThru -NoNewWindow
  Write-Host "    exit=$($p.ExitCode)"
  if ($p.ExitCode -ne 0) { Get-Content $err -Tail 20; exit $p.ExitCode }
}

$short = Join-Path $bin "bs_test_attach_day19_shortcoming_regression.exe"
$trace = Join-Path $logDir "repro_shortcoming_after_stress.stderr"
Write-Host "==> shortcoming (${ShortcomingTimeoutSec}s cap)"
$proc = Start-Process -FilePath $short -WorkingDirectory $bin `
  -RedirectStandardError $trace -RedirectStandardOutput "${trace}.stdout" -PassThru -NoNewWindow
if (-not $proc.WaitForExit($ShortcomingTimeoutSec * 1000)) {
  $proc.Kill(); Write-Host "HANG"; Get-Content $trace -Tail 30; exit 124
}
Write-Host "exit=$($proc.ExitCode)"; Get-Content $trace -Tail 15
