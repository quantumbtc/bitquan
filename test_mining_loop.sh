#!/bin/bash

# 测试比特币循环挖矿功能
# 使用方法: ./test_mining_loop.sh

echo "=== 比特币循环挖矿测试 ==="
echo ""

# 检查比特币节点是否运行
if ! bitcoin-cli getblockchaininfo > /dev/null 2>&1; then
    echo "错误: 比特币节点未运行或无法连接"
    echo "请确保 bitcoind 正在运行并且可以连接"
    exit 1
fi

echo "✓ 比特币节点连接正常"
echo ""

# 显示当前区块链信息
echo "当前区块链信息:"
bitcoin-cli getblockchaininfo | grep -E "(blocks|difficulty|chainwork)"
echo ""

# 显示钱包信息
echo "钱包信息:"
bitcoin-cli getwalletinfo | grep -E "(walletname|balance|txcount)"
echo ""

echo "=== 开始循环挖矿测试 ==="
echo "注意: 按 Ctrl+C 停止挖矿"
echo ""

# 开始循环挖矿 (每次生成1个区块，最大尝试次数1000)
bitcoin-cli -generate loop 1 1000
