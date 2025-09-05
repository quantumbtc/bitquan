@echo off
REM CPUMiner Windows Package Creation Script
REM Bitquantum RandomQ CPU Miner

echo Creating Windows Package for CPUMiner
echo =====================================

REM Check if we're in the right directory
if not exist "CMakeLists.txt" (
    echo Error: CMakeLists.txt not found. Please run this script from the cpuminer directory.
    pause
    exit /b 1
)

REM Create package directory
set PACKAGE_DIR=cpuminer_windows_package
echo Creating package directory: %PACKAGE_DIR%
if exist "%PACKAGE_DIR%" rmdir /s /q "%PACKAGE_DIR%"
mkdir "%PACKAGE_DIR%"

REM Find the executable
set EXE_PATH=
if exist "build_windows\bin\Release\cpuminer.exe" (
    set EXE_PATH=build_windows\bin\Release\cpuminer.exe
) else if exist "build_vcpkg\bin\Release\cpuminer.exe" (
    set EXE_PATH=build_vcpkg\bin\Release\cpuminer.exe
) else if exist "build_mingw\cpuminer.exe" (
    set EXE_PATH=build_mingw\cpuminer.exe
) else (
    echo Error: cpuminer.exe not found. Please build the project first.
    pause
    exit /b 1
)

echo Found executable: %EXE_PATH%

REM Copy executable
echo Copying executable...
copy "%EXE_PATH%" "%PACKAGE_DIR%\cpuminer.exe"

REM Copy configuration file
echo Copying configuration file...
copy "config.conf" "%PACKAGE_DIR%\"

REM Copy documentation
echo Copying documentation...
copy "README.md" "%PACKAGE_DIR%\"
copy "README_Windows.md" "%PACKAGE_DIR%\"

REM Copy required DLLs if they exist
echo Copying required DLLs...
if exist "build_windows\bin\Release\*.dll" (
    copy "build_windows\bin\Release\*.dll" "%PACKAGE_DIR%\"
)
if exist "build_vcpkg\bin\Release\*.dll" (
    copy "build_vcpkg\bin\Release\*.dll" "%PACKAGE_DIR%\"
)

REM Create batch files for easy running
echo Creating run scripts...

REM Create run_cpuminer.bat
echo @echo off > "%PACKAGE_DIR%\run_cpuminer.bat"
echo echo Starting CPUMiner - Bitquantum RandomQ CPU Miner >> "%PACKAGE_DIR%\run_cpuminer.bat"
echo echo ================================================ >> "%PACKAGE_DIR%\run_cpuminer.bat"
echo echo. >> "%PACKAGE_DIR%\run_cpuminer.bat"
echo echo Usage examples: >> "%PACKAGE_DIR%\run_cpuminer.bat"
echo echo   cpuminer.exe --help >> "%PACKAGE_DIR%\run_cpuminer.bat"
echo echo   cpuminer.exe --rpc-user bitquantum --rpc-password bitquantum123 --threads 4 >> "%PACKAGE_DIR%\run_cpuminer.bat"
echo echo   cpuminer.exe --config config.conf >> "%PACKAGE_DIR%\run_cpuminer.bat"
echo echo. >> "%PACKAGE_DIR%\run_cpuminer.bat"
echo pause >> "%PACKAGE_DIR%\run_cpuminer.bat"

REM Create run_with_config.bat
echo @echo off > "%PACKAGE_DIR%\run_with_config.bat"
echo echo Starting CPUMiner with configuration file... >> "%PACKAGE_DIR%\run_with_config.bat"
echo cpuminer.exe --config config.conf >> "%PACKAGE_DIR%\run_with_config.bat"
echo pause >> "%PACKAGE_DIR%\run_with_config.bat"

