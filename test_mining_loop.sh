#!/bin/bash

# Test Bitquantum loop mining feature
# Usage: ./test_mining_loop.sh

echo "=== Bitquantum Loop Mining Test ==="
echo ""

# Check if node is running
if ! bitquantum-cli getblockchaininfo > /dev/null 2>&1; then
    echo "Error: node not running or unreachable"
    echo "Ensure bitquantumd is running and reachable"
    exit 1
fi

echo "✓ Node connection OK"
echo ""

# Show current chain info
echo "Current blockchain info:"
bitquantum-cli getblockchaininfo | grep -E "(blocks|difficulty|chainwork)"
echo ""

# Show wallet info
echo "Wallet info:"
bitquantum-cli getwalletinfo | grep -E "(walletname|balance|txcount)"
echo ""

echo "=== Start loop mining test ==="
echo "Note: Press Ctrl+C to stop"
echo ""

# Start loop mining (1 block per iteration, max 1000 tries)
bitquantum-cli -generate loop 1 1000
