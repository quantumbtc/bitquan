#!/bin/bash

# CPUMiner Windows Build Script
# Bitquantum RandomQ CPU Miner

set -e

echo "Building CPUMiner for Windows using CMake"
echo "========================================="

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

# Parse command line arguments
BUILD_TYPE="Release"
CLEAN=false
THREADS=$(nproc)
VERBOSE=false
COMPILER="mingw"

while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -j|--jobs)
            THREADS="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        --msvc)
            COMPILER="msvc"
            shift
            ;;
        --mingw)
            COMPILER="mingw"
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  -d, --debug     Build in Debug mode (default: Release)"
            echo "  -c, --clean     Clean build directory before building"
            echo "  -j, --jobs N    Number of parallel jobs (default: $(nproc))"
            echo "  -v, --verbose   Verbose output"
            echo "  --msvc          Use MSVC compiler (Windows only)"
            echo "  --mingw         Use MinGW compiler (default)"
            echo "  -h, --help      Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

# Check if MinGW is available
if [ "$COMPILER" = "mingw" ]; then
    if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
        echo "Error: MinGW-w64 not found. Please install MinGW-w64."
        echo "Ubuntu/Debian: sudo apt-get install mingw-w64"
        echo "CentOS/RHEL: sudo yum install mingw64-gcc"
        echo "Arch Linux: sudo pacman -S mingw-w64-gcc"
        exit 1
    fi
fi

# Clean build directory if requested
if [ "$CLEAN" = true ]; then
    echo "Cleaning build directory..."
    rm -rf build_windows
fi

# Create build directory
echo "Creating build directory..."
mkdir -p build_windows
cd build_windows

# Configure with CMake
echo "Configuring with CMake for Windows..."
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_CXX_STANDARD=20"

if [ "$COMPILER" = "mingw" ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_SYSTEM_NAME=Windows"
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_SYSTEM_PROCESSOR=x86_64"
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc"
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++"
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_FIND_ROOT_PATH=/usr/x86_64-w64-mingw32"
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY"
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY"
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY"
elif [ "$COMPILER" = "msvc" ]; then
    CMAKE_ARGS="$CMAKE_ARGS -G \"Visual Studio 17 2022\""
    CMAKE_ARGS="$CMAKE_ARGS -A x64"
fi

if [ "$VERBOSE" = true ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_VERBOSE_MAKEFILE=ON"
fi

cmake .. $CMAKE_ARGS

# Build
echo "Building with $THREADS parallel jobs..."
if [ "$VERBOSE" = true ]; then
    cmake --build . --parallel $THREADS --verbose
else
    cmake --build . --parallel $THREADS
fi

echo ""
echo "Build completed successfully!"
if [ "$COMPILER" = "mingw" ]; then
    echo "Executable: ./build_windows/cpuminer.exe"
    echo ""
    echo "Usage examples:"
    echo "  ./build_windows/cpuminer.exe --help"
    echo "  ./build_windows/cpuminer.exe --rpc-user bitquantum --rpc-password bitquantum123 --threads 4"
    echo "  ./build_windows/cpuminer.exe --config ../config.conf"
else
    echo "Executable: ./build_windows/bin/cpuminer.exe"
    echo ""
    echo "Usage examples:"
    echo "  ./build_windows/bin/cpuminer.exe --help"
    echo "  ./build_windows/bin/cpuminer.exe --rpc-user bitquantum --rpc-password bitquantum123 --threads 4"
    echo "  ./build_windows/bin/cpuminer.exe --config ../config.conf"
fi
echo ""
