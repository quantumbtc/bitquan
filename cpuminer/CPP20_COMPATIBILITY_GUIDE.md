# C++20兼容性指南

本指南帮助解决CPUMiner中的C++20兼容性问题。

## 🚨 问题描述

错误信息：
```
error: 'span' in namespace 'std' does not name a template type
error: 'consteval' does not name a type
```

**原因：** 代码使用了C++20特性，但编译器设置为C++17标准。

## 🚀 快速修复

### 方法1：使用修复脚本（推荐）
```bash
chmod +x fix_cpp20.sh
./fix_cpp20.sh
```

### 方法2：手动修复
```bash
# 清理构建目录
rm -rf build

# 创建构建目录
mkdir build && cd build

# 配置CMake使用C++20
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20

# 构建
make -j$(nproc)
```

## 📋 编译器要求

### 支持的编译器版本

| 编译器 | 最低版本 | 推荐版本 |
|--------|----------|----------|
| GCC | 10.0+ | 11.0+ |
| Clang | 10.0+ | 12.0+ |
| MSVC | 2019 16.11+ | 2022 17.0+ |

### 检查编译器版本

```bash
# GCC
g++ --version

# Clang
clang++ --version

# MSVC
cl
```

## 🔧 修复内容

### 1. CMakeLists.txt更新
- 设置C++标准为C++20
- 启用C++20特性支持
- 更新编译器标志

### 2. 构建脚本更新
- build.sh - 添加C++20支持
- build_windows.sh - 添加C++20支持
- CMakePresets.json - 更新预设配置

### 3. 文档更新
- README.md - 更新编译器要求
- README_CMAKE.md - 更新依赖要求

## 🎯 C++20特性使用

### 主要使用的C++20特性

1. **std::span** - 用于安全的数组视图
2. **consteval** - 编译时求值函数
3. **std::string_view** - 字符串视图
4. **std::byte** - 字节类型
5. **概念(Concepts)** - 类型约束

### 示例代码

```cpp
// 使用std::span
std::string HexStr(const std::span<const uint8_t> s);

// 使用consteval
consteval ConstevalFormatString(const char* str);

// 使用std::string_view
std::string EncodeBase64(std::string_view str);
```

## 🐛 常见问题

### 问题1：编译器版本过低
```bash
# Ubuntu/Debian - 安装GCC 10+
sudo apt-get install gcc-10 g++-10

# 设置默认编译器
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-10 100
```

### 问题2：CMake版本过低
```bash
# 检查CMake版本
cmake --version

# 升级CMake
sudo apt-get install cmake
```

### 问题3：头文件缺失
```bash
# 安装C++20开发包
sudo apt-get install build-essential
sudo apt-get install libstdc++-10-dev
```

### 问题4：MinGW版本过低
```bash
# 检查MinGW版本
x86_64-w64-mingw32-gcc --version

# 升级MinGW
sudo apt-get install mingw-w64
```

## 📊 性能影响

### C++20特性优势

1. **std::span** - 零开销的数组视图
2. **consteval** - 编译时优化
3. **std::string_view** - 避免不必要的字符串拷贝
4. **概念** - 更好的错误信息

### 编译时间

- C++20编译时间可能比C++17稍长
- 但运行时性能通常更好
- 内存使用更高效

## 🎯 最佳实践

1. **使用现代编译器** - 推荐GCC 11+或Clang 12+
2. **启用所有警告** - 使用-Wall -Wextra
3. **使用静态分析** - 启用-fanalyzer
4. **测试兼容性** - 在多个编译器上测试

## 📞 支持

如果问题仍然存在：

1. 检查编译器版本是否满足要求
2. 检查CMake版本是否满足要求
3. 使用`--verbose`选项查看详细输出
4. 查看编译器错误日志

## 📄 许可证

本兼容性指南遵循MIT许可证。
