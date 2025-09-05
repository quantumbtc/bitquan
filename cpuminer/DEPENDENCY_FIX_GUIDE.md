# 依赖问题修复指南 - nlohmann-json

本指南帮助解决CMake找不到nlohmann-json库的问题。

## 🚀 快速修复

### 方法1：一键修复（推荐）
```bash
chmod +x quick_fix.sh
./quick_fix.sh
```

### 方法2：使用修复版构建脚本
```bash
chmod +x build_fix.sh
./build_fix.sh
```

### 方法3：使用header-only版本
```bash
cp CMakeLists_header_only.txt CMakeLists.txt
./build.sh
```

## 📋 问题分析

错误信息：
```
CMake Error at CMakeLists.txt:28 (find_package):
  By not providing "Findnlohmann_json.cmake" in CMAKE_MODULE_PATH this
  project has asked CMake to find a package configuration file provided by
  "nlohmann_json", but CMake did not find one.
```

**原因：** CMake无法找到nlohmann-json的配置文件。

## 🛠️ 解决方案

### 方案1：安装开发包（推荐）

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install nlohmann-json3-dev
```

#### CentOS/RHEL/Fedora
```bash
sudo yum install nlohmann-json3-devel
```

#### Arch Linux
```bash
sudo pacman -S nlohmann-json
```

### 方案2：手动安装

```bash
# 创建依赖目录
mkdir -p deps
cd deps

# 克隆nlohmann-json
git clone https://github.com/nlohmann/json.git
cd json

# 构建和安装
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc)
sudo make install
sudo ldconfig

# 返回项目目录
cd ../../..
```

### 方案3：使用vcpkg

```bash
# 安装vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh

# 安装依赖
./vcpkg install nlohmann-json

# 设置环境变量
export VCPKG_ROOT=$(pwd)
export CMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

# 返回项目目录
cd ..

# 构建项目
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
make -j$(nproc)
```

### 方案4：使用header-only版本

```bash
# 备份原始CMakeLists.txt
cp CMakeLists.txt CMakeLists_original.txt

# 使用header-only版本
cp CMakeLists_header_only.txt CMakeLists.txt

# 构建
./build.sh
```

## 🔧 修复后的CMakeLists.txt特性

修复后的CMakeLists.txt包含以下改进：

1. **多方法检测** - 尝试多种方式找到nlohmann-json
2. **pkg-config支持** - 使用pkg-config检测库
3. **header-only支持** - 支持纯头文件版本
4. **自动安装提示** - 提供详细的安装指导
5. **兼容性** - 支持多种Linux发行版

## 📊 检测方法

### 检查nlohmann-json是否已安装

```bash
# 方法1：检查pkg-config
pkg-config --exists nlohmann_json && echo "Found via pkg-config"

# 方法2：检查头文件
ls /usr/include/nlohmann/json.hpp 2>/dev/null && echo "Found header file"

# 方法3：检查库文件
find /usr -name "libnlohmann_json*" 2>/dev/null && echo "Found library"
```

### 检查CMake是否能找到库

```bash
# 创建测试目录
mkdir test_cmake && cd test_cmake

# 创建测试CMakeLists.txt
cat > CMakeLists.txt << EOF
cmake_minimum_required(VERSION 3.16)
project(test)
find_package(nlohmann_json REQUIRED)
message(STATUS "nlohmann-json found: \${nlohmann_json_FOUND}")
EOF

# 测试CMake
cmake .
cd .. && rm -rf test_cmake
```

## 🐛 常见问题

### 问题1：权限不足
```bash
# 解决方案：使用sudo
sudo apt-get install nlohmann-json3-dev
```

### 问题2：包名不同
```bash
# 检查可用的包名
apt search nlohmann
yum search nlohmann
pacman -Ss nlohmann
```

### 问题3：版本不兼容
```bash
# 检查CMake版本
cmake --version

# 检查nlohmann-json版本
pkg-config --modversion nlohmann_json
```

### 问题4：路径问题
```bash
# 设置CMAKE_PREFIX_PATH
export CMAKE_PREFIX_PATH=/usr/local:$CMAKE_PREFIX_PATH

# 或者使用cmake参数
cmake .. -DCMAKE_PREFIX_PATH=/usr/local
```

## 📁 文件说明

| 文件 | 用途 |
|------|------|
| `CMakeLists.txt` | 修复后的主CMake文件 |
| `CMakeLists_header_only.txt` | 使用header-only版本的CMake文件 |
| `quick_fix.sh` | 一键修复脚本 |
| `build_fix.sh` | 修复版构建脚本 |
| `install_dependencies_linux.sh` | Linux依赖安装脚本 |

## 🎯 推荐流程

1. **首先尝试**：`./quick_fix.sh`
2. **如果失败**：手动安装开发包
3. **如果仍有问题**：使用header-only版本
4. **最后选择**：使用vcpkg

## 📞 支持

如果问题仍然存在：

1. 检查系统信息：`uname -a`
2. 检查包管理器：`which apt-get yum pacman`
3. 检查CMake版本：`cmake --version`
4. 查看详细错误：`cmake .. --debug-output`

## 📄 许可证

本修复指南遵循MIT许可证。
