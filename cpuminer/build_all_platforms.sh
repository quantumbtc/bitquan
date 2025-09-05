#!/bin/bash

# CPUMiner Multi-Platform Build Script
# Bitquantum RandomQ CPU Miner

set -e

echo "Building CPUMiner for All Platforms - Bitquantum RandomQ CPU Miner"
echo "=================================================================="

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: CMakeLists.txt not found. Please run this script from the cpuminer directory."
    exit 1
fi

# Function to build for Linux
build_linux() {
    echo ""
    echo "Building for Linux..."
    echo "===================="
    
    # Create Linux build directory
    mkdir -p build_linux
    cd build_linux
    
    # Configure with CMake
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=17 \
        -DCMAKE_CXX_FLAGS="-O3 -march=native -mtune=native"
    
    # Build
    make -j$(nproc)
    
    echo "Linux build completed: ./build_linux/cpuminer"
    cd ..
}

# Function to build for Windows (MinGW)
build_windows_mingw() {
    echo ""
    echo "Building for Windows (MinGW)..."
    echo "==============================="
    
    # Check if MinGW is available
    if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
        echo "Warning: MinGW-w64 not found. Skipping Windows build."
        echo "Install MinGW-w64 to build Windows executable."
        return 1
    fi
    
    # Create Windows build directory
    mkdir -p build_windows_mingw
    cd build_windows_mingw
    
    # Configure with CMake for MinGW
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=17 \
        -DCMAKE_SYSTEM_NAME=Windows \
        -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
        -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
        -DCMAKE_CXX_FLAGS="-O3 -march=native -mtune=native -static-libgcc -static-libstdc++"
    
    # Build
    make -j$(nproc)
    
    echo "Windows (MinGW) build completed: ./build_windows_mingw/cpuminer.exe"
    cd ..
}

# Function to create packages
create_packages() {
    echo ""
    echo "Creating packages..."
    echo "==================="
    
    # Create packages directory
    mkdir -p packages
    
    # Linux package
    if [ -f "build_linux/cpuminer" ]; then
        echo "Creating Linux package..."
        mkdir -p packages/cpuminer_linux
        cp build_linux/cpuminer packages/cpuminer_linux/
        cp config.conf packages/cpuminer_linux/
        cp README.md packages/cpuminer_linux/
        cp build.sh packages/cpuminer_linux/
        chmod +x packages/cpuminer_linux/build.sh
        chmod +x packages/cpuminer_linux/cpuminer
        
        # Create tar.gz
        cd packages
        tar -czf cpuminer_linux.tar.gz cpuminer_linux/
        cd ..
        
        echo "Linux package created: packages/cpuminer_linux.tar.gz"
    fi
    
    # Windows package
    if [ -f "build_windows_mingw/cpuminer.exe" ]; then
        echo "Creating Windows package..."
        mkdir -p packages/cpuminer_windows
        cp build_windows_mingw/cpuminer.exe packages/cpuminer_windows/
        cp config.conf packages/cpuminer_windows/
        cp README.md packages/cpuminer_windows/
        cp README_Windows.md packages/cpuminer_windows/
        cp build_windows.bat packages/cpuminer_windows/
        cp build_windows_mingw.bat packages/cpuminer_windows/
        cp build_windows_vcpkg.bat packages/cpuminer_windows/
        cp install_dependencies_windows.bat packages/cpuminer_windows/
        cp create_windows_package.bat packages/cpuminer_windows/
        
        # Create run scripts
        echo '#!/bin/bash' > packages/cpuminer_windows/run_cpuminer.sh
        echo 'echo "Starting CPUMiner - Bitquantum RandomQ CPU Miner"' >> packages/cpuminer_windows/run_cpuminer.sh
        echo 'echo "================================================"' >> packages/cpuminer_windows/run_cpuminer.sh
        echo 'echo ""' >> packages/cpuminer_windows/run_cpuminer.sh
        echo 'echo "Usage examples:"' >> packages/cpuminer_windows/run_cpuminer.sh
        echo 'echo "  cpuminer.exe --help"' >> packages/cpuminer_windows/run_cpuminer.sh
        echo 'echo "  cpuminer.exe --rpc-user bitquantum --rpc-password bitquantum123 --threads 4"' >> packages/cpuminer_windows/run_cpuminer.sh
        echo 'echo "  cpuminer.exe --config config.conf"' >> packages/cpuminer_windows/run_cpuminer.sh
        echo 'echo ""' >> packages/cpuminer_windows/run_cpuminer.sh
        echo 'pause' >> packages/cpuminer_windows/run_cpuminer.sh
        
        # Create ZIP
        cd packages
        zip -r cpuminer_windows.zip cpuminer_windows/
        cd ..
        
        echo "Windows package created: packages/cpuminer_windows.zip"
    fi
}

# Main build process
echo "Starting multi-platform build process..."

# Build for Linux
build_linux

# Build for Windows (MinGW)
build_windows_mingw

# Create packages
create_packages

echo ""
echo "Multi-platform build completed successfully!"
echo "==========================================="
echo ""
echo "Build outputs:"
if [ -f "build_linux/cpuminer" ]; then
    echo "  Linux: ./build_linux/cpuminer"
fi
if [ -f "build_windows_mingw/cpuminer.exe" ]; then
    echo "  Windows: ./build_windows_mingw/cpuminer.exe"
fi
echo ""
echo "Packages:"
if [ -f "packages/cpuminer_linux.tar.gz" ]; then
    echo "  Linux: packages/cpuminer_linux.tar.gz"
fi
if [ -f "packages/cpuminer_windows.zip" ]; then
    echo "  Windows: packages/cpuminer_windows.zip"
fi
echo ""
echo "Usage examples:"
echo "  Linux:   ./build_linux/cpuminer --help"
echo "  Windows: ./build_windows_mingw/cpuminer.exe --help"
echo ""
