@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo   Resident Evil Gaiden (GBC) Native C Recompilation
echo ========================================================
echo.

set "BUILD_DIR=build"

echo [*] Configuring with CMake (Ninja + Clang)...
cmake -G Ninja -B %BUILD_DIR% -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
if %ERRORLEVEL% neq 0 (
    echo [!] CMake configuration failed.
    exit /b %ERRORLEVEL%
)

echo [*] Building native executable...
ninja -C %BUILD_DIR%
if %ERRORLEVEL% neq 0 (
    echo [!] Build failed.
    exit /b %ERRORLEVEL%
)

echo [*] Copying runtime dependencies...
if not exist "bin" mkdir "bin"
if exist "deps\SDL2-2.30.12\lib\x64\SDL2.dll" (
    copy /Y "deps\SDL2-2.30.12\lib\x64\SDL2.dll" "%BUILD_DIR%\src\recompiled\" >nul
    copy /Y "deps\SDL2-2.30.12\lib\x64\SDL2.dll" "bin\" >nul
)
if exist "%BUILD_DIR%\src\recompiled\Resident_Evil_Gaiden__USA_.exe" (
    copy /Y "%BUILD_DIR%\src\recompiled\Resident_Evil_Gaiden__USA_.exe" "bin\" >nul
)

echo.
echo [✓] Build completed successfully!
echo Launch the game directly from: bin\Resident_Evil_Gaiden__USA_.exe
echo.
