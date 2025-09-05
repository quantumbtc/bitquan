#!/bin/bash

# CPUMiner MinGW Cross-Compilation Script for Linux
# Bitquantum RandomQ CPU Miner

set -e

echo "Building CPUMiner for Windows using MinGW Cross-Compilation"
echo "=========================================================="

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

# Check for MinGW libcurl
echo "Checking MinGW libcurl..."
if ! x86_64-w64-mingw32-pkg-config --exists libcurl 2>/dev/null; then
    echo "Installing MinGW libcurl..."
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

# Create build directory
echo "Creating build directory..."
mkdir -p build_mingw
cd build_mingw

# Configure with CMake for MinGW cross-compilation
echo "Configuring with CMake for MinGW cross-compilation..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_CXX_FLAGS="-O3 -march=native -mtune=native -static-libgcc -static-libstdc++ -D_WIN32_WINNT=0x0601 -D_WIN32_IE=0x0800 -D_WIN32 -D_WIN64 -D__USE_MINGW_ANSI_STDIO=1" \
    -DCMAKE_FIND_ROOT_PATH=/usr/x86_64-w64-mingw32 \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY

# Build
echo "Building..."
cmake --build . --parallel

echo ""
echo "Build completed successfully!"
echo "Executable: ./build_mingw/cpuminer.exe"
echo ""
echo "Usage examples:"
echo "  ./build_mingw/cpuminer.exe --help"
echo "  ./build_mingw/cpuminer.exe --rpc-user bitquantum --rpc-password bitquantum123 --threads 4"
echo "  ./build_mingw/cpuminer.exe --config ../config.conf"
echo ""
