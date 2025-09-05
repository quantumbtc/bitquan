# MinGW Linux交叉编译问题修复指南

本指南帮助解决在Linux环境中使用MinGW进行Windows交叉编译的问题。

## 🚀 快速修复

### 方法1：一键修复（推荐）
```bash
chmod +x fix_mingw_linux.sh
./fix_mingw_linux.sh
```

### 方法2：使用简化构建脚本
```bash
chmod +x build_mingw_simple.sh
./build_mingw_simple.sh
```

### 方法3：使用完整构建脚本
```bash
chmod +x build_mingw_linux.sh
./build_mingw_linux.sh
```

## 📋 问题分析

错误信息：
```
CMake Error: Could not create named generator MinGW Makefiles
```

**原因：** "MinGW Makefiles"生成器只在Windows上可用，在Linux环境中无法使用。

## 🛠️ 解决方案

### 方案1：使用修复版CMakeLists.txt

```bash
# 备份原始文件
cp CMakeLists.txt CMakeLists_backup.txt

# 使用MinGW Linux专用版本
cp CMakeLists_mingw_linux.txt CMakeLists.txt

# 构建
mkdir build_mingw && cd build_mingw
cmake .. -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++
make -j$(nproc)
```

### 方案2：使用简化构建脚本

```bash
# 给脚本执行权限
chmod +x build_mingw_simple.sh

# 运行构建脚本
./build_mingw_simple.sh
```

### 方案3：使用完整构建脚本

```bash
# 给脚本执行权限
chmod +x build_mingw_linux.sh

# 运行构建脚本
./build_mingw_linux.sh
```

## 🔧 修复内容

修复后的CMakeLists.txt包含以下改进：

1. **交叉编译设置** - 正确设置MinGW交叉编译环境
2. **编译器指定** - 明确指定MinGW编译器
3. **系统名称** - 设置目标系统为Windows
4. **编译标志** - 添加Windows特定的编译标志

## 📊 修复的文件

| 文件 | 修复内容 |
|------|----------|
| `CMakeLists.txt` | 原始CMake文件（有MinGW问题） |
| `CMakeLists_mingw_linux.txt` | MinGW Linux专用CMake文件 |
| `fix_mingw_linux.sh` | 一键修复脚本 |
| `build_mingw_simple.sh` | 简化构建脚本 |
| `build_mingw_linux.sh` | 完整构建脚本 |
| `CMakeLists_backup.txt` | 原始文件备份 |

## 🎯 推荐使用流程

1. **首先尝试**：`./fix_mingw_linux.sh`
2. **如果失败**：使用`./build_mingw_simple.sh`
3. **如果仍有问题**：使用`./build_mingw_linux.sh`

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
chmod +x fix_mingw_linux.sh
chmod +x build_mingw_simple.sh
chmod +x build_mingw_linux.sh
```

### 问题3：依赖库问题
```bash
# 安装依赖库
sudo apt-get install libcurl4-openssl-dev
sudo apt-get install nlohmann-json3-dev
```

### 问题4：编译器路径问题
```bash
# 检查MinGW编译器
which x86_64-w64-mingw32-gcc
which x86_64-w64-mingw32-g++

# 检查编译器版本
x86_64-w64-mingw32-gcc --version
```

## 📁 文件说明

| 文件 | 用途 |
|------|------|
| `CMakeLists.txt` | 原始CMake文件（有MinGW问题） |
| `CMakeLists_mingw_linux.txt` | MinGW Linux专用CMake文件 |
| `fix_mingw_linux.sh` | 一键修复脚本 |
| `build_mingw_simple.sh` | 简化构建脚本 |
| `build_mingw_linux.sh` | 完整构建脚本 |
| `CMakeLists_backup.txt` | 原始文件备份 |

## 🎯 验证修复

修复后，运行以下命令验证：

```bash
# 检查MinGW编译器
x86_64-w64-mingw32-gcc --version

# 尝试构建
mkdir build_mingw && cd build_mingw
cmake .. -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++

# 如果成功，继续构建
make -j$(nproc)
```

## 📞 支持

如果问题仍然存在：

1. 检查MinGW安装：`x86_64-w64-mingw32-gcc --version`
2. 检查CMake版本：`cmake --version`
3. 检查编译标志：`cmake .. --debug-output`
4. 查看详细错误：`make VERBOSE=1`

## 📄 许可证

本修复指南遵循MIT许可证。
