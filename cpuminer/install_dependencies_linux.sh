#!/bin/bash

# CPUMiner Linux Dependencies Installation Script
# Bitquantum RandomQ CPU Miner

set -e

echo "Installing CPUMiner Dependencies for Linux"
echo "=========================================="

# Detect Linux distribution
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$NAME
    VER=$VERSION_ID
else
    echo "Cannot detect Linux distribution"
    exit 1
fi

echo "Detected OS: $OS $VER"
echo ""

# Function to install dependencies on Ubuntu/Debian
install_ubuntu_debian() {
    echo "Installing dependencies for Ubuntu/Debian..."
    
    # Update package list
    sudo apt-get update
    
    # Install build tools
    sudo apt-get install -y build-essential cmake pkg-config
    
    # Install libcurl
    sudo apt-get install -y libcurl4-openssl-dev
    
    # Install nlohmann-json
    sudo apt-get install -y nlohmann-json3-dev
    
    # Install additional development tools
    sudo apt-get install -y git wget curl
    
    echo "Dependencies installed successfully!"
}

# Function to install dependencies on CentOS/RHEL/Fedora
install_centos_rhel() {
    echo "Installing dependencies for CentOS/RHEL/Fedora..."
    
    # Install build tools
    sudo yum groupinstall -y "Development Tools"
    sudo yum install -y cmake pkg-config
    
    # Install libcurl
    sudo yum install -y libcurl-devel
    
    # Install nlohmann-json
    sudo yum install -y nlohmann-json3-devel
    
    # Install additional development tools
    sudo yum install -y git wget curl
    
    echo "Dependencies installed successfully!"
}

# Function to install dependencies on Arch Linux
install_arch() {
    echo "Installing dependencies for Arch Linux..."
    
    # Update package database
    sudo pacman -Sy
    
    # Install build tools
    sudo pacman -S --noconfirm base-devel cmake pkg-config
    
    # Install libcurl
    sudo pacman -S --noconfirm curl
    
    # Install nlohmann-json
    sudo pacman -S --noconfirm nlohmann-json
    
    # Install additional development tools
    sudo pacman -S --noconfirm git wget
    
    echo "Dependencies installed successfully!"
}

# Function to install dependencies manually
install_manual() {
    echo "Installing dependencies manually..."
    
    # Create dependencies directory
    mkdir -p deps
    cd deps
    
    # Install nlohmann-json manually
    echo "Installing nlohmann-json manually..."
    if [ ! -d "json" ]; then
        git clone https://github.com/nlohmann/json.git
    fi
    cd json
    mkdir -p build
    cd build
    cmake ..
    make -j$(nproc)
    sudo make install
    cd ../..
    
    # Install libcurl if not available
    if ! pkg-config --exists libcurl; then
        echo "Installing libcurl manually..."
        wget https://curl.se/download/curl-8.5.0.tar.gz
        tar -xzf curl-8.5.0.tar.gz
        cd curl-8.5.0
        ./configure --prefix=/usr/local
        make -j$(nproc)
        sudo make install
        cd ..
    fi
    
    cd ..
    echo "Dependencies installed successfully!"
}

# Function to install dependencies using vcpkg
install_vcpkg() {
    echo "Installing dependencies using vcpkg..."
    
    # Install vcpkg
    if [ ! -d "vcpkg" ]; then
        git clone https://github.com/Microsoft/vcpkg.git
    fi
    cd vcpkg
    ./bootstrap-vcpkg.sh
    
    # Install dependencies
    ./vcpkg install curl nlohmann-json
    
    # Set environment variable
    echo 'export VCPKG_ROOT="'$(pwd)'"' >> ~/.bashrc
    echo 'export CMAKE_TOOLCHAIN_FILE="'$(pwd)'/scripts/buildsystems/vcpkg.cmake"' >> ~/.bashrc
    
    cd ..
    echo "Dependencies installed successfully!"
    echo "Please run: source ~/.bashrc"
}

# Main installation logic
case "$OS" in
    *"Ubuntu"*|*"Debian"*)
        install_ubuntu_debian
        ;;
    *"CentOS"*|*"Red Hat"*|*"Fedora"*)
        install_centos_rhel
        ;;
    *"Arch"*|*"Manjaro"*)
        install_arch
        ;;
    *)
        echo "Unsupported Linux distribution: $OS"
        echo "Please install dependencies manually:"
        echo "1. libcurl development package"
        echo "2. nlohmann-json development package"
        echo "3. cmake, pkg-config, build-essential"
        echo ""
        echo "Or try manual installation:"
        install_manual
        ;;
esac

echo ""
echo "Dependency installation completed!"
echo ""
echo "You can now build CPUMiner:"
echo "  ./build.sh"
echo ""
echo "Or for all platforms:"
echo "  ./build_all_platforms.sh"
echo ""
