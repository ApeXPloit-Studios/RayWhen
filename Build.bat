@echo off
echo === Creating Directories ===
if not exist "maps" mkdir maps
if not exist "src" mkdir src
if not exist "dist" mkdir dist
echo Directories ready!
echo.

echo === Checking TCC Compiler ===
tcc -version >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: TCC compiler not found! Please install Tiny C Compiler.
    pause
    exit /b 1
)
echo TCC compiler found!
echo.

echo === Compiling Advanced Raycasting Engine with TCC ===
if not exist "src\raywin.c" (
    echo Error: src\raywin.c not found!
    pause
    exit /b 1
)
tcc src\raywin.c -o dist\raywin.exe -luser32 -lgdi32

if %errorlevel% neq 0 (
    echo Compile failed!
    pause
    exit /b %errorlevel%
)

echo === Compilation successful! ===
echo === Features: ===
echo - Window resizing support
echo - DDA raycasting algorithm
echo - Texture mapping with procedural textures
echo - Collision detection
echo - Minimap display
echo - Smooth movement controls (WASD/Arrows)
echo - Multiple wall types with different colors
echo - Distance-based shading
echo.
echo === Compiling Launcher ===
if not exist "src\launcher.c" (
    echo Error: src\launcher.c not found!
    pause
    exit /b 1
)
tcc src\launcher.c -o dist\launcher.exe -luser32 -lgdi32

echo === Compiling Map Editor ===
if not exist "src\mapedit.c" (
    echo Error: src\mapedit.c not found!
    pauseqqqqqqqqqqqqq
    exit /b 1
)
tcc src\mapedit.c -o dist\mapedit.exe -luser32 -lgdi32 -lcomdlg32


if %errorlevel% neq 0 (
    echo Launcher compile failed!
    pause
    exit /b %errorlevel%
)

echo === Launching launcher.exe ===
dist\launcher.exe
