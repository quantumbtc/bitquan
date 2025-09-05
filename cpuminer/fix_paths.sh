#!/bin/bash

# Fix Paths Script for CPUMiner
# Bitquantum RandomQ CPU Miner

echo "Fixing CMakeLists.txt paths..."
echo "=============================="

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: CMakeLists.txt not found. Please run this script from the cpuminer directory."
    exit 1
fi

# Check if we're in the bitquan directory
if [ ! -d "../src" ]; then
    echo "Error: src directory not found. Please run this script from the bitquan/cpuminer directory."
    echo "Current directory: $(pwd)"
    echo "Expected structure: bitquan/cpuminer/"
    exit 1
fi

# Backup original CMakeLists.txt
cp CMakeLists.txt CMakeLists_backup.txt
echo "Backed up original CMakeLists.txt to CMakeLists_backup.txt"

# Use the fixed version
cp CMakeLists_fixed.txt CMakeLists.txt
echo "Applied fixed CMakeLists.txt"

echo ""
echo "Path fix completed!"
echo ""
echo "Now you can build CPUMiner:"
echo "  ./build_simple.sh"
echo ""
echo "Or manually:"
echo "  mkdir build && cd build"
echo "  cmake .. -DCMAKE_BUILD_TYPE=Release"
echo "  make -j\$(nproc)"
echo ""
