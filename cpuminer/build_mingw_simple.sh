#!/bin/bash

# CPUMiner Simple MinGW Cross-Compilation Script
# Bitquantum RandomQ CPU Miner

set -e

echo "Building CPUMiner for Windows using MinGW - Simple Method"
echo "========================================================"

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: CMakeLists.txt not found. Please run this script from the cpuminer directory."
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

# Create build directory
echo "Creating build directory..."
mkdir -p build_mingw
cd build_mingw

# Configure with CMake for MinGW
echo "Configuring with CMake for MinGW..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_CXX_FLAGS="-O3 -static-libgcc -static-libstdc++ -D_WIN32_WINNT=0x0601 -D_WIN32_IE=0x0800 -D_WIN32 -D_WIN64 -D__USE_MINGW_ANSI_STDIO=1"

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
