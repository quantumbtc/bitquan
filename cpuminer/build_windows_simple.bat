@echo off
REM CPUMiner Simple Windows Build Script
REM Bitquantum RandomQ CPU Miner

echo Building CPUMiner for Windows - Simple Method
echo =============================================

REM Check if we're in the right directory
if not exist "CMakeLists.txt" (
    echo Error: CMakeLists.txt not found. Please run this script from the cpuminer directory.
    pause
    exit /b 1
)

REM Create build directory
echo Creating build directory...
if not exist "build" mkdir build
cd build

REM Configure with CMake
echo Configuring with CMake...
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release

if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed!
    echo.
    echo Please ensure you have:
    echo 1. Visual Studio 2022 installed
    echo 2. CMake installed and in PATH
    echo 3. Required dependencies (libcurl, nlohmann-json)
    echo.
    echo For dependency installation, run: install_dependencies_windows.bat
    pause
    exit /b 1
)

REM Build
echo Building...
cmake --build . --config Release --parallel

if %ERRORLEVEL% neq 0 (
    echo Build failed!
    echo.
    echo Common solutions:
    echo 1. Install missing dependencies
    echo 2. Check Visual Studio installation
    echo 3. Try running as administrator
    pause
    exit /b 1
)

echo.
echo Build completed successfully!
echo.
echo Executable location: .\build\bin\Release\cpuminer.exe
echo.
echo Quick test:
echo   .\build\bin\Release\cpuminer.exe --help
echo.
echo For full usage, see README_Windows.md
echo.
pause
