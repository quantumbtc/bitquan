#!/bin/bash

# CPUMiner Build Script with Dependency Fix
# Bitquantum RandomQ CPU Miner

set -e

echo "Building CPUMiner with Dependency Fix"
echo "====================================="

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: CMakeLists.txt not found. Please run this script from the cpuminer directory."
    exit 1
fi

# Function to install nlohmann-json
install_nlohmann_json() {
    echo "Installing nlohmann-json..."
    
    # Try package manager first
    if command -v apt-get &> /dev/null; then
        echo "Trying apt-get..."
        sudo apt-get update
        sudo apt-get install -y nlohmann-json3-dev || true
    elif command -v yum &> /dev/null; then
        echo "Trying yum..."
        sudo yum install -y nlohmann-json3-devel || true
    elif command -v pacman &> /dev/null; then
        echo "Trying pacman..."
        sudo pacman -S --noconfirm nlohmann-json || true
    fi
    
    # Check if installation was successful
    if pkg-config --exists nlohmann_json 2>/dev/null; then
        echo "nlohmann-json found via pkg-config"
        return 0
    fi
    
    # Try to find header file
    if [ -f "/usr/include/nlohmann/json.hpp" ] || [ -f "/usr/local/include/nlohmann/json.hpp" ]; then
        echo "nlohmann-json header found"
        return 0
    fi
    
    # Manual installation
    echo "Installing nlohmann-json manually..."
    mkdir -p deps
    cd deps
    
    if [ ! -d "json" ]; then
        git clone https://github.com/nlohmann/json.git
    fi
    
    cd json
    mkdir -p build
    cd build
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
    make -j$(nproc)
    sudo make install
    sudo ldconfig
    
    cd ../../..
    echo "nlohmann-json installed manually"
}

# Function to check dependencies
check_dependencies() {
    echo "Checking dependencies..."
    
    # Check for cmake
    if ! command -v cmake &> /dev/null; then
        echo "Error: CMake not found. Please install CMake."
        exit 1
    fi
    
    # Check for libcurl
    if ! pkg-config --exists libcurl 2>/dev/null; then
        echo "Error: libcurl not found. Please install libcurl development package."
        echo "Ubuntu/Debian: sudo apt-get install libcurl4-openssl-dev"
        echo "CentOS/RHEL: sudo yum install libcurl-devel"
        exit 1
    fi
    
    # Check for nlohmann-json
    if ! pkg-config --exists nlohmann_json 2>/dev/null && \
       [ ! -f "/usr/include/nlohmann/json.hpp" ] && \
       [ ! -f "/usr/local/include/nlohmann/json.hpp" ]; then
        echo "nlohmann-json not found. Installing..."
        install_nlohmann_json
    else
        echo "nlohmann-json found"
    fi
}

# Function to build with different methods
build_with_method() {
    local method=$1
    local build_dir=$2
    
    echo "Building with method: $method"
    
    case $method in
        "standard")
            cmake .. -DCMAKE_BUILD_TYPE=Release
            make -j$(nproc)
            ;;
        "pkg-config")
            cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(pkg-config --variable=prefix nlohmann_json)
            make -j$(nproc)
            ;;
        "manual")
            cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/local
            make -j$(nproc)
            ;;
        "vcpkg")
            if [ -n "$VCPKG_ROOT" ]; then
                cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
            else
                echo "VCPKG_ROOT not set, falling back to standard method"
                cmake .. -DCMAKE_BUILD_TYPE=Release
            fi
            make -j$(nproc)
            ;;
    esac
}

# Main build process
echo "Starting build process..."

# Check dependencies
check_dependencies

# Create build directory
echo "Creating build directory..."
mkdir -p build
cd build

# Try different build methods
echo "Attempting build..."

# Method 1: Standard CMake
if build_with_method "standard" "build"; then
    echo "Build successful with standard method!"
elif build_with_method "pkg-config" "build"; then
    echo "Build successful with pkg-config method!"
elif build_with_method "manual" "build"; then
    echo "Build successful with manual method!"
elif build_with_method "vcpkg" "build"; then
    echo "Build successful with vcpkg method!"
else
    echo "All build methods failed!"
    echo ""
    echo "Please try the following solutions:"
    echo "1. Install nlohmann-json development package:"
    echo "   Ubuntu/Debian: sudo apt-get install nlohmann-json3-dev"
    echo "   CentOS/RHEL: sudo yum install nlohmann-json3-devel"
    echo ""
    echo "2. Install manually:"
    echo "   git clone https://github.com/nlohmann/json.git"
    echo "   cd json && mkdir build && cd build"
    echo "   cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local"
    echo "   make && sudo make install"
    echo ""
    echo "3. Use vcpkg:"
    echo "   git clone https://github.com/Microsoft/vcpkg.git"
    echo "   cd vcpkg && ./bootstrap-vcpkg.sh"
    echo "   ./vcpkg install nlohmann-json"
    echo "   export VCPKG_ROOT=\$(pwd)"
    echo "   export CMAKE_TOOLCHAIN_FILE=\$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
    exit 1
fi

echo ""
echo "Build completed successfully!"
echo "Executable: ./build/cpuminer"
echo ""
echo "Usage examples:"
echo "  ./build/cpuminer --help"
echo "  ./build/cpuminer --rpc-user bitquantum --rpc-password bitquantum123 --threads 4"
echo "  ./build/cpuminer --config ../config.conf"
echo ""
