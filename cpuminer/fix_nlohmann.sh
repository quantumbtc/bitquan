#!/bin/bash

# Fix nlohmann-json Compatibility Issues
# Bitquantum RandomQ CPU Miner

echo "Fixing nlohmann-json compatibility issues..."
echo "============================================="

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: CMakeLists.txt not found. Please run this script from the cpuminer directory."
    exit 1
fi

# Backup original CMakeLists.txt
cp CMakeLists.txt CMakeLists_backup.txt
echo "Backed up original CMakeLists.txt to CMakeLists_backup.txt"

# Use the fixed version
cp CMakeLists_nlohmann_fix.txt CMakeLists.txt
echo "Applied nlohmann-json compatibility fix"

# Clean build directory if it exists
if [ -d "build" ]; then
    echo "Cleaning build directory..."
    rm -rf build
fi

echo ""
echo "nlohmann-json compatibility fix completed!"
echo ""
echo "Now you can build CPUMiner:"
echo "  ./build_fixed.sh"
echo ""
echo "Or manually:"
echo "  mkdir build && cd build"
echo "  cmake .. -DCMAKE_BUILD_TYPE=Release"
echo "  make -j\$(nproc)"
echo ""
