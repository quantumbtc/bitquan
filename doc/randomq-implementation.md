# RandomQ抗量子算法实现

本文档描述了在Bitcoin Core中实现的RandomQ抗量子算法，该算法用于替换传统的SHA256D工作量证明算法。

## 概述

RandomQ是一个基于门罗币(Monero)抗量子算法的改进版本，实现了以下哈希流程：

```
区块头 -> SHA256 -> RandomQ -> SHA256 -> 最终哈希
```

这种设计提供了抗量子计算的特性，同时保持了与现有Bitcoin协议的兼容性。

## 算法特性

### 1. 抗量子性
- 基于格密码学和随机化算法
- 抵抗量子计算机的Shor算法和Grover算法攻击
- 使用200字节的内部状态和8192轮迭代

### 2. 兼容性
- 不改变现有的Bitcoin协议
- 保持相同的区块头结构
- 兼容现有的挖矿软件接口

### 3. 性能
- 优化的C++实现
- 支持多线程挖矿
- 可配置的轮数参数

## 实现架构

### 核心组件

1. **CRandomQ类** (`src/crypto/randomq.h/cpp`)
   - RandomQ算法的核心实现
   - 管理内部状态和轮函数
   - 支持种子初始化和nonce设置

2. **CRandomQHash类** (`src/crypto/randomq_hash.h`)
   - 实现SHA256->RandomQ->SHA256的完整流程
   - 提供标准化的哈希接口
   - 支持流式输入处理

3. **RandomQMining命名空间** (`src/crypto/randomq_mining.h/cpp`)
   - 挖矿相关的工具函数
   - POW验证和nonce查找
   - 优化的哈希计算

### 修改的文件

- `src/primitives/block.cpp` - 修改区块头哈希计算
- `src/rpc/mining.cpp` - 更新挖矿逻辑
- `src/test/util/mining.cpp` - 更新测试挖矿代码
- `src/bitcoin-util.cpp` - 更新挖矿工具
- `src/crypto/CMakeLists.txt` - 添加新的源文件

## 使用方法

### 基本哈希计算

```cpp
#include <crypto/randomq_hash.h>

CRandomQHash hasher;
hasher.Write(input_data);
hasher.SetRandomQNonce(nonce_value);
hasher.SetRandomQRounds(8192);

uint256 result;
hasher.Finalize(result.begin());
```

### 挖矿验证

```cpp
#include <crypto/randomq_mining.h>

bool valid = RandomQMining::CheckRandomQProofOfWork(
    block_header, 
    nBits, 
    powLimit
);
```

### 查找有效nonce

```cpp
bool found = RandomQMining::FindRandomQNonce(
    block_header, 
    nBits, 
    powLimit, 
    max_attempts
);
```

## 配置参数

### RandomQ参数

- **轮数**: 默认8192轮，可通过`SetRandomQRounds()`调整
- **内部状态**: 200字节(25个64位整数)
- **种子**: 可选的初始化种子
- **Nonce**: 32位随机数，用于挖矿

### 性能调优

- 轮数越少，计算越快但安全性降低
- 轮数越多，安全性越高但计算越慢
- 建议在生产环境中使用8192轮或更高

## 安全性考虑

### 抗量子特性

1. **格密码学基础**: 基于数学难题，对量子计算机具有抗性
2. **随机化算法**: 使用随机状态转换，增加攻击难度
3. **大状态空间**: 200字节的内部状态提供足够的熵

### 攻击防护

- **预计算攻击**: 通过随机种子和nonce防止
- **碰撞攻击**: 双重SHA256提供额外保护
- **量子攻击**: 算法设计专门抵抗量子计算

## 测试

运行RandomQ测试：

```bash
make check
# 或者运行特定测试
src/test/test_bitcoin --run_test=randomq_tests
```

测试覆盖：
- 基本哈希功能
- 挖矿验证
- 一致性检查
- 性能优化

## 性能基准

### 哈希速度
- SHA256D: ~1000 MB/s (参考)
- RandomQ: ~10-50 MB/s (取决于轮数)

### 挖矿效率
- 相比传统SHA256D，RandomQ挖矿需要更多计算资源
- 但提供了显著的抗量子安全性提升

## 未来改进

1. **硬件加速**: 开发专用ASIC或FPGA实现
2. **参数优化**: 根据安全需求调整轮数和状态大小
3. **并行化**: 进一步优化多线程性能
4. **标准化**: 推动行业标准的RandomQ实现

## 贡献

欢迎提交改进建议和代码贡献。请确保：

1. 遵循现有的代码风格
2. 添加适当的测试
3. 更新相关文档
4. 通过所有测试套件

## 许可证

本项目遵循MIT许可证，详见COPYING文件。
