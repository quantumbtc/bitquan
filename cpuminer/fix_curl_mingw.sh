#!/bin/bash

# Fix CURL Library Issues for MinGW Cross-Compilation
# Bitquantum RandomQ CPU Miner

echo "Fixing CURL library issues for MinGW cross-compilation..."
echo "======================================================="

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

# Install MinGW CURL development package
echo "Installing MinGW CURL development package..."

if command -v apt-get &> /dev/null; then
    echo "Detected Ubuntu/Debian system"
    sudo apt-get update
    sudo apt-get install -y libcurl4-openssl-dev
    sudo apt-get install -y mingw-w64-dev
    # Try to install MinGW CURL if available
    sudo apt-get install -y libcurl4-openssl-dev:amd64 || true
elif command -v yum &> /dev/null; then
    echo "Detected CentOS/RHEL system"
    sudo yum install -y libcurl-devel
    sudo yum install -y mingw64-gcc-c++
elif command -v pacman &> /dev/null; then
    echo "Detected Arch Linux system"
    sudo pacman -S --noconfirm curl
    sudo pacman -S --noconfirm mingw-w64-gcc
else
    echo "Unknown system. Please install CURL and MinGW manually."
    exit 1
fi

# Check if MinGW CURL headers exist
echo "Checking MinGW CURL headers..."
if [ -f "/usr/x86_64-w64-mingw32/include/curl/curl.h" ]; then
    echo "MinGW CURL headers found at /usr/x86_64-w64-mingw32/include/curl/curl.h"
elif [ -f "/usr/i686-w64-mingw32/include/curl/curl.h" ]; then
    echo "MinGW CURL headers found at /usr/i686-w64-mingw32/include/curl/curl.h"
else
    echo "MinGW CURL headers not found. Trying to create symlinks..."
    
    # Try to create symlinks to system CURL headers
    if [ -f "/usr/include/curl/curl.h" ]; then
        echo "Creating symlink to system CURL headers..."
        sudo mkdir -p /usr/x86_64-w64-mingw32/include/curl
        sudo ln -sf /usr/include/curl/curl.h /usr/x86_64-w64-mingw32/include/curl/curl.h
        sudo ln -sf /usr/include/curl/curlver.h /usr/x86_64-w64-mingw32/include/curl/curlver.h
        sudo ln -sf /usr/include/curl/easy.h /usr/x86_64-w64-mingw32/include/curl/easy.h
        sudo ln -sf /usr/include/curl/multi.h /usr/x86_64-w64-mingw32/include/curl/multi.h
        sudo ln -sf /usr/include/curl/typecheck-gcc.h /usr/x86_64-w64-mingw32/include/curl/typecheck-gcc.h
        sudo ln -sf /usr/include/curl/urlapi.h /usr/x86_64-w64-mingw32/include/curl/urlapi.h
        echo "Symlinks created successfully"
    else
        echo "System CURL headers not found. Please install libcurl development package."
        exit 1
    fi
fi

# Check if MinGW CURL library exists
echo "Checking MinGW CURL library..."
if [ -f "/usr/x86_64-w64-mingw32/lib/libcurl.a" ] || [ -f "/usr/x86_64-w64-mingw32/lib/libcurl.dll.a" ]; then
    echo "MinGW CURL library found"
elif [ -f "/usr/lib/x86_64-linux-gnu/libcurl.a" ]; then
    echo "Creating symlink to system CURL library..."
    sudo mkdir -p /usr/x86_64-w64-mingw32/lib
    sudo ln -sf /usr/lib/x86_64-linux-gnu/libcurl.a /usr/x86_64-w64-mingw32/lib/libcurl.a
    echo "Symlink created successfully"
else
    echo "CURL library not found. Please install libcurl development package."
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
    -DCURL_INCLUDE_DIR=/usr/x86_64-w64-mingw32/include \
    -DCURL_LIBRARY=/usr/x86_64-w64-mingw32/lib/libcurl.a \
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
