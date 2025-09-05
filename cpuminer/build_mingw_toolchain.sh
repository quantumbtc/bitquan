#!/bin/bash

# CPUMiner MinGW Cross-Compilation Script using Toolchain
# Bitquantum RandomQ CPU Miner

set -e

echo "Building CPUMiner for Windows using MinGW Toolchain"
echo "=================================================="

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

# Check if MinGW is available
if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
    echo "Error: MinGW-w64 not found. Please install MinGW-w64."
    echo "Ubuntu/Debian: sudo apt-get install mingw-w64"
    echo "CentOS/RHEL: sudo yum install mingw64-gcc"
    echo "Arch Linux: sudo pacman -S mingw-w64-gcc"
    exit 1
fi

# Check if MinGW headers exist
if [ ! -d "/usr/x86_64-w64-mingw32/include" ]; then
    echo "Error: MinGW headers not found at /usr/x86_64-w64-mingw32/include"
    echo "Please install MinGW-w64 development packages:"
    echo "Ubuntu/Debian: sudo apt-get install mingw-w64-dev"
    echo "CentOS/RHEL: sudo yum install mingw64-gcc-c++"
    echo "Arch Linux: sudo pacman -S mingw-w64-gcc"
    exit 1
fi

# Install dependencies if needed
echo "Checking dependencies..."

# Check for nlohmann-json
if ! pkg-config --exists nlohmann_json 2>/dev/null && \
   [ ! -f "/usr/include/nlohmann/json.hpp" ] && \
   [ ! -f "/usr/local/include/nlohmann/json.hpp" ]; then
    echo "Installing nlohmann-json..."
    if command -v apt-get &> /dev/null; then
        sudo apt-get update
        sudo apt-get install -y nlohmann-json3-dev
    elif command -v yum &> /dev/null; then
        sudo yum install -y nlohmann-json3-devel
    elif command -v pacman &> /dev/null; then
        sudo pacman -S --noconfirm nlohmann-json
    else
        echo "Please install nlohmann-json manually"
        exit 1
    fi
fi

# Check for libcurl
if ! pkg-config --exists libcurl 2>/dev/null; then
    echo "Installing libcurl..."
    if command -v apt-get &> /dev/null; then
        sudo apt-get update
        sudo apt-get install -y libcurl4-openssl-dev
    elif command -v yum &> /dev/null; then
        sudo yum install -y libcurl-devel
    elif command -v pacman &> /dev/null; then
        sudo pacman -S --noconfirm curl
    else
        echo "Please install libcurl manually"
        exit 1
    fi
fi

# Create build directory
echo "Creating build directory..."
mkdir -p build_mingw_toolchain
cd build_mingw_toolchain

# Configure with CMake using toolchain file
echo "Configuring with CMake using MinGW toolchain..."
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchain-mingw-linux.cmake \
    -DCMAKE_BUILD_TYPE=Release

# Build
echo "Building..."
cmake --build . --parallel

echo ""
echo "Build completed successfully!"
echo "Executable: ./build_mingw_toolchain/cpuminer.exe"
echo ""
echo "Usage examples:"
echo "  ./build_mingw_toolchain/cpuminer.exe --help"
echo "  ./build_mingw_toolchain/cpuminer.exe --rpc-user bitquantum --rpc-password bitquantum123 --threads 4"
echo "  ./build_mingw_toolchain/cpuminer.exe --config ../config.conf"
echo ""
