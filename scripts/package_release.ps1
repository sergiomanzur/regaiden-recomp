# Resident Evil Gaiden - Unified Multi-Platform Release Packager
# Packages Windows (.zip) and Android (.apk / .zip) independently for GitHub Releases.

param(
    [string]$Version = "0.2.0",
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
    Copy-Item "$RootDir/CHANGELOG.md" $WinPkgDir -Force
    Copy-Item "$RootDir/LICENSE" $WinPkgDir -Force
    if (Test-Path "$RootDir/hd_pack") {
        Copy-Item -Recurse "$RootDir/hd_pack" $WinPkgDir -Force
    }
    
    $WinZip = Join-Path $ReleaseDir "Resident_Evil_Gaiden_Recomp_v${Version}_Windows.zip"
    if (Test-Path $WinZip) { Remove-Item -Force $WinZip }
    Compress-Archive -Path "$WinPkgDir/*" -DestinationPath $WinZip -Force
    
    Write-Host "[Windows] Successfully generated: $WinZip" -ForegroundColor Green
}

# ---------------------------------------------------------
# 2. Build Android Release Package (APK)
# ---------------------------------------------------------
if (-not $SkipAndroid) {
    Write-Host "`n[Android] Building Android APK (ARM64-v8a)..." -ForegroundColor Yellow
    $AndroidDir = Join-Path $RootDir "android"
    
    if (Test-Path $AndroidDir) {
        if (Test-Path "C:\Program Files\JetBrains\PyCharm 2025.2.4\jbr") {
            $env:JAVA_HOME = "C:\Program Files\JetBrains\PyCharm 2025.2.4\jbr"
        } elseif (Test-Path "C:\Program Files\Android\Android Studio\jbr") {
            $env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"
        }
        $env:PATH = "$env:JAVA_HOME\bin;$env:PATH"
        Push-Location $AndroidDir
        try {
            .\gradlew assembleDebug
        } finally {
            Pop-Location
        }
        
        $BuiltApk = Join-Path $AndroidDir "app/build/outputs/apk/debug/app-debug.apk"
        if (Test-Path $BuiltApk) {
            $OutApk = Join-Path $ReleaseDir "Resident_Evil_Gaiden_Recomp_v${Version}_Android.apk"
            Copy-Item $BuiltApk $OutApk -Force
            
            $AndroidPkgDir = Join-Path $ReleaseDir "Resident_Evil_Gaiden_Recomp_v${Version}_Android"
            if (Test-Path $AndroidPkgDir) { Remove-Item -Recurse -Force $AndroidPkgDir }
            New-Item -ItemType Directory -Path $AndroidPkgDir | Out-Null
            Copy-Item $OutApk $AndroidPkgDir -Force
            Copy-Item "$RootDir/README.md" $AndroidPkgDir -Force
            Copy-Item "$RootDir/CHANGELOG.md" $AndroidPkgDir -Force
            Copy-Item "$RootDir/LICENSE" $AndroidPkgDir -Force
            
            $AndroidZip = Join-Path $ReleaseDir "Resident_Evil_Gaiden_Recomp_v${Version}_Android.zip"
            if (Test-Path $AndroidZip) { Remove-Item -Force $AndroidZip }
            Compress-Archive -Path "$AndroidPkgDir/*" -DestinationPath $AndroidZip -Force
            
            Write-Host "[Android] Successfully generated: $OutApk" -ForegroundColor Green
            Write-Host "[Android] Successfully generated: $AndroidZip" -ForegroundColor Green
        } else {
            Write-Warning "[Android] APK not found at $BuiltApk"
        }
    }
}

Write-Host "`n==================================================" -ForegroundColor Cyan
Write-Host " Release Packaging Complete!" -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan
