@echo off
echo Testing GPU RandomQ Verification...
echo.

REM Copy kernel file to current directory for easy access
if not exist "randomq_kernel.cl" (
    echo Copying kernel file...
    copy "src\tools\randomq_kernel.cl" "randomq_kernel.cl"
    if errorlevel 1 (
        echo ERROR: Failed to copy kernel file
        echo Please make sure src\tools\randomq_kernel.cl exists
        pause
        exit /b 1
    )
)

echo Running GPU verification...
gpuminer-randomq -verify -gpu

echo.
echo Test completed!
pause
