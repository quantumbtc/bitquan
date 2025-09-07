#!/bin/bash

# Final Cleanup Script - Remove All Windows Files
# Bitquantum RandomQ CPU Miner

echo "Final cleanup - Removing all Windows files..."
echo "============================================="

# Remove Windows build directories
if [ -d "build_windows" ]; then
    echo "Removing build_windows directory..."
    rm -rf build_windows
fi

if [ -d "build" ]; then
    echo "Removing build directory..."
    rm -rf build
fi

# Remove vcpkg directory
if [ -d "vcpkg" ]; then
    echo "Removing vcpkg directory..."
    rm -rf vcpkg
fi

# Remove cmake directory (if empty)
if [ -d "cmake" ]; then
    echo "Removing cmake directory..."
    rm -rf cmake
fi

# Remove CMake cache files
if [ -f "CMakeCache.txt" ]; then
    echo "Removing CMakeCache.txt..."
    rm -f CMakeCache.txt
fi

if [ -d "CMakeFiles" ]; then
    echo "Removing CMakeFiles directory..."
    rm -rf CMakeFiles
fi

if [ -f "cmake_install.cmake" ]; then
    echo "Removing cmake_install.cmake..."
    rm -f cmake_install.cmake
fi

if [ -f "Makefile" ]; then
    echo "Removing Makefile..."
    rm -f Makefile
fi

# Remove temporary files
echo "Removing temporary files..."
rm -f *.tmp *.log *.tlog

echo ""
echo "Cleanup completed!"
echo ""
echo "Remaining files:"
ls -la
echo ""
echo "Project is now Linux-only!"
