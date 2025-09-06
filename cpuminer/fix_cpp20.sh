#!/bin/bash

# Fix C++20 Compatibility Issues
# Bitquantum RandomQ CPU Miner

echo "Fixing C++20 compatibility issues..."
echo "==================================="

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: CMakeLists.txt not found. Please run this script from the cpuminer directory."
    exit 1
fi

# Check compiler version
echo "Checking compiler version..."
if command -v g++ &> /dev/null; then
    GCC_VERSION=$(g++ -dumpversion | cut -d. -f1)
    echo "GCC version: $GCC_VERSION"
    if [ "$GCC_VERSION" -lt 10 ]; then
        echo "Warning: GCC version $GCC_VERSION is too old. C++20 requires GCC 10+"
        echo "Please upgrade your compiler or use a different one."
    fi
fi

if command -v clang++ &> /dev/null; then
    CLANG_VERSION=$(clang++ --version | grep -o '[0-9]\+\.[0-9]\+' | head -1 | cut -d. -f1)
    echo "Clang version: $CLANG_VERSION"
    if [ "$CLANG_VERSION" -lt 10 ]; then
        echo "Warning: Clang version $CLANG_VERSION is too old. C++20 requires Clang 10+"
        echo "Please upgrade your compiler or use a different one."
    fi
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
