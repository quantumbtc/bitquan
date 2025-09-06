#!/bin/bash

# Fix Compilation Errors Script
# Bitquantum RandomQ CPU Miner

echo "Fixing compilation errors..."
echo "==========================="

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: CMakeLists.txt not found. Please run this script from the cpuminer directory."
    exit 1
fi

# Clean build directory if it exists
if [ -d "build" ]; then
    echo "Cleaning build directory..."
    rm -rf build
fi

# Create build directory
echo "Creating build directory..."
mkdir -p build
cd build

# Configure with CMake using C++20
echo "Configuring with CMake for C++20..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=20 \
    -DCMAKE_CXX_STANDARD_REQUIRED=ON \
    -DCMAKE_CXX_EXTENSIONS=OFF

# Build
echo "Building..."
cmake --build . --parallel

echo ""
echo "Build completed successfully!"
echo "Executable: ./build/cpuminer"
echo ""
echo "Usage examples:"
echo "  ./build/cpuminer --help"
echo "  ./build/cpuminer --rpc-user bitquantum --rpc-password bitquantum123 --threads 4"
echo "  ./build/cpuminer --config ../config.conf"
echo ""
