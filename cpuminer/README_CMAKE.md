# CPUMiner CMake构建指南

本指南介绍如何使用CMake构建Bitquantum RandomQ CPU Miner。

## 🚀 快速开始

### Linux构建
```bash
# 基本构建
./build.sh

# 调试构建
./build.sh --debug

# 清理构建
./build.sh --clean

# 详细输出
./build.sh --verbose
```

### Windows构建
```bash
# MinGW交叉编译
./build_windows.sh

# MSVC构建（需要Visual Studio）
./build_windows.sh --msvc

# 调试构建
./build_windows.sh --debug
```

## 📋 依赖要求

### Linux
- CMake 3.16+
- GCC 7+ 或 Clang 5+
- libcurl开发包
- nlohmann-json开发包

### Windows (MinGW交叉编译)
- MinGW-w64
- CMake 3.16+
- libcurl开发包
- nlohmann-json开发包

### Windows (MSVC)
- Visual Studio 2019/2022
- CMake 3.16+
- vcpkg (推荐)

## 🔧 构建选项

### 构建脚本选项

#### build.sh
- `-d, --debug`: 调试构建
- `-c, --clean`: 清理构建目录
- `-j, --jobs N`: 并行作业数
- `-v, --verbose`: 详细输出
- `-h, --help`: 帮助信息

#### build_windows.sh
- `-d, --debug`: 调试构建
- `-c, --clean`: 清理构建目录
- `-j, --jobs N`: 并行作业数
- `-v, --verbose`: 详细输出
- `--msvc`: 使用MSVC编译器
- `--mingw`: 使用MinGW编译器（默认）
- `-h, --help`: 帮助信息

### CMake预设

使用CMake预设可以简化构建过程：

```bash
# 配置预设
cmake --preset default
cmake --preset debug
cmake --preset windows-mingw
cmake --preset windows-msvc

# 构建预设
cmake --build --preset default
cmake --build --preset debug
cmake --build --preset windows-mingw
cmake --build --preset windows-msvc
```

## 📁 项目文件结构

```
cpuminer/
├── CMakeLists.txt                 # CMake构建配置
├── CMakePresets.json              # CMake预设配置
├── build.sh                       # Linux构建脚本
├── build_windows.sh               # Windows构建脚本
├── build_windows.bat              # Windows批处理构建脚本
├── install_dependencies_linux.sh  # Linux依赖安装脚本
├── install_dependencies_windows.bat # Windows依赖安装脚本
├── main.cpp                       # 主程序入口
├── miner.h/cpp                    # 主挖矿类
├── randomq_miner.h/cpp            # RandomQ挖矿逻辑
├── rpc_client.h/cpp               # RPC通信
├── config.h/cpp                   # 配置管理
├── config.conf                    # 默认配置文件
├── cmake/                         # CMake工具链文件
├── README.md                      # 项目说明文档
└── README_CMAKE.md                # CMake构建指南
```

## 📁 构建目录结构

```
cpuminer/
├── build/                         # Linux构建目录
├── build_debug/                   # Linux调试构建目录
├── build_windows_mingw/           # Windows MinGW构建目录
├── build_windows_msvc/            # Windows MSVC构建目录
└── ...
```

## 🐛 常见问题

### 问题1：依赖库未找到
```bash
# Ubuntu/Debian
sudo apt-get install libcurl4-openssl-dev nlohmann-json3-dev

# CentOS/RHEL
sudo yum install libcurl-devel nlohmann-json3-devel

# Arch Linux
sudo pacman -S curl nlohmann-json
```

### 问题2：MinGW交叉编译问题
```bash
# 安装MinGW-w64
sudo apt-get install mingw-w64

# 检查MinGW编译器
x86_64-w64-mingw32-gcc --version
```

### 问题3：CMake版本过低
```bash
# 检查CMake版本
cmake --version

# 升级CMake
sudo apt-get install cmake
```

### 问题4：权限问题
```bash
# 给脚本执行权限
chmod +x build.sh
chmod +x build_windows.sh
```

## 📊 构建配置

### 默认配置
- 构建类型：Release
- 编译器优化：-O3
- 架构优化：-march=native -mtune=native
- 警告级别：-Wall -Wextra

### 调试配置
- 构建类型：Debug
- 调试信息：-g
- 优化级别：-O0
- 断言：启用

### Windows配置
- 系统：Windows
- 架构：x86_64
- 静态链接：-static-libgcc -static-libstdc++
- Windows API：-D_WIN32_WINNT=0x0601

## 🎯 最佳实践

1. **使用构建脚本**：推荐使用提供的构建脚本而不是直接使用CMake
2. **清理构建**：遇到问题时使用`--clean`选项
3. **并行构建**：使用`-j`选项指定并行作业数
4. **详细输出**：调试时使用`--verbose`选项
5. **预设配置**：使用CMake预设简化配置过程

## 📞 支持

如果遇到问题：

1. 检查依赖库是否安装
2. 检查CMake版本是否满足要求
3. 使用`--verbose`选项查看详细输出
4. 使用`--clean`选项清理构建目录
5. 查看CMake错误日志

## 📄 许可证

本构建系统遵循MIT许可证。
