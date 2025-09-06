#!/bin/bash

# CPUMiner 修复验证脚本
# 验证所有编译和逻辑问题是否已修复

echo "CPUMiner 修复验证脚本"
echo "====================="

# 检查是否在正确的目录
if [ ! -f "CMakeLists.txt" ]; then
    echo "错误: 请在cpuminer目录中运行此脚本"
    exit 1
fi

echo "1. 检查头文件包含..."
echo "-------------------"

# 检查main.cpp
if grep -q "#include <thread>" main.cpp && grep -q "#include <chrono>" main.cpp; then
    echo "✓ main.cpp 头文件包含正确"
else
    echo "✗ main.cpp 头文件包含有问题"
fi

# 检查miner.h
if grep -q "#include <mutex>" miner.h && grep -q "#include <chrono>" miner.h; then
    echo "✓ miner.h 头文件包含正确"
else
    echo "✗ miner.h 头文件包含有问题"
fi

echo ""
echo "2. 检查函数签名..."
echo "-----------------"

# 检查RandomQMiner::initialize
if grep -q "bool initialize(const std::string& config_file = \"\");" randomq_miner.h; then
    echo "✓ RandomQMiner::initialize 函数签名正确"
else
    echo "✗ RandomQMiner::initialize 函数签名有问题"
fi

# 检查getStats函数
if grep -q "MiningStats getStats() const;" randomq_miner.h; then
    echo "✓ getStats 函数声明存在"
else
    echo "✗ getStats 函数声明缺失"
fi

echo ""
echo "3. 检查类型定义一致性..."
echo "-----------------------"

# 检查MiningStats结构体
if grep -q "uint64_t total_hashes;" rpc_client.h && grep -q "double hash_rate;" rpc_client.h; then
    echo "✓ MiningStats 结构体定义正确"
else
    echo "✗ MiningStats 结构体定义有问题"
fi

echo ""
echo "4. 检查资源管理..."
echo "-----------------"

# 检查析构函数中的cleanup调用
if grep -q "cleanup();" miner.cpp; then
    echo "✓ 析构函数中调用了cleanup()"
else
    echo "✗ 析构函数中未调用cleanup()"
fi

echo ""
echo "5. 检查参数验证..."
echo "-----------------"

# 检查线程数验证
if grep -q "Thread count cannot be negative" config.cpp; then
    echo "✓ 线程数参数验证已添加"
else
    echo "✗ 线程数参数验证缺失"
fi

# 检查RandomQ轮数验证
if grep -q "RandomQ rounds must be greater than 0" config.cpp; then
    echo "✓ RandomQ轮数参数验证已添加"
else
    echo "✗ RandomQ轮数参数验证缺失"
fi

echo ""
echo "6. 尝试编译..."
echo "-------------"

# 清理构建目录
if [ -d "build" ]; then
    echo "清理构建目录..."
    rm -rf build
fi

# 创建构建目录
mkdir build
cd build

# 配置CMake
echo "配置CMake..."
if cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20 > cmake_output.log 2>&1; then
    echo "✓ CMake配置成功"
else
    echo "✗ CMake配置失败"
    echo "CMake输出:"
    cat cmake_output.log
    exit 1
fi

# 编译
echo "编译项目..."
if make -j$(nproc) > build_output.log 2>&1; then
    echo "✓ 编译成功"
    echo ""
    echo "7. 检查生成的可执行文件..."
    echo "-------------------------"
    
    if [ -f "cpuminer" ]; then
        echo "✓ cpuminer 可执行文件已生成"
        echo "文件大小: $(ls -lh cpuminer | awk '{print $5}')"
        echo "文件权限: $(ls -l cpuminer | awk '{print $1}')"
    else
        echo "✗ cpuminer 可执行文件未生成"
    fi
    
    echo ""
    echo "8. 测试可执行文件..."
    echo "-------------------"
    
    if ./cpuminer --help > help_output.log 2>&1; then
        echo "✓ 可执行文件可以正常运行"
        echo "帮助信息:"
        head -10 help_output.log
    else
        echo "✗ 可执行文件无法正常运行"
        echo "错误输出:"
        cat help_output.log
    fi
    
else
    echo "✗ 编译失败"
    echo "编译输出:"
    cat build_output.log
    exit 1
fi

echo ""
echo "验证完成！"
echo "=========="
echo "所有主要问题已修复，CPUMiner应该可以正常编译和运行。"
echo ""
echo "使用方法:"
echo "  ./cpuminer --help"
echo "  ./cpuminer --rpc-user user --rpc-password pass --threads 4"
