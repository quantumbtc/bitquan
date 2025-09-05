#!/bin/bash

# Quick Fix for nlohmann-json Issue
# Bitquantum RandomQ CPU Miner

echo "Quick Fix for nlohmann-json Issue"
echo "================================="

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: CMakeLists.txt not found. Please run this script from the cpuminer directory."
    exit 1
fi

echo "Installing nlohmann-json..."

# Try different installation methods
if command -v apt-get &> /dev/null; then
    echo "Using apt-get to install nlohmann-json..."
    sudo apt-get update
    sudo apt-get install -y nlohmann-json3-dev
elif command -v yum &> /dev/null; then
    echo "Using yum to install nlohmann-json..."
    sudo yum install -y nlohmann-json3-devel
elif command -v pacman &> /dev/null; then
    echo "Using pacman to install nlohmann-json..."
    sudo pacman -S --noconfirm nlohmann-json
else
    echo "Package manager not found. Installing manually..."
    
    # Create deps directory
    mkdir -p deps
    cd deps
    
    # Clone and install nlohmann-json
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
fi

echo ""
echo "nlohmann-json installation completed!"
echo ""
echo "Now you can build CPUMiner:"
echo "  ./build.sh"
echo ""
echo "Or use the fixed build script:"
echo "  ./build_fix.sh"
echo ""
