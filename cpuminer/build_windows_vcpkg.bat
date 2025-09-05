@echo off
REM CPUMiner Windows Build Script (vcpkg)
REM Bitquantum RandomQ CPU Miner

echo Building CPUMiner for Windows using vcpkg - Bitquantum RandomQ CPU Miner
echo ========================================================================

REM Check if we're in the right directory
if not exist "CMakeLists.txt" (
    echo Error: CMakeLists.txt not found. Please run this script from the cpuminer directory.
    pause
    exit /b 1
)

REM Check if vcpkg is available
where vcpkg >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Error: vcpkg not found. Please install vcpkg and add it to PATH.
    echo Download from: https://github.com/Microsoft/vcpkg
    pause
    exit /b 1
)

REM Install dependencies
echo Installing dependencies with vcpkg...
vcpkg install curl:x64-windows nlohmann-json:x64-windows

if %ERRORLEVEL% neq 0 (
    echo vcpkg install failed!
    pause
    exit /b 1
)

REM Create build directory
echo Creating build directory...
if not exist "build_vcpkg" mkdir build_vcpkg
cd build_vcpkg

REM Configure with CMake for vcpkg
echo Configuring with CMake for vcpkg...
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_CXX_STANDARD=17 ^
    -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake

if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

REM Build
echo Building...
cmake --build . --config Release --parallel

if %ERRORLEVEL% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo Build completed successfully!
echo Executable: .\build_vcpkg\bin\Release\cpuminer.exe
echo.
echo Usage examples:
echo   .\build_vcpkg\bin\Release\cpuminer.exe --help
echo   .\build_vcpkg\bin\Release\cpuminer.exe --rpc-user bitquantum --rpc-password bitquantum123 --threads 4
echo   .\build_vcpkg\bin\Release\cpuminer.exe --config ..\config.conf
echo.
pause
