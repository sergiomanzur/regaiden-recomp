# Resident Evil Gaiden Native Recompilation Build Script
param (
    [string]$BuildType = "MinSizeRel",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "  Resident Evil Gaiden (GBC) Native C Recompilation" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host ""

$BuildDir = "build"

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "[*] Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

Write-Host "[*] Configuring CMake with Clang..." -ForegroundColor Green
cmake -G Ninja -B $BuildDir -DCMAKE_BUILD_TYPE=$BuildType -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++

Write-Host "[*] Compiling native binary..." -ForegroundColor Green
ninja -C $BuildDir

if (!(Test-Path "bin")) {
    New-Item -ItemType Directory -Force "bin" | Out-Null
}

if (Test-Path "deps/SDL2-2.30.12/lib/x64/SDL2.dll") {
    Copy-Item "deps/SDL2-2.30.12/lib/x64/SDL2.dll" "$BuildDir/src/recompiled/" -Force
    Copy-Item "deps/SDL2-2.30.12/lib/x64/SDL2.dll" "bin/" -Force
}

if (Test-Path "$BuildDir/src/recompiled/Resident_Evil_Gaiden__USA_.exe") {
    Copy-Item "$BuildDir/src/recompiled/Resident_Evil_Gaiden__USA_.exe" "bin/" -Force
}

Write-Host ""
Write-Host "[✓] Build complete!" -ForegroundColor Green
Write-Host "Launch the game directly from: bin\Resident_Evil_Gaiden__USA_.exe" -ForegroundColor Yellow
