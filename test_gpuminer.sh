#!/bin/bash
# GPU RandomQ Miner Test Script

echo "=== GPU RandomQ Miner Test Script ==="
echo

# Check if gpuminer binary exists
if [ ! -f "build/src/gpuminer-randomq" ]; then
    echo "❌ gpuminer-randomq not found. Please compile first:"
    echo "   cmake -B build -S ."
    echo "   cmake --build build --target gpuminer-randomq"
    exit 1
fi

echo "✅ Found gpuminer-randomq binary"

# Test 1: Check CUDA devices
echo
echo "=== Test 1: CUDA Device Detection ==="
./build/src/gpuminer-randomq -list-gpus
if [ $? -eq 0 ]; then
    echo "✅ GPU detection successful"
else
    echo "❌ GPU detection failed"
    echo "Please check:"
    echo "  - NVIDIA GPU is installed"
    echo "  - NVIDIA drivers are installed"
    echo "  - CUDA toolkit is installed"
    exit 1
fi

# Test 2: Check if bitquantumd is running
echo
echo "=== Test 2: RPC Connection Test ==="
if command -v bitquantum-cli &> /dev/null; then
    if bitquantum-cli getblockchaininfo &> /dev/null; then
        echo "✅ bitquantumd is running and accessible"
        RPC_OK=1
    else
        echo "⚠️  bitquantumd not accessible via RPC"
        echo "Please start bitquantumd with RPC enabled"
        RPC_OK=0
    fi
else
    echo "⚠️  bitquantum-cli not found"
    echo "Please ensure Bitquantum is properly installed"
    RPC_OK=0
fi

# Test 3: Basic parameter validation
echo
echo "=== Test 3: Parameter Validation ==="
./build/src/gpuminer-randomq --help &> /dev/null
if [ $? -eq 0 ]; then
    echo "✅ Help system working"
else
    echo "❌ Help system failed"
fi

# Test 4: Address validation (if provided)
echo
echo "=== Test 4: Mining Address Validation ==="
if [ -z "$1" ]; then
    echo "⚠️  No mining address provided"
    echo "Usage: $0 <btq_address>"
    echo "Example: $0 btq1qxy2kgdygjrsqtzq2n0yrf2493p83kkfjhx0wlh"
    echo
    echo "You can generate an address with:"
    echo "  bitquantum-cli getnewaddress"
    ADDRESS_OK=0
else
    MINING_ADDRESS="$1"
    echo "✅ Mining address provided: $MINING_ADDRESS"
    
    # Basic address format validation
    if [[ $MINING_ADDRESS == btq1* ]] && [[ ${#MINING_ADDRESS} -eq 42 ]]; then
        echo "✅ Address format appears valid (bech32)"
        ADDRESS_OK=1
    else
        echo "⚠️  Address format may be invalid"
        echo "Expected: btq1... (42 characters)"
        echo "Got: $MINING_ADDRESS (${#MINING_ADDRESS} characters)"
        ADDRESS_OK=0
    fi
fi

# Test 5: Quick mining test (if everything is ready)
echo
echo "=== Test 5: Quick Mining Test ==="
if [ $RPC_OK -eq 1 ] && [ $ADDRESS_OK -eq 1 ]; then
    echo "🚀 Starting 10-second mining test..."
    echo "Press Ctrl+C to stop early"
    
    timeout 10s ./build/src/gpuminer-randomq \
        -address=$MINING_ADDRESS \
        -maxtries=100000 \
        2>&1 | head -20
    
    echo
    echo "✅ Mining test completed"
    echo "Check the output above for any errors"
else
    echo "⚠️  Skipping mining test (RPC or address not ready)"
fi

# Summary
echo
echo "=== Test Summary ==="
echo "GPU Detection: $([ -f "build/src/gpuminer-randomq" ] && echo "✅ OK" || echo "❌ FAIL")"
echo "RPC Connection: $([ $RPC_OK -eq 1 ] && echo "✅ OK" || echo "⚠️  NOT READY")"
echo "Mining Address: $([ $ADDRESS_OK -eq 1 ] && echo "✅ OK" || echo "⚠️  NOT PROVIDED")"

echo
if [ $RPC_OK -eq 1 ] && [ $ADDRESS_OK -eq 1 ]; then
    echo "🎉 All tests passed! Ready for mining:"
    echo "   ./build/src/gpuminer-randomq -address=$MINING_ADDRESS"
else
    echo "⚠️  Setup incomplete. Please address the issues above."
fi

echo
echo "=== Useful Commands ==="
echo "List GPUs:           ./build/src/gpuminer-randomq -list-gpus"
echo "Start mining:        ./build/src/gpuminer-randomq -address=<your_address>"
echo "Use specific GPU:    ./build/src/gpuminer-randomq -address=<your_address> -gpu=0"
echo "Monitor hash rate:   watch -n 1 'nvidia-smi'"
echo "Get new address:     bitquantum-cli getnewaddress"
echo "Check node status:   bitquantum-cli getblockchaininfo"
