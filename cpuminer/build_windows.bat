@echo off
REM CPUMiner Windows Build Script
REM Bitquantum RandomQ CPU Miner

echo Building CPUMiner for Windows - Bitquantum RandomQ CPU Miner
echo ============================================================

REM Check if we're in the right directory
if not exist "CMakeLists.txt" (
    echo Error: CMakeLists.txt not found. Please run this script from the cpuminer directory.
    pause
    exit /b 1
)

REM Create build directory
echo Creating build directory...
if not exist "build_windows" mkdir build_windows
cd build_windows

REM Configure with CMake for Windows
echo Configuring with CMake for Windows...
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_CXX_STANDARD=17 ^
    -DCMAKE_CXX_FLAGS="/O2 /Oi /Ot /Oy /GL /DNDEBUG" ^
    -DCMAKE_EXE_LINKER_FLAGS="/LTCG"

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
echo Executable: .\build_windows\bin\Release\cpuminer.exe
echo.
echo Usage examples:
echo   .\build_windows\bin\Release\cpuminer.exe --help
echo   .\build_windows\bin\Release\cpuminer.exe --rpc-user bitquantum --rpc-password bitquantum123 --threads 4
echo   .\build_windows\bin\Release\cpuminer.exe --config ..\config.conf
echo.
pause
