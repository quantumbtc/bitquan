#!/bin/bash

# Test script for multi-core RandomQ mining in BitQuantum
# This script demonstrates the enhanced generatetoaddress command with CPU multi-core support

echo "=== BitQuantum Multi-Core Mining Test ==="
echo

# Check if bitquantum-cli is available
if ! command -v bitquantum-cli &> /dev/null; then
    echo "Error: bitquantum-cli not found. Please ensure BitQuantum is built and bitquantum-cli is in your PATH."
    exit 1
fi

# Test address (replace with your own address)
TEST_ADDRESS="btq1qrv7jvtvjwhe33qvpqrns9pllwhzz8ytdafra6l"

echo "Testing address: $TEST_ADDRESS"
echo

# Test 1: Single-threaded mining
echo "=== Test 1: Single-threaded mining (1 block) ==="
echo "Command: bitquantum-cli generatetoaddress 1 $TEST_ADDRESS 100000 1"
echo
bitquantum-cli generatetoaddress 1 "$TEST_ADDRESS" 100000 1
echo

# Test 2: Multi-threaded mining (auto-detect cores)
echo "=== Test 2: Multi-threaded mining with auto-detection (1 block) ==="
echo "Command: bitquantum-cli generatetoaddress 1 $TEST_ADDRESS 100000 0"
echo
bitquantum-cli generatetoaddress 1 "$TEST_ADDRESS" 100000 0
echo

# Test 3: Multi-threaded mining (4 threads)
echo "=== Test 3: Multi-threaded mining with 4 threads (1 block) ==="
echo "Command: bitquantum-cli generatetoaddress 1 $TEST_ADDRESS 100000 4"
echo
bitquantum-cli generatetoaddress 1 "$TEST_ADDRESS" 100000 4
echo

# Test 4: Performance comparison
echo "=== Test 4: Performance comparison (2 blocks each) ==="
echo

echo "Single-threaded (2 blocks):"
time bitquantum-cli generatetoaddress 2 "$TEST_ADDRESS" 100000 1
echo

echo "Multi-threaded with 4 threads (2 blocks):"
time bitquantum-cli generatetoaddress 2 "$TEST_ADDRESS" 100000 4
echo

echo "=== Test completed ==="
echo "Note: Mining performance will vary based on your CPU and current network difficulty."
echo "The multi-threaded version should show improved hash rates and faster block generation."
