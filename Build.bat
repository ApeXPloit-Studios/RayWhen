@echo off
echo === Creating Directories ===
if not exist "maps" mkdir maps
if not exist "src" mkdir src
if not exist "dist" mkdir dist
if not exist "dist\maps" mkdir dist\maps
echo Directories ready!

echo === Copying Maps to Dist ===
if exist "maps\*.rwm" copy "maps\*.rwm" "dist\maps\"
if exist "maps\*.txt" copy "maps\*.txt" "dist\maps\"
echo Maps copied!

echo === Copying Assets to Dist ===
if exist "assets" xcopy "assets" "dist\assets\" /E /I /Y
echo Assets copied!
echo.

echo === Compiler Selection ===
echo Please choose your compiler:
echo 1. Clang - Fast compilation with good optimization
echo 2. GCC - GNU Compiler Collection with mature optimization
echo.
set /p choice="Enter your choice (1-2): "

if "%choice%"=="1" goto use_clang
if "%choice%"=="2" goto use_gcc
echo Invalid choice! Please run the script again and select 1 or 2.
pause
exit /b 1


:use_clang
echo === Checking Clang Compiler ===
clang --version >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: Clang compiler not found! Please install LLVM Clang.
    echo Download from: https://releases.llvm.org/download.html
    pause
    exit /b 1
)
echo Clang compiler found!
echo === Compiling Advanced Raycasting Engine with Clang ===
if not exist "src\raywin.c" (
    echo Error: src\raywin.c not found!
    pause
    exit /b 1
)
clang src\raywin.c src\texture.c src\map.c src\player.c src\enemy.c src\renderer.c -o dist\raywin.exe -luser32 -lgdi32 -O2 -Wall
goto compile_launcher

:use_gcc
echo === Checking GCC Compiler ===
gcc --version >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: GCC compiler not found! Please install MinGW-w64 or MSYS2.
    echo Download from: https://www.msys2.org/ or https://mingw-w64.org/
    pause
    exit /b 1
)
echo GCC compiler found!
echo === Compiling Advanced Raycasting Engine with GCC ===
if not exist "src\raywin.c" (
    echo Error: src\raywin.c not found!
    pause
    exit /b 1
)
gcc src\raywin.c src\texture.c src\map.c src\player.c src\enemy.c src\renderer.c -o dist\raywin.exe -luser32 -lgdi32 -O2 -Wall
goto compile_launcher

:compile_launcher

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

if "%choice%"=="1" (
    clang src\launcher.c -o dist\launcher.exe -luser32 -lgdi32 -O2 -Wall
) else if "%choice%"=="2" (
    gcc src\launcher.c -o dist\launcher.exe -luser32 -lgdi32 -O2 -Wall
)

echo === Compiling Map Editor ===
if not exist "src\mapedit.c" (
    echo Error: src\mapedit.c not found!
    pause
    exit /b 1
)

if "%choice%"=="1" (
    clang src\mapedit.c -o dist\mapedit.exe -luser32 -lgdi32 -lcomdlg32 -O2 -Wall
) else if "%choice%"=="2" (
    gcc src\mapedit.c -o dist\mapedit.exe -luser32 -lgdi32 -lcomdlg32 -O2 -Wall
)


if %errorlevel% neq 0 (
    echo Launcher compile failed!
    pause
    exit /b %errorlevel%
)

echo === Launching launcher.exe ===
dist\launcher.exe
