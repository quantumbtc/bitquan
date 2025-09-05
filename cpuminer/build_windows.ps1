# CPUMiner Windows PowerShell Build Script
# Bitquantum RandomQ CPU Miner

param(
    [string]$BuildType = "Release",
    [string]$Generator = "Visual Studio 17 2022",
    [string]$Architecture = "x64",
    [switch]$Clean,
    [switch]$Package,
    [switch]$Help
)

if ($Help) {
    Write-Host "CPUMiner Windows Build Script" -ForegroundColor Green
    Write-Host "=============================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Usage: .\build_windows.ps1 [options]" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Options:" -ForegroundColor Yellow
    Write-Host "  -BuildType <type>     Build type (Release, Debug) [default: Release]" -ForegroundColor White
    Write-Host "  -Generator <gen>      CMake generator [default: Visual Studio 17 2022]" -ForegroundColor White
    Write-Host "  -Architecture <arch>  Target architecture (x64, x86) [default: x64]" -ForegroundColor White
    Write-Host "  -Clean                Clean build directory before building" -ForegroundColor White
    Write-Host "  -Package              Create distribution package after building" -ForegroundColor White
    Write-Host "  -Help                 Show this help message" -ForegroundColor White
    Write-Host ""
    Write-Host "Examples:" -ForegroundColor Yellow
    Write-Host "  .\build_windows.ps1" -ForegroundColor White
    Write-Host "  .\build_windows.ps1 -BuildType Debug" -ForegroundColor White
    Write-Host "  .\build_windows.ps1 -Clean -Package" -ForegroundColor White
    exit 0
}

Write-Host "CPUMiner Windows Build Script" -ForegroundColor Green
Write-Host "=============================" -ForegroundColor Green
Write-Host "Build Type: $BuildType" -ForegroundColor Cyan
Write-Host "Generator: $Generator" -ForegroundColor Cyan
Write-Host "Architecture: $Architecture" -ForegroundColor Cyan
Write-Host ""

# Check if we're in the right directory
if (-not (Test-Path "CMakeLists.txt")) {
    Write-Host "Error: CMakeLists.txt not found. Please run this script from the cpuminer directory." -ForegroundColor Red
    exit 1
}

# Clean build directory if requested
if ($Clean) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    if (Test-Path "build") {
        Remove-Item -Recurse -Force "build"
    }
}

# Create build directory
Write-Host "Creating build directory..." -ForegroundColor Yellow
if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}
Set-Location "build"

# Configure with CMake
Write-Host "Configuring with CMake..." -ForegroundColor Yellow
$cmakeArgs = @(
    "..",
    "-G", $Generator,
    "-A", $Architecture,
    "-DCMAKE_BUILD_TYPE=$BuildType"
)

# Add vcpkg toolchain if available
if ($env:VCPKG_ROOT) {
    $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
    Write-Host "Using vcpkg toolchain: $env:VCPKG_ROOT" -ForegroundColor Cyan
}

& cmake @cmakeArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please ensure you have:" -ForegroundColor Yellow
    Write-Host "1. Visual Studio 2022 installed" -ForegroundColor White
    Write-Host "2. CMake installed and in PATH" -ForegroundColor White
    Write-Host "3. Required dependencies (libcurl, nlohmann-json)" -ForegroundColor White
    Write-Host ""
    Write-Host "For dependency installation, run: install_dependencies_windows.bat" -ForegroundColor Yellow
    exit 1
}

# Build
Write-Host "Building..." -ForegroundColor Yellow
& cmake --build . --config $BuildType --parallel

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Common solutions:" -ForegroundColor Yellow
    Write-Host "1. Install missing dependencies" -ForegroundColor White
    Write-Host "2. Check Visual Studio installation" -ForegroundColor White
    Write-Host "3. Try running as administrator" -ForegroundColor White
    Write-Host "4. Check for antivirus interference" -ForegroundColor White
    exit 1
}

# Check if executable was created
$exePath = "bin\$BuildType\cpuminer.exe"
if (-not (Test-Path $exePath)) {
    Write-Host "Error: Executable not found at expected location: $exePath" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Build completed successfully!" -ForegroundColor Green
Write-Host "Executable: .\build\$exePath" -ForegroundColor Cyan
Write-Host ""

# Test executable
Write-Host "Testing executable..." -ForegroundColor Yellow
& ".\$exePath" --help | Out-Null
if ($LASTEXITCODE -eq 0) {
    Write-Host "Executable test passed!" -ForegroundColor Green
} else {
    Write-Host "Warning: Executable test failed" -ForegroundColor Yellow
}

# Create package if requested
if ($Package) {
    Write-Host ""
    Write-Host "Creating distribution package..." -ForegroundColor Yellow
    
    $packageDir = "..\cpuminer_windows_package"
    if (Test-Path $packageDir) {
        Remove-Item -Recurse -Force $packageDir
    }
    New-Item -ItemType Directory -Path $packageDir | Out-Null
    
    # Copy executable
    Copy-Item $exePath "$packageDir\cpuminer.exe"
    
    # Copy configuration and documentation
    Copy-Item "..\config.conf" $packageDir
    Copy-Item "..\README.md" $packageDir
    Copy-Item "..\README_Windows.md" $packageDir
    
    # Copy required DLLs
    $dllFiles = Get-ChildItem -Path "bin\$BuildType" -Filter "*.dll"
    foreach ($dll in $dllFiles) {
        Copy-Item $dll.FullName $packageDir
    }
    
    # Create run script
    $runScript = @"
@echo off
echo Starting CPUMiner - Bitquantum RandomQ CPU Miner
echo ================================================
echo.
echo Usage examples:
echo   cpuminer.exe --help
echo   cpuminer.exe --rpc-user bitquantum --rpc-password bitquantum123 --threads 4
echo   cpuminer.exe --config config.conf
echo.
pause
"@
    $runScript | Out-File -FilePath "$packageDir\run_cpuminer.bat" -Encoding ASCII
    
    # Create ZIP package
    $zipPath = "..\cpuminer_windows.zip"
    if (Test-Path $zipPath) {
        Remove-Item $zipPath
    }
    Compress-Archive -Path "$packageDir\*" -DestinationPath $zipPath
    
    Write-Host "Package created: $zipPath" -ForegroundColor Green
    Write-Host "Package directory: $packageDir" -ForegroundColor Cyan
}

Write-Host ""
Write-Host "Build Summary:" -ForegroundColor Green
Write-Host "=============" -ForegroundColor Green
Write-Host "Executable: .\build\$exePath" -ForegroundColor White
if ($Package) {
    Write-Host "Package: .\cpuminer_windows.zip" -ForegroundColor White
}
Write-Host ""
Write-Host "Usage:" -ForegroundColor Yellow
Write-Host "  .\build\$exePath --help" -ForegroundColor White
Write-Host "  .\build\$exePath --rpc-user bitquantum --rpc-password bitquantum123 --threads 4" -ForegroundColor White
Write-Host "  .\build\$exePath --config ..\config.conf" -ForegroundColor White
Write-Host ""
Write-Host "For more information, see README_Windows.md" -ForegroundColor Cyan
