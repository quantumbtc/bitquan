@echo off
REM CPUMiner Windows Build Script (MinGW)
REM Bitquantum RandomQ CPU Miner

echo Building CPUMiner for Windows using MinGW - Bitquantum RandomQ CPU Miner
echo ========================================================================

REM Check if we're in the right directory
if not exist "CMakeLists.txt" (
    echo Error: CMakeLists.txt not found. Please run this script from the cpuminer directory.
    pause
    exit /b 1
)

REM Check if MinGW is available
where gcc >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Error: MinGW GCC not found. Please install MinGW-w64 and add it to PATH.
    echo Download from: https://www.mingw-w64.org/downloads/
    pause
    exit /b 1
)

REM Create build directory
echo Creating build directory...
if not exist "build_mingw" mkdir build_mingw
cd build_mingw

REM Configure with CMake for MinGW
echo Configuring with CMake for MinGW...
cmake .. -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_CXX_STANDARD=17 ^
    -DCMAKE_CXX_FLAGS="-O3 -march=native -mtune=native -static-libgcc -static-libstdc++"

if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

REM Build
echo Building...
cmake --build . --parallel

if %ERRORLEVEL% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo Build completed successfully!
echo Executable: .\build_mingw\cpuminer.exe
echo.
echo Usage examples:
echo   .\build_mingw\cpuminer.exe --help
echo   .\build_mingw\cpuminer.exe --rpc-user bitquantum --rpc-password bitquantum123 --threads 4
echo   .\build_mingw\cpuminer.exe --config ..\config.conf
echo.
pause