REM Create install_dependencies.bat
echo @echo off > "%PACKAGE_DIR%\install_dependencies.bat"
echo echo Installing CPUMiner Dependencies for Windows >> "%PACKAGE_DIR%\install_dependencies.bat"
echo echo ============================================= >> "%PACKAGE_DIR%\install_dependencies.bat"
echo echo. >> "%PACKAGE_DIR%\install_dependencies.bat"
echo echo This will install the required dependencies for CPUMiner. >> "%PACKAGE_DIR%\install_dependencies.bat"
echo echo. >> "%PACKAGE_DIR%\install_dependencies.bat"
echo pause >> "%PACKAGE_DIR%\install_dependencies.bat"
echo call install_dependencies_windows.bat >> "%PACKAGE_DIR%\install_dependencies.bat"

REM Copy the dependency installer
if exist "install_dependencies_windows.bat" (
    copy "install_dependencies_windows.bat" "%PACKAGE_DIR%\"
)

REM Create package info file
echo Creating package info...
echo CPUMiner Windows Package > "%PACKAGE_DIR%\PACKAGE_INFO.txt"
echo ======================== >> "%PACKAGE_DIR%\PACKAGE_INFO.txt"
echo. >> "%PACKAGE_DIR%\PACKAGE_INFO.txt"
echo Version: 1.0.0 >> "%PACKAGE_DIR%\PACKAGE_INFO.txt"
echo Build Date: %DATE% %TIME% >> "%PACKAGE_DIR%\PACKAGE_INFO.txt"
echo Platform: Windows x64 >> "%PACKAGE_DIR%\PACKAGE_INFO.txt"
echo. >> "%PACKAGE_DIR%\PACKAGE_INFO.txt"
echo Contents: >> "%PACKAGE_DIR%\PACKAGE_INFO.txt"
echo - cpuminer.exe (Main executable) >> "%PACKAGE_DIR%\PACKAGE_INFO.txt"
echo - config.conf (Configuration file) >> "%PACKAGE_DIR%\PACKAGE_INFO.txt"
echo - README.md (Documentation) >> "%PACKAGE_DIR%\PACKAGE_INFO.txt"
echo - README_Windows.md (Windows-specific guide) >> "%PACKAGE_DIR%\PACKAGE_INFO.txt"
echo - run_cpuminer.bat (Run script with examples) >> "%PACKAGE_DIR%\PACKAGE_INFO.txt"
echo - run_with_config.bat (Run with config file) >> "%PACKAGE_DIR%\PACKAGE_INFO.txt"
echo - install_dependencies.bat (Dependency installer) >> "%PACKAGE_DIR%\PACKAGE_INFO.txt"
echo. >> "%PACKAGE_DIR%\PACKAGE_INFO.txt"
echo Quick Start: >> "%PACKAGE_DIR%\PACKAGE_INFO.txt"
echo 1. Run install_dependencies.bat to install required dependencies >> "%PACKAGE_DIR%\PACKAGE_INFO.txt"
echo 2. Edit config.conf with your RPC settings >> "%PACKAGE_DIR%\PACKAGE_INFO.txt"
echo 3. Run run_with_config.bat to start mining >> "%PACKAGE_DIR%\PACKAGE_INFO.txt"
echo. >> "%PACKAGE_DIR%\PACKAGE_INFO.txt"
echo For more information, see README_Windows.md >> "%PACKAGE_DIR%\PACKAGE_INFO.txt"

REM Create ZIP package
echo Creating ZIP package...
if exist "cpuminer_windows.zip" del "cpuminer_windows.zip"
powershell Compress-Archive -Path "%PACKAGE_DIR%\*" -DestinationPath "cpuminer_windows.zip"

echo.
echo Package creation completed successfully!
echo.
echo Package directory: %PACKAGE_DIR%\
echo ZIP file: cpuminer_windows.zip
echo.
echo Package contents:
dir /b "%PACKAGE_DIR%"
echo.
echo To use the package:
echo 1. Extract cpuminer_windows.zip
echo 2. Run install_dependencies.bat
echo 3. Edit config.conf with your settings
echo 4. Run run_with_config.bat
echo.
pause
