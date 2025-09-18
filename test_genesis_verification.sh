#!/bin/bash

echo "========================================"
echo "Genesis Block Verification Test Suite"
echo "========================================"
echo

echo "[Test 1] CPU RandomQ Algorithm Verification"
echo "--------------------------------------------"
echo "Testing CPU implementation against known genesis block..."
echo
./gpuminer-randomq -verify
cpu_result=$?
echo
echo "CPU test completed with exit code: $cpu_result"
read -p "Press Enter to continue to GPU test..."

echo
echo "[Test 2] GPU RandomQ Algorithm Verification"  
echo "--------------------------------------------"
echo "Testing GPU implementation against known genesis block..."
echo "This will also compare GPU vs CPU performance..."
echo
./gpuminer-randomq -verify -gpu
gpu_result=$?
echo
echo "GPU test completed with exit code: $gpu_result"

echo
echo "========================================"
echo "Verification Test Suite Results"
echo "========================================"
echo

if [ $cpu_result -eq 0 ]; then
    echo "✅ CPU Test: PASSED"
else
    echo "❌ CPU Test: FAILED (exit code: $cpu_result)"
fi

if [ $gpu_result -eq 0 ]; then
    echo "✅ GPU Test: PASSED"
else
    echo "❌ GPU Test: FAILED (exit code: $gpu_result)"
fi

echo
if [ $cpu_result -eq 0 ] && [ $gpu_result -eq 0 ]; then
    echo "🎉 ALL TESTS PASSED! Your RandomQ implementation is correct!"
    exit 0
else
    echo "💥 SOME TESTS FAILED! Check the debug output above for details."
    exit 1
fi
