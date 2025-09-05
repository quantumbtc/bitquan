@echo off
REM CPUMiner Windows Dependencies Installation Script
REM Bitquantum RandomQ CPU Miner

echo Installing CPUMiner Dependencies for Windows
echo ============================================

echo.
echo This script will help you install the required dependencies for building CPUMiner on Windows.
echo.

REM Check if running as administrator
net session >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Warning: Not running as administrator. Some installations may require elevated privileges.
    echo.
)

echo Available installation methods:
echo 1. vcpkg (Recommended - Microsoft's C++ package manager)
echo 2. vcpkg (Manual installation)
echo 3. Manual installation instructions
echo 4. Exit
echo.

set /p choice="Please select an option (1-4): "

if "%choice%"=="1" goto vcpkg_auto
if "%choice%"=="2" goto vcpkg_manual
if "%choice%"=="3" goto manual_instructions
if "%choice%"=="4" goto end
goto invalid_choice

:vcpkg_auto
echo.
echo Installing vcpkg automatically...
echo.

REM Check if git is available
where git >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Error: Git not found. Please install Git first.
    echo Download from: https://git-scm.com/download/win
    pause
    exit /b 1
)

REM Clone vcpkg
echo Cloning vcpkg repository...
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg

REM Bootstrap vcpkg
echo Bootstrapping vcpkg...
.\bootstrap-vcpkg.bat

REM Add vcpkg to PATH (temporary)
set VCPKG_ROOT=%CD%
set PATH=%PATH%;%VCPKG_ROOT%

REM Install dependencies
echo Installing dependencies...
vcpkg install curl:x64-windows nlohmann-json:x64-windows

if %ERRORLEVEL% neq 0 (
    echo vcpkg installation failed!
    pause
    exit /b 1
)

echo.
echo vcpkg installation completed successfully!
echo vcpkg root: %VCPKG_ROOT%
echo.
echo You can now run: build_windows_vcpkg.bat
goto end

:vcpkg_manual
echo.
echo Manual vcpkg installation:
echo.
echo 1. Download and install Git from: https://git-scm.com/download/win
echo 2. Clone vcpkg repository:
echo    git clone https://github.com/Microsoft/vcpkg.git
echo 3. Navigate to vcpkg directory:
echo    cd vcpkg
echo 4. Bootstrap vcpkg:
echo    .\bootstrap-vcpkg.bat
echo 5. Install dependencies:
echo    vcpkg install curl:x64-windows nlohmann-json:x64-windows
echo 6. Set VCPKG_ROOT environment variable to vcpkg directory
echo 7. Run: build_windows_vcpkg.bat
echo.
goto end

:manual_instructions
echo.
echo Manual Installation Instructions:
echo.
echo 1. Install Visual Studio 2022 Community (free):
echo    https://visualstudio.microsoft.com/vs/community/
echo    - Select "Desktop development with C++" workload
echo    - Include Windows 10/11 SDK
echo.
echo 2. Install CMake:
echo    https://cmake.org/download/
echo    - Add CMake to PATH during installation
echo.
echo 3. Install libcurl:
echo    Option A - Using vcpkg (recommended):
echo    - Follow vcpkg installation steps above
echo.
echo    Option B - Manual installation:
echo    - Download curl from: https://curl.se/download.html
echo    - Extract to C:\curl
echo    - Add C:\curl\bin to PATH
echo.
echo 4. Install nlohmann-json:
echo    Option A - Using vcpkg (recommended):
echo    - Follow vcpkg installation steps above
echo.
echo    Option B - Header-only library:
echo    - Download from: https://github.com/nlohmann/json/releases
echo    - Extract to C:\json
echo    - Add C:\json\include to include path
echo.
echo 5. Build CPUMiner:
echo    - Run build_windows.bat
echo.
goto end

:invalid_choice
echo Invalid choice. Please select 1-4.
goto end

:end
echo.
echo Installation guide completed.
pause
