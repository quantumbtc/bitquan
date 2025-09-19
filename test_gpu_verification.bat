@echo off
echo Testing GPU RandomQ verification...

REM Copy kernel file to current directory for testing
copy "src\tools\randomq_kernel.cl" "randomq_kernel.cl" >nul 2>&1

REM Test with relative path
echo Testing with relative path...
build\src\gpuminer-randomq.exe -verify -gpu

REM Test with explicit kernel path
echo.
echo Testing with explicit kernel path...
build\src\gpuminer-randomq.exe -verify -gpu --kernel="src\tools\randomq_kernel.cl"

REM Test with environment variable
echo.
echo Testing with environment variable...
set BTQ_OPENCL_KERNEL=src\tools\randomq_kernel.cl
build\src\gpuminer-randomq.exe -verify -gpu

pause
