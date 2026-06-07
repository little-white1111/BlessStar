<#
.SYNOPSIS
  Stop stale local CTest / attach test processes that may hold RESOURCE_LOCK attach_integration.

.DESCRIPTION
  Use before ctest -L regression or contract_gate_runner when prior runs were interrupted.
  Does not touch unrelated processes outside python/ctest/bs_test_* name patterns.
#>
$ErrorActionPreference = "SilentlyContinue"
$targets = Get-Process python, ctest, bs_test* -ErrorAction SilentlyContinue
if ($targets) {
  $targets | Stop-Process -Force
  Start-Sleep -Seconds 1
  Write-Host "Stopped stale test processes: $($targets.Count)"
}
else {
  Write-Host "No stale python/ctest/bs_test_* processes found."
}
