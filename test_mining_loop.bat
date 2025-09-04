@echo off
chcp 65001 >nul
echo === Bitcoin Loop Mining Test ===
echo.

REM Check if node is running
bitcoin-cli getblockchaininfo >nul 2>&1
if errorlevel 1 (
    echo Error: node not running or unreachable
    echo Ensure bitcoind is running and reachable
    pause
    exit /b 1
)

echo ✓ Node connection OK
echo.

REM Show current blockchain info
echo Current blockchain info:
bitcoin-cli getblockchaininfo | findstr /R "blocks difficulty chainwork"
echo.

REM Show wallet info
echo Wallet info:
bitcoin-cli getwalletinfo | findstr /R "walletname balance txcount"
echo.

echo === Start loop mining test ===
echo Note: Press Ctrl+C to stop
echo.

REM Start loop mining (1 block per iteration, max 1000 tries)
bitcoin-cli -generate loop 1 1000

pause
