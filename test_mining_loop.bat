@echo off
chcp 65001 >nul
echo === 比特币循环挖矿测试 ===
echo.

REM 检查比特币节点是否运行
bitcoin-cli getblockchaininfo >nul 2>&1
if errorlevel 1 (
    echo 错误: 比特币节点未运行或无法连接
    echo 请确保 bitcoind 正在运行并且可以连接
    pause
    exit /b 1
)

echo ✓ 比特币节点连接正常
echo.

REM 显示当前区块链信息
echo 当前区块链信息:
bitcoin-cli getblockchaininfo | findstr /R "blocks difficulty chainwork"
echo.

REM 显示钱包信息
echo 钱包信息:
bitcoin-cli getwalletinfo | findstr /R "walletname balance txcount"
echo.

echo === 开始循环挖矿测试 ===
echo 注意: 按 Ctrl+C 停止挖矿
echo.

REM 开始循环挖矿 (每次生成1个区块，最大尝试次数1000)
bitcoin-cli -generate loop 1 1000

pause
