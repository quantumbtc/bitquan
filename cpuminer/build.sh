#!/bin/bash

# CPUMiner Build Script
# Bitquantum RandomQ CPU Miner

set -e

echo "Building CPUMiner - Bitquantum RandomQ CPU Miner"
echo "================================================"

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: CMakeLists.txt not found. Please run this script from the cpuminer directory."
    exit 1
fi

# Create build directory
echo "Creating build directory..."
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_CXX_FLAGS="-O3 -march=native -mtune=native"

# Build
echo "Building..."
make -j$(nproc)

echo ""
echo "Build completed successfully!"
echo "Executable: ./build/cpuminer"
echo ""
echo "Usage examples:"
echo "  ./build/cpuminer --help"
echo "  ./build/cpuminer --rpc-user bitquantum --rpc-password bitquantum123 --threads 4"
echo "  ./build/cpuminer --config ../config.conf"
echo ""
