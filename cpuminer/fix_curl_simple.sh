#!/bin/bash

# Simple CURL Fix for MinGW Cross-Compilation
# Bitquantum RandomQ CPU Miner

echo "Fixing CURL library issues for MinGW - Simple Method"
echo "==================================================="

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: CMakeLists.txt not found. Please run this script from the cpuminer directory."
    exit 1
fi

# Install CURL development package
echo "Installing CURL development package..."
if command -v apt-get &> /dev/null; then
    sudo apt-get update
    sudo apt-get install -y libcurl4-openssl-dev
elif command -v yum &> /dev/null; then
    sudo yum install -y libcurl-devel
elif command -v pacman &> /dev/null; then
    sudo pacman -S --noconfirm curl
else
    echo "Please install libcurl development package manually"
    exit 1
fi

# Clean build directory if it exists
if [ -d "build_mingw" ]; then
    echo "Cleaning build directory..."
    rm -rf build_mingw
fi

# Create build directory
echo "Creating build directory..."
mkdir -p build_mingw
cd build_mingw

# Configure with CMake using explicit CURL paths
echo "Configuring with CMake for MinGW cross-compilation..."
cmake .. \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_FIND_ROOT_PATH=/usr/x86_64-w64-mingw32 \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=17 \
    -DCURL_INCLUDE_DIR=/usr/include \
    -DCURL_LIBRARY=/usr/lib/x86_64-linux-gnu/libcurl.so \
    -DCMAKE_CXX_FLAGS="-O3 -march=native -mtune=native -static-libgcc -static-libstdc++ -D_WIN32_WINNT=0x0601 -D_WIN32_IE=0x0800 -D_WIN32 -D_WIN64 -D__USE_MINGW_ANSI_STDIO=1 -isystem /usr/x86_64-w64-mingw32/include -isystem /usr/lib/gcc/x86_64-w64-mingw32/13-win32/include"

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
