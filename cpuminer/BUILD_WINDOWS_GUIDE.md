# Windows构建指南 - CPUMiner

本指南详细说明如何在Windows平台上构建CPUMiner挖矿工具。

## 🚀 快速开始

### 方法1：一键构建（推荐）
```cmd
build_windows_simple.bat
```

### 方法2：PowerShell构建
```powershell
.\build_windows.ps1
```

### 方法3：完整构建（包含打包）
```powershell
.\build_windows.ps1 -Package
```

## 📋 系统要求

### 必需软件
- **Windows 10/11** (64位)
- **Visual Studio 2022** (Community Edition免费)
- **CMake** (3.16或更高版本)
- **Git** (用于vcpkg)

### 依赖库
- **libcurl** - HTTP客户端库
- **nlohmann-json** - JSON处理库

## 🛠️ 构建方法

### 1. 简单构建 (build_windows_simple.bat)
最简单的构建方法，适合快速测试：

```cmd
build_windows_simple.bat
```

**特点：**
- 自动检测Visual Studio 2022
- 使用默认配置
- 生成Release版本
- 自动错误提示

### 2. PowerShell构建 (build_windows.ps1)
功能最全面的构建方法：

```powershell
# 基本构建
.\build_windows.ps1

# 调试版本
.\build_windows.ps1 -BuildType Debug

# 清理后构建
.\build_windows.ps1 -Clean

# 构建并打包
.\build_windows.ps1 -Package

# 查看帮助
.\build_windows.ps1 -Help
```

**特点：**
- 支持多种构建类型
- 自动清理功能
- 自动打包功能
- 详细的错误诊断

### 3. vcpkg构建 (build_windows_vcpkg.bat)
使用vcpkg管理依赖的构建方法：

```cmd
build_windows_vcpkg.bat
```

**特点：**
- 自动安装依赖
- 使用vcpkg工具链
- 适合开发环境

### 4. MinGW构建 (build_windows_mingw.bat)
使用MinGW-w64编译器的构建方法：

```cmd
build_windows_mingw.bat
```

**特点：**
- 不需要Visual Studio
- 生成静态链接的可执行文件
- 适合轻量级部署

## 📦 输出文件

### 可执行文件位置
- **Visual Studio构建**: `build\bin\Release\cpuminer.exe`
- **vcpkg构建**: `build_vcpkg\bin\Release\cpuminer.exe`
- **MinGW构建**: `build_mingw\cpuminer.exe`

### 打包文件
- **ZIP包**: `cpuminer_windows.zip`
- **包目录**: `cpuminer_windows_package\`

## 🔧 依赖安装

### 自动安装
```cmd
install_dependencies_windows.bat
```

### 手动安装

#### 使用vcpkg
```cmd
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
vcpkg install curl:x64-windows nlohmann-json:x64-windows
```

#### 手动下载
1. **libcurl**: https://curl.se/download.html
2. **nlohmann-json**: https://github.com/nlohmann/json/releases

## 🚀 使用方法

### 基本使用
```cmd
cpuminer.exe --rpc-user bitquantum --rpc-password bitquantum123 --threads 4
```

### 使用配置文件
```cmd
cpuminer.exe --config config.conf
```

### 查看帮助
```cmd
cpuminer.exe --help
```

## 🐛 故障排除

### 常见问题

#### 1. CMake未找到
```
Error: CMake not found
```
**解决方案：**
- 安装CMake并添加到PATH
- 重启命令提示符

#### 2. Visual Studio未找到
```
Error: Visual Studio not found
```
**解决方案：**
- 安装Visual Studio 2022
- 确保安装了C++工作负载

#### 3. 依赖库未找到
```
Error: Could not find libcurl
```
**解决方案：**
- 运行 `install_dependencies_windows.bat`
- 或手动安装依赖库

#### 4. 构建失败
```
Build failed
```
**解决方案：**
- 检查Visual Studio安装
- 尝试以管理员身份运行
- 检查杀毒软件是否干扰

### 调试构建
```powershell
.\build_windows.ps1 -BuildType Debug
```

### 清理构建
```powershell
.\build_windows.ps1 -Clean
```

## 📊 性能优化

### 编译器优化
- **Release模式**: 启用最高优化级别
- **全程序优化**: 启用LTCG优化
- **内联函数**: 启用函数内联

### CPU优化
- **AVX2支持**: 自动检测并启用
- **SSE4支持**: 自动检测并启用
- **原生优化**: 针对目标CPU优化

## 📁 项目结构

```
cpuminer/
├── build_windows_simple.bat      # 简单构建脚本
├── build_windows.ps1             # PowerShell构建脚本
├── build_windows_vcpkg.bat       # vcpkg构建脚本
├── build_windows_mingw.bat       # MinGW构建脚本
├── install_dependencies_windows.bat  # 依赖安装脚本
├── create_windows_package.bat    # 打包脚本
├── CMakeLists.txt                # CMake配置
├── vcpkg.json                    # vcpkg依赖配置
├── config.conf                   # 配置文件
├── README_Windows.md             # Windows文档
└── BUILD_WINDOWS_GUIDE.md        # 本文件
```

## 🎯 构建选项对比

| 方法 | 难度 | 依赖管理 | 输出 | 推荐度 |
|------|------|----------|------|--------|
| build_windows_simple.bat | ⭐ | 手动 | exe | ⭐⭐⭐⭐ |
| build_windows.ps1 | ⭐⭐ | 手动 | exe + 包 | ⭐⭐⭐⭐⭐ |
| build_windows_vcpkg.bat | ⭐⭐⭐ | 自动 | exe | ⭐⭐⭐ |
| build_windows_mingw.bat | ⭐⭐ | 手动 | 静态exe | ⭐⭐⭐ |

## 📞 支持

如果遇到问题：

1. 查看错误信息
2. 检查系统要求
3. 尝试不同的构建方法
4. 查看README_Windows.md
5. 提交GitHub Issue

## 📄 许可证

本项目采用MIT许可证。详见COPYING文件。
