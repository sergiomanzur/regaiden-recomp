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
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed ($LASTEXITCODE)" }

Write-Host "[*] Compiling native binary..." -ForegroundColor Green
ninja -C $BuildDir
if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)" }

if (!(Test-Path "bin")) {
    New-Item -ItemType Directory -Force "bin" | Out-Null
}

Copy-Item "$BuildDir/src/recompiled/SDL2.dll" "bin/" -Force

if (Test-Path "$BuildDir/src/recompiled/Resident_Evil_Gaiden__USA_.exe") {
    Copy-Item "$BuildDir/src/recompiled/Resident_Evil_Gaiden__USA_.exe" "bin/" -Force
}

Write-Host ""
Write-Host "[✓] Build complete!" -ForegroundColor Green
Write-Host "Launch the game directly from: bin\Resident_Evil_Gaiden__USA_.exe" -ForegroundColor Yellow
