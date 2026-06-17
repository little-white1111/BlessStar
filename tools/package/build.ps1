#Requires -Version 5.1
<#
.SYNOPSIS
    BlessStar one-click packaging script (Windows PowerShell)
.DESCRIPTION
    Step 1: CMake Release build
    Step 2: Collect C++ DLLs to native/bin/
    Step 3: Run electron-builder
    Step 4: Copy installer to dist/
#>
$ErrorActionPreference = "Stop"
$ProjectRoot = Resolve-Path "$PSScriptRoot/../.."
$BuildDir    = Join-Path $ProjectRoot "build"
$NativeDir   = Join-Path $ProjectRoot "app/editor/native"
$NativeBin   = Join-Path $NativeDir "bin"
$EditorDir   = Join-Path $ProjectRoot "app/editor"
$DistDir     = Join-Path $ProjectRoot "dist"

Write-Host "=== BlessStar packaging START ===" -ForegroundColor Cyan

# Step 1: CMake Release build
Write-Host "[1/4] CMake Release build ..." -ForegroundColor Yellow
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
}
Push-Location $BuildDir
try {
    cmake .. -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
    cmake --build . --config Release
    if ($LASTEXITCODE -ne 0) { throw "CMake build failed" }
}
finally {
    Pop-Location
}
Write-Host "[1/4] Done" -ForegroundColor Green

# Step 2: Collect DLLs
Write-Host "[2/4] Collect C++ DLLs to native/bin/ ..." -ForegroundColor Yellow
if (-not (Test-Path $NativeBin)) {
    New-Item -ItemType Directory -Path $NativeBin -Force | Out-Null
}
$dlls = @("libmysql.dll", "libssl-3-x64.dll", "libcrypto-3-x64.dll")
$foundDll = $false
foreach ($dll in $dlls) {
    $src = Get-ChildItem -Path $BuildDir -Recurse -Filter $dll | Select-Object -First 1
    if ($src) {
        Copy-Item -Path $src.FullName -Destination (Join-Path $NativeBin $dll) -Force
        Write-Host "  Copied: $($src.FullName) -> $NativeBin"
        $foundDll = $true
    } else {
        Write-Warning "  Not found: $dll, skipping"
    }
}
if (-not $foundDll) {
    Write-Host "  [INFO] No DLLs found in build output. Place them manually in native/bin/" -ForegroundColor DarkYellow
}
Write-Host "[2/4] Done" -ForegroundColor Green

# Step 3: electron-builder
Write-Host "[3/4] electron-builder packaging ..." -ForegroundColor Yellow
Push-Location $EditorDir
try {
    npx electron-builder --config electron-builder.yml
    if ($LASTEXITCODE -ne 0) { throw "electron-builder failed" }
}
finally {
    Pop-Location
}
Write-Host "[3/4] Done" -ForegroundColor Green

# Step 4: Copy installer to dist/
Write-Host "[4/4] Copy installer to dist/ ..." -ForegroundColor Yellow
$ReleaseDir = Join-Path $EditorDir "release"
if (-not (Test-Path $DistDir)) {
    New-Item -ItemType Directory -Path $DistDir -Force | Out-Null
}
$installers = Get-ChildItem -Path $ReleaseDir -Recurse -Include "*.exe", "*.dmg", "*.AppImage", "*.msi", "*.zip" -ErrorAction SilentlyContinue
if ($installers) {
    foreach ($pkg in $installers) {
        Copy-Item -Path $pkg.FullName -Destination (Join-Path $DistDir $pkg.Name) -Force
        Write-Host "  Copied: $($pkg.Name)"
    }
} else {
    Write-Warning "  No installer found under $ReleaseDir"
}
Write-Host "[4/4] Done" -ForegroundColor Green
Write-Host "=== BlessStar packaging DONE ===" -ForegroundColor Cyan
Write-Host "Installer location: $DistDir"
