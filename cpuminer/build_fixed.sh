#!/bin/bash

# CPUMiner Fixed Build Script
# Bitquantum RandomQ CPU Miner

set -e

echo "Building CPUMiner - Fixed Version"
echo "================================="

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

# Check for build tools
if ! command -v cmake &> /dev/null; then
    echo "Installing cmake..."
    if command -v apt-get &> /dev/null; then
        sudo apt-get install -y cmake build-essential
    elif command -v yum &> /dev/null; then
        sudo yum install -y cmake gcc-c++ make
    elif command -v pacman &> /dev/null; then
        sudo pacman -S --noconfirm cmake base-devel
    else
        echo "Please install cmake manually"
        exit 1
    fi
fi

# Create build directory
echo "Creating build directory..."
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-std=c++17 -D_GLIBCXX_USE_C99"

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
