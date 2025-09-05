#!/bin/bash

# Fix MinGW Cross-Compilation Issues
# Bitquantum RandomQ CPU Miner

echo "Fixing MinGW cross-compilation issues..."
echo "======================================="

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

# Use the MinGW-specific version
cp CMakeLists_mingw.txt CMakeLists.txt
echo "Applied MinGW-specific CMakeLists.txt"

# Clean build directory if it exists
if [ -d "build" ]; then
    echo "Cleaning build directory..."
    rm -rf build
fi

echo ""
echo "MinGW cross-compilation fix completed!"
echo ""
echo "Now you can build CPUMiner for Windows:"
echo "  mkdir build && cd build"
echo "  cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchain-mingw.cmake"
echo "  make -j\$(nproc)"
echo ""
echo "Or use the MinGW build script:"
echo "  ./build_windows_mingw.bat"
echo ""
