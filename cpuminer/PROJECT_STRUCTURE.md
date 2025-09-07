# CPUMiner 项目结构

## 📁 清理后的项目结构（仅Linux）

### 核心源代码文件
```
cpuminer/
├── main.cpp                    # 主程序入口
├── miner.cpp                   # 挖矿器主类实现
├── miner.h                     # 挖矿器主类头文件
├── randomq_miner.cpp           # RandomQ挖矿算法实现
├── randomq_miner.h             # RandomQ挖矿算法头文件
├── rpc_client.cpp              # RPC客户端实现
├── rpc_client.h                # RPC客户端头文件
├── config.cpp                  # 配置管理实现
└── config.h                    # 配置管理头文件
```

### 构建配置文件
```
├── CMakeLists.txt              # 主CMake配置文件（仅Linux）
└── CMakePresets.json           # CMake预设配置
```

### 构建脚本
```
├── build.sh                    # Linux构建脚本
└── install_dependencies_linux.sh    # Linux依赖安装脚本
```

### 配置文件
```
├── config.conf                 # 示例配置文件
└── README.md                   # 项目说明文档
```

### 文档文件
```
├── README.md                   # 主项目说明
└── README_CMAKE.md            # CMake构建指南
```

## 🗑️ 已删除的Windows文件

### Windows构建脚本
- `build_windows.bat` - Windows批处理构建脚本
- `build_windows.sh` - Windows Shell构建脚本
- `build_windows_enhanced.bat` - Windows增强构建脚本
- `build_windows_portable.bat` - Windows便携版构建脚本
- `install_dependencies_windows.bat` - Windows依赖安装脚本
- `cleanup_invalid_files.bat` - 清理无效文件脚本

### Windows工具链文件
- `cmake/Toolchain-mingw.cmake` - MinGW工具链
- `cmake/Toolchain-mingw-linux.cmake` - MinGW Linux工具链
- `vcpkg/` - vcpkg包管理器目录
- `vcpkg.json` - vcpkg包管理配置

### Windows构建目录
- `build_windows/` - Windows构建输出目录

## 📋 保留文件说明

### 核心文件（必须保留）
- **源代码文件**：所有 `.cpp` 和 `.h` 文件
- **CMakeLists.txt**：主构建配置（仅Linux）
- **README.md**：项目说明

### 构建文件（推荐保留）
- **build.sh**：Linux构建脚本
- **CMakePresets.json**：CMake预设配置
- **install_dependencies_linux.sh**：Linux依赖安装

### 可选文件
- **config.conf**：示例配置文件
- **README_CMAKE.md**：详细构建指南

## 🚀 使用建议

### 快速开始
```bash
# Linux
./build.sh
```

### 安装依赖
```bash
# 安装Linux依赖
./install_dependencies_linux.sh
```

### 手动构建
```bash
# 创建构建目录
mkdir build && cd build

# 配置CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# 构建
make -j$(nproc)
```

## 📊 文件统计

| 类型 | 数量 | 说明 |
|------|------|------|
| 源代码文件 | 8 | 核心功能实现 |
| 构建脚本 | 2 | Linux构建支持 |
| 配置文件 | 3 | 项目配置 |
| 文档文件 | 2 | 项目说明 |

**总计**：15个核心文件（仅Linux）

## ✅ 清理完成

项目结构现在更加简洁，专注于Linux平台：
- 移除了所有Windows相关文件
- 简化了CMakeLists.txt配置
- 保留了核心源代码和Linux构建支持
- 项目现在只支持Linux平台

## 🔧 构建要求

### Linux系统要求
- Ubuntu 18.04+ / Debian 10+ / CentOS 7+
- GCC 10+ 或 Clang 10+
- CMake 3.16+
- libcurl4-openssl-dev
- nlohmann-json3-dev

### 安装依赖
```bash
# Ubuntu/Debian
sudo apt-get install build-essential cmake libcurl4-openssl-dev nlohmann-json3-dev

# CentOS/RHEL
sudo yum install gcc-c++ cmake libcurl-devel nlohmann-json3-devel
```