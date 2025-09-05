# CPUMiner Windows Build Guide

This guide explains how to build CPUMiner for Windows platforms.

## Prerequisites

### Required Software

1. **Visual Studio 2022** (Community Edition is free)
   - Download from: https://visualstudio.microsoft.com/vs/community/
   - Select "Desktop development with C++" workload
   - Include Windows 10/11 SDK

2. **CMake** (3.16 or later)
   - Download from: https://cmake.org/download/
   - Add CMake to PATH during installation

3. **Git** (for vcpkg)
   - Download from: https://git-scm.com/download/win

### Dependencies

- **libcurl** - HTTP client library
- **nlohmann-json** - JSON library

## Installation Methods

### Method 1: vcpkg (Recommended)

vcpkg is Microsoft's C++ package manager and the easiest way to install dependencies.

#### Automatic Installation

1. Run the dependency installer:
   ```cmd
   install_dependencies_windows.bat
   ```
   Select option 1 for automatic vcpkg installation.

2. Build CPUMiner:
   ```cmd
   build_windows_vcpkg.bat
   ```

#### Manual vcpkg Installation

1. Clone vcpkg repository:
   ```cmd
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   ```

2. Bootstrap vcpkg:
   ```cmd
   .\bootstrap-vcpkg.bat
   ```

3. Install dependencies:
   ```cmd
   vcpkg install curl:x64-windows nlohmann-json:x64-windows
   ```

4. Set environment variable:
   ```cmd
   set VCPKG_ROOT=C:\path\to\vcpkg
   ```

5. Build CPUMiner:
   ```cmd
   build_windows_vcpkg.bat
   ```

### Method 2: Visual Studio Build

1. Install dependencies using vcpkg (see Method 1)

2. Build using Visual Studio:
   ```cmd
   build_windows.bat
   ```

### Method 3: MinGW Build

1. Install MinGW-w64:
   - Download from: https://www.mingw-w64.org/downloads/
   - Add MinGW to PATH

2. Install dependencies manually or use vcpkg

3. Build:
   ```cmd
   build_windows_mingw.bat
   ```

## Build Scripts

### build_windows.bat
- Uses Visual Studio 2022 generator
- Optimized Release build
- Automatic dependency copying

### build_windows_vcpkg.bat
- Uses vcpkg for dependency management
- Visual Studio 2022 generator
- Automatic dependency installation

### build_windows_mingw.bat
- Uses MinGW-w64 compiler
- Static linking for standalone executable
- No Visual Studio required

### build_windows_msbuild.bat
- Uses MSBuild directly
- Alternative to CMake build
- Requires Visual Studio

## Output

After successful build, the executable will be located at:
- **Visual Studio build**: `build_windows\bin\Release\cpuminer.exe`
- **vcpkg build**: `build_vcpkg\bin\Release\cpuminer.exe`
- **MinGW build**: `build_mingw\cpuminer.exe`

## Usage

### Basic Usage
```cmd
cpuminer.exe --rpc-user bitquantum --rpc-password bitquantum123 --threads 4
```

### Using Configuration File
```cmd
cpuminer.exe --config config.conf
```

### Help
```cmd
cpuminer.exe --help
```

## Troubleshooting

### Common Issues

1. **CMake not found**
   - Install CMake and add to PATH
   - Restart command prompt

2. **Visual Studio not found**
   - Install Visual Studio 2022
   - Ensure C++ workload is installed

3. **Dependencies not found**
   - Use vcpkg for automatic dependency management
   - Check VCPKG_ROOT environment variable

4. **Build errors**
   - Ensure all dependencies are installed
   - Check Visual Studio version compatibility
   - Try cleaning build directory

### Debug Build

For debugging, change build type to Debug:

```cmd
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

### Static Linking

For standalone executable (MinGW):

```cmd
cmake .. -DCMAKE_CXX_FLAGS="-static-libgcc -static-libstdc++"
```

## Performance Optimization

### Compiler Optimizations

The build scripts include the following optimizations:

- **Release mode**: `/O2` optimization
- **Inlining**: `/Oi` enable intrinsic functions
- **Frame pointer**: `/Oy` omit frame pointers
- **Whole program optimization**: `/GL` and `/LTCG`

### CPU Optimizations

- **AVX2 support**: Enabled by default
- **SSE4 support**: Enabled by default
- **Native optimization**: `-march=native` (MinGW)

## Distribution

### Standalone Executable

For distribution without dependencies:

1. Use MinGW build with static linking
2. Copy required DLLs (if any)
3. Include configuration file

### Installer

Create Windows installer using:

- **NSIS**: Nullsoft Scriptable Install System
- **WiX Toolset**: Microsoft's installer technology
- **Inno Setup**: Free installer creator

## Development

### IDE Setup

1. Open Visual Studio 2022
2. Open folder containing CMakeLists.txt
3. Visual Studio will automatically configure CMake
4. Build and debug directly in IDE

### Debugging

1. Set breakpoints in Visual Studio
2. Use Debug build configuration
3. Attach debugger to running process

## Support

For Windows-specific issues:

- Check Windows Event Viewer for system errors
- Use Dependency Walker to check DLL dependencies
- Verify Visual Studio redistributables are installed

## License

This project is licensed under the MIT License. See the COPYING file for details.
