<#
.SYNOPSIS
  Run clang-format on BlessStar C/C++ sources only (never .py / .sh / .md).

.DESCRIPTION
  Avoids PowerShell pitfalls where Get-ChildItem -Recurse -Include can match too broadly.
  Use the same extension list as CI clang-format step.
#>
param(
  [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path,
  [string]$ClangFormat = "clang-format"
)

$ErrorActionPreference = "Stop"
$ext = @(".cpp", ".h", ".hpp", ".cc", ".c")
$dirs = @("kernel", "adapter", "tests", "cmake")
$files = @()
foreach ($d in $dirs) {
  $p = Join-Path $RepoRoot $d
  if (-not (Test-Path $p)) { continue }
  $files += Get-ChildItem -LiteralPath $p -Recurse -File | Where-Object {
    $ext -contains $_.Extension.ToLower() -and $_.FullName -notmatch '[\\/]Source[\\/]'
  }
}
if ($files.Count -eq 0) {
  Write-Host "No C/C++ sources found under kernel/adapter/tests/cmake."
  exit 0
}
foreach ($f in $files) {
  & $ClangFormat -i --style=file $f.FullName
  if ($LASTEXITCODE -ne 0) { throw "clang-format failed: $($f.FullName)" }
}
Write-Host ("Formatted {0} files." -f $files.Count)
