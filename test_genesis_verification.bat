@echo off
echo ========================================
echo Genesis Block Verification Test Suite
echo ========================================
echo.

echo [Test 1] CPU RandomQ Algorithm Verification
echo --------------------------------------------
echo Testing CPU implementation against known genesis block...
echo.
gpuminer-randomq -verify
echo.
echo CPU test completed. Press any key to continue to GPU test...
pause >nul

echo.
echo [Test 2] GPU RandomQ Algorithm Verification  
echo --------------------------------------------
echo Testing GPU implementation against known genesis block...
echo This will also compare GPU vs CPU performance...
echo.
gpuminer-randomq -verify -gpu
echo.
echo GPU test completed.

echo.
echo ========================================
echo Verification Test Suite Completed
echo ========================================
echo.
echo If both tests show "SUCCESS", your RandomQ implementation is correct!
echo If any test shows "FAILURE", check the debug output above for details.
echo.
pause
