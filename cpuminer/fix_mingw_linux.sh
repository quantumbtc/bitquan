#!/bin/bash

# Fix MinGW Cross-Compilation Issues for Linux
# Bitquantum RandomQ CPU Miner

echo "Fixing MinGW cross-compilation issues for Linux..."
echo "================================================="

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

# Backup original CMakeLists.txt
cp CMakeLists.txt CMakeLists_backup.txt
echo "Backed up original CMakeLists.txt to CMakeLists_backup.txt"

# Use the MinGW Linux-specific version
cp CMakeLists_mingw_linux.txt CMakeLists.txt
echo "Applied MinGW Linux-specific CMakeLists.txt"

# Clean build directory if it exists
if [ -d "build_mingw" ]; then
    echo "Cleaning build directory..."
    rm -rf build_mingw
fi

echo ""
echo "MinGW cross-compilation fix for Linux completed!"
echo ""
echo "Now you can build CPUMiner for Windows:"
echo "  ./build_mingw_simple.sh"
echo ""
echo "Or manually:"
echo "  mkdir build_mingw && cd build_mingw"
echo "  cmake .. -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++"
echo "  make -j\$(nproc)"
echo ""
