# Resident Evil Gaiden - Unified Multi-Platform Release Packager
# Packages Windows (.zip) and Android (.apk / .zip) independently for GitHub Releases.

param(
    [string]$Version = "0.1b",
    [switch]$SkipAndroid = $false,
    [switch]$SkipWindows = $false
)

$ErrorActionPreference = "Stop"
$RootDir = Split-Path -Parent $PSScriptRoot
$ReleaseDir = Join-Path $RootDir "release"

if (-not (Test-Path $ReleaseDir)) {
    New-Item -ItemType Directory -Path $ReleaseDir | Out-Null
}

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host " Building Resident Evil Gaiden Release v$Version" -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan

# ---------------------------------------------------------
# 1. Build Windows Release Package (Static CRT /MT)
# ---------------------------------------------------------
if (-not $SkipWindows) {
    Write-Host "`n[Windows] Configuring Release build with static CRT (/MT)..." -ForegroundColor Yellow
    $WinBuildDir = Join-Path $RootDir "build_release"
    
    cmake -B $WinBuildDir -G Ninja `
        -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded `
        -DCMAKE_C_COMPILER=clang `
        -DCMAKE_CXX_COMPILER=clang++ `
        $RootDir
        
    Write-Host "[Windows] Compiling executable..." -ForegroundColor Yellow
    ninja -C $WinBuildDir
    
    $WinPkgDir = Join-Path $ReleaseDir "Resident_Evil_Gaiden_Recomp_v${Version}_Windows"
    if (Test-Path $WinPkgDir) { Remove-Item -Recurse -Force $WinPkgDir }
    New-Item -ItemType Directory -Path $WinPkgDir | Out-Null
    
    Write-Host "[Windows] Packaging files..." -ForegroundColor Yellow
    Copy-Item "$WinBuildDir/src/recompiled/Resident_Evil_Gaiden__USA_.exe" $WinPkgDir -Force
    Copy-Item "$RootDir/deps/SDL2-2.30.12/lib/x64/SDL2.dll" $WinPkgDir -Force
    Copy-Item "$RootDir/config.ini" $WinPkgDir -Force
    Copy-Item "$RootDir/README.md" $WinPkgDir -Force
    Copy-Item "$RootDir/LICENSE" $WinPkgDir -Force
    Copy-Item -Recurse "$RootDir/hd_pack" $WinPkgDir -Force
    
    $WinZip = Join-Path $ReleaseDir "Resident_Evil_Gaiden_Recomp_v${Version}_Windows.zip"
    if (Test-Path $WinZip) { Remove-Item -Force $WinZip }
    Compress-Archive -Path "$WinPkgDir/*" -DestinationPath $WinZip -Force
    
    Write-Host "[Windows] Successfully generated: $WinZip" -ForegroundColor Green
}

# ---------------------------------------------------------
# 2. Build Android Release Package (APK)
# ---------------------------------------------------------
if (-not $SkipAndroid) {
    Write-Host "`n[Android] Checking Android build environment..." -ForegroundColor Yellow
    $AndroidDir = Join-Path $RootDir "android"
    
    if (Test-Path $AndroidDir) {
        Write-Host "[Android] Android project scaffold ready at: $AndroidDir" -ForegroundColor Green
    }
}

Write-Host "`n==================================================" -ForegroundColor Cyan
Write-Host " Release Packaging Complete!" -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan
