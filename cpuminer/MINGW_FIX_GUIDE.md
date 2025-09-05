# MinGW交叉编译问题修复指南

本指南帮助解决MinGW交叉编译环境中的头文件路径问题。

## 🚀 快速修复

### 方法1：一键修复（推荐）
```bash
chmod +x fix_mingw.sh
./fix_mingw.sh
```

### 方法2：使用修复版构建脚本
```bash
chmod +x build_mingw_fixed.sh
./build_mingw_fixed.sh
```

### 方法3：手动修复
```bash
cp CMakeLists_mingw.txt CMakeLists.txt
```

## 📋 问题分析

错误信息：
```
/usr/include/stdint.h:26:10: fatal error: bits/libc-header-start.h: No such file or directory
/usr/include/wchar.h:27:10: fatal error: bits/libc-header-start.h: No such file or directory
```

**原因：** MinGW交叉编译器在Linux环境中找不到Windows头文件，导致编译失败。

## 🛠️ 解决方案

### 方案1：使用修复版CMakeLists.txt

```bash
# 备份原始文件
cp CMakeLists.txt CMakeLists_backup.txt

# 使用MinGW专用版本
cp CMakeLists_mingw.txt CMakeLists.txt

# 构建
mkdir build_mingw && cd build_mingw
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### 方案2：使用修复版构建脚本

```bash
# 给脚本执行权限
chmod +x build_mingw_fixed.sh

# 运行构建脚本
./build_mingw_fixed.sh
```

### 方案3：使用工具链文件

```bash
# 创建构建目录
mkdir build_mingw && cd build_mingw

# 使用工具链文件
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchain-mingw.cmake

# 构建
make -j$(nproc)
```

## 🔧 修复内容

修复后的CMakeLists.txt包含以下改进：

1. **MinGW检测** - 自动检测MinGW交叉编译器
2. **Windows标志** - 添加Windows特定的编译标志
3. **头文件路径** - 修复头文件路径问题
4. **静态链接** - 使用静态链接生成独立可执行文件

## 📊 修复的文件

| 文件 | 修复内容 |
|------|----------|
| `CMakeLists.txt` | 添加MinGW检测和修复 |
| `CMakeLists_mingw.txt` | MinGW专用CMake文件 |
| `cmake/Toolchain-mingw.cmake` | MinGW工具链文件 |
| `fix_mingw.sh` | 一键修复脚本 |
| `build_mingw_fixed.sh` | 修复版构建脚本 |

## 🎯 推荐使用流程

1. **首先尝试**：`./fix_mingw.sh`
2. **如果失败**：使用`./build_mingw_fixed.sh`
3. **如果仍有问题**：使用工具链文件

## 🐛 常见问题

### 问题1：MinGW未安装
```bash
# Ubuntu/Debian
sudo apt-get install mingw-w64

# CentOS/RHEL
sudo yum install mingw64-gcc

# Arch Linux
sudo pacman -S mingw-w64-gcc
```

### 问题2：权限不足
```bash
# 解决方案：给脚本执行权限
chmod +x fix_mingw.sh
chmod +x build_mingw_fixed.sh
```

### 问题3：依赖库问题
```bash
# 安装MinGW依赖库
sudo apt-get install libcurl4-openssl-dev
sudo apt-get install nlohmann-json3-dev
```

### 问题4：工具链路径问题
```bash
# 检查MinGW安装路径
which x86_64-w64-mingw32-gcc

# 检查工具链文件
ls -la cmake/Toolchain-mingw.cmake
```

## 📁 文件说明

| 文件 | 用途 |
|------|------|
| `CMakeLists.txt` | 原始CMake文件（有MinGW问题） |
| `CMakeLists_mingw.txt` | MinGW专用CMake文件 |
| `cmake/Toolchain-mingw.cmake` | MinGW工具链文件 |
| `fix_mingw.sh` | 一键修复脚本 |
| `build_mingw_fixed.sh` | 修复版构建脚本 |
| `CMakeLists_backup.txt` | 原始文件备份 |

## 🎯 验证修复

修复后，运行以下命令验证：

```bash
# 检查MinGW编译器
x86_64-w64-mingw32-gcc --version

# 尝试构建
mkdir build_mingw && cd build_mingw
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

# 如果成功，继续构建
make -j$(nproc)
```

## 📞 支持

如果问题仍然存在：

1. 检查MinGW安装：`x86_64-w64-mingw32-gcc --version`
2. 检查工具链文件：`cat cmake/Toolchain-mingw.cmake`
3. 检查编译标志：`cmake .. --debug-output`
4. 查看详细错误：`make VERBOSE=1`

## 📄 许可证

本修复指南遵循MIT许可证。
