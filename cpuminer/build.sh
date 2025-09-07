#!/bin/bash

# CPUMiner Build Script
# Bitquantum RandomQ CPU Miner

set -e

echo "Building CPUMiner using CMake"
echo "============================="

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
VERBOSE=false
WINDOWS=false
TOOLCHAIN_FILE="depends/x86_64-w64-mingw32/toolchain.cmake"

# Detect number of processors with portable fallback
if command -v nproc >/dev/null 2>&1; then
    THREADS=$(nproc)
elif command -v getconf >/dev/null 2>&1; then
    THREADS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)
else
    THREADS=1
fi

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
            if [[ -n "$2" && ! "$2" =~ ^- ]]; then
                THREADS="$2"
                shift 2
            else
                echo "Error: --jobs requires a numeric argument"
                exit 1
            fi
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        --windows)
            WINDOWS=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  -d, --debug     Build in Debug mode (default: Release)"
            echo "  -c, --clean     Clean build directory before building"
            echo "  -j, --jobs N    Number of parallel jobs (default: $THREADS)"
            echo "  -v, --verbose   Verbose output"
            echo "  --windows       Cross-compile for Windows (requires mingw-w64 and toolchain file)"
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

# Clean build directory if requested
if [ "$CLEAN" = true ]; then
    echo "Cleaning build directory..."
    rm -rf build
fi

# Create build directory
echo "Creating build directory..."
mkdir -p build
cd build

# Configure with CMake (use array to avoid word-splitting)
echo "Configuring with CMake..."
CMAKE_ARGS=( -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_CXX_STANDARD=20 )
if [ "$VERBOSE" = true ]; then
    CMAKE_ARGS+=( -DCMAKE_VERBOSE_MAKEFILE=ON )
fi

if [ "$WINDOWS" = true ]; then
    if [ ! -f "../$TOOLCHAIN_FILE" ]; then
        echo "Error: Windows toolchain file not found: $TOOLCHAIN_FILE"
        exit 1
    fi
    echo "Cross-compiling for Windows using MinGW toolchain..."
    cmake .. -DCMAKE_TOOLCHAIN_FILE="../$TOOLCHAIN_FILE" "${CMAKE_ARGS[@]}"
else
    cmake .. "${CMAKE_ARGS[@]}"
fi

# Build
echo "Building with $THREADS parallel jobs..."
if [ "$VERBOSE" = true ]; then
    cmake --build . --parallel "$THREADS" --verbose
else
    cmake --build . --parallel "$THREADS"
fi

echo ""
if [ "$WINDOWS" = true ]; then
    EXE_PATH="$(pwd)/cpuminer.exe"
    echo "Windows build completed successfully!"
    echo "Executable: $EXE_PATH"
    echo ""
    echo "Usage examples:"
    echo "  $EXE_PATH --help"
    echo "  $EXE_PATH --rpc-user bitquantum --rpc-password bitquantum123 --threads 4"
    echo "  $EXE_PATH --config ../config.conf"
    echo ""
else
    EXE_PATH="$(pwd)/cpuminer"
    echo "Build completed successfully!"
    echo "Executable: $EXE_PATH"
    echo ""
    echo "Usage examples:"
    echo "  $EXE_PATH --help"
    echo "  $EXE_PATH --rpc-user bitquantum --rpc-password bitquantum123 --threads 4"
    echo "  $EXE_PATH --config ../config.conf"
    echo ""
fi
