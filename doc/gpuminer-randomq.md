# GPU RandomQ Miner (NVIDIA CUDA)

基于NVIDIA CUDA的高性能RandomQ GPU挖矿程序，专为Bitquantum区块链设计。

## 特性

- **NVIDIA GPU优化**: 专门针对NVIDIA显卡优化，使用CUDA并行计算
- **完整RandomQ实现**: SHA256 → RandomQ → SHA256 完整挖矿流程
- **多GPU支持**: 支持选择和管理多个NVIDIA GPU设备
- **RPC集成**: 与bitquantumd节点无缝集成，自动获取区块模板
- **实时监控**: 实时显示哈希率和挖矿状态
- **自动重试**: 网络中断自动重连，模板过期自动刷新

## 系统要求

### 硬件要求
- **NVIDIA GPU**: 计算能力3.0或更高 (GTX 600系列及以上)
- **显存**: 至少1GB显存
- **推荐GPU**: RTX 30/40系列, GTX 1060及以上

### 软件要求
- **CUDA Toolkit**: 版本11.0或更高
- **NVIDIA驱动**: 支持CUDA的最新驱动
- **操作系统**: Windows 10/11, Linux (Ubuntu 18.04+)

## 编译安装

### 1. 安装CUDA Toolkit

#### Windows
1. 从NVIDIA官网下载CUDA Toolkit
2. 运行安装程序，选择完整安装
3. 重启系统

#### Linux (Ubuntu)
```bash
# 添加NVIDIA包仓库
wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2004/x86_64/cuda-ubuntu2004.pin
sudo mv cuda-ubuntu2004.pin /etc/apt/preferences.d/cuda-repository-pin-600
wget https://developer.download.nvidia.com/compute/cuda/12.3.0/local_installers/cuda-repo-ubuntu2004-12-3-local_12.3.0-545.23.06-1_amd64.deb
sudo dpkg -i cuda-repo-ubuntu2004-12-3-local_12.3.0-545.23.06-1_amd64.deb
sudo cp /var/cuda-repo-ubuntu2004-12-3-local/cuda-*-keyring.gpg /usr/share/keyrings/
sudo apt-get update
sudo apt-get -y install cuda
```

### 2. 编译Bitquantum

```bash
# 配置CMake（确保检测到CUDA）
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# 编译
cmake --build build --target gpuminer-randomq

# 检查是否成功编译
ls build/src/gpuminer-randomq
```

## 使用方法

### 基本命令

```bash
# 查看可用GPU设备
./gpuminer-randomq -list-gpus

# 开始挖矿
./gpuminer-randomq -address=<your_btq_address>

# 指定GPU设备
./gpuminer-randomq -address=<your_btq_address> -gpu=0

# 自定义RPC连接
./gpuminer-randomq -address=<your_btq_address> -rpcconnect=192.168.1.100 -rpcport=8332
```

### 完整参数列表

#### 必需参数
- `-address=<bech32>` - 挖矿收益地址（必需）

#### RPC连接参数
- `-rpcconnect=<ip>` - RPC服务器IP（默认：127.0.0.1）
- `-rpcport=<port>` - RPC端口（默认：8332）
- `-rpcuser=<user>` - RPC用户名
- `-rpcpassword=<pw>` - RPC密码
- `-rpccookiefile=<loc>` - RPC cookie文件路径
- `-rpcwait` - 等待RPC服务器就绪
- `-rpcwaittimeout=<n>` - RPC等待超时（秒）

#### GPU参数
- `-gpu=<n>` - 使用的GPU设备ID（默认：0）
- `-gputhreads=<n>` - 每个CUDA块的线程数（默认：256）
- `-gpublocks=<n>` - CUDA网格中的块数（默认：自动）
- `-list-gpus` - 列出可用GPU设备并退出

#### 挖矿参数
- `-maxtries=<n>` - 刷新模板前的最大nonce尝试次数（默认：1000000）

### 使用示例

#### 1. 基本挖矿
```bash
# 使用默认GPU挖矿
./gpuminer-randomq -address=btq1qxy2kgdygjrsqtzq2n0yrf2493p83kkfjhx0wlh
```

#### 2. 多GPU系统
```bash
# 查看可用GPU
./gpuminer-randomq -list-gpus

# 输出示例：
# [GPU] Found 2 CUDA device(s)
# [GPU] Device 0: NVIDIA GeForce RTX 3080
#       Memory: 10240.0 MB
#       Compute Capability: 8.6
#       Available: Yes
# [GPU] Device 1: NVIDIA GeForce GTX 1660
#       Memory: 6144.0 MB  
#       Compute Capability: 7.5
#       Available: Yes

# 使用第二个GPU
./gpuminer-randomq -address=btq1qxy2kgdygjrsqtzq2n0yrf2493p83kkfjhx0wlh -gpu=1
```

#### 3. 远程节点挖矿
```bash
# 连接到远程节点
./gpuminer-randomq \
  -address=btq1qxy2kgdygjrsqtzq2n0yrf2493p83kkfjhx0wlh \
  -rpcconnect=192.168.1.100 \
  -rpcport=8332 \
  -rpcuser=miner \
  -rpcpassword=password123
```

#### 4. 性能调优
```bash
# 调整GPU参数以获得最佳性能
./gpuminer-randomq \
  -address=btq1qxy2kgdygjrsqtzq2n0yrf2493p83kkfjhx0wlh \
  -gpu=0 \
  -gputhreads=512 \
  -gpublocks=2048
```

## 性能优化

### 1. GPU参数调优

#### 线程数调优
- **低端GPU** (GTX 1060): `-gputhreads=128`
- **中端GPU** (RTX 3060): `-gputhreads=256` 
- **高端GPU** (RTX 4090): `-gputhreads=512`

#### 块数调优
- 通常设为 `GPU核心数 × 2-4`
- RTX 3080: `-gpublocks=2048`
- RTX 4090: `-gpublocks=4096`

### 2. 系统优化

#### Windows
- 设置高性能电源计划
- 关闭Windows Update自动重启
- 增加虚拟内存大小

#### Linux
- 设置GPU性能模式：`nvidia-smi -pm 1`
- 调整GPU功耗限制：`nvidia-smi -pl 300` (300W)
- 设置GPU时钟：`nvidia-smi -ac 5001,1400`

### 3. 监控工具

#### 使用nvidia-smi监控
```bash
# 实时监控GPU状态
watch -n 1 nvidia-smi

# 查看详细信息
nvidia-smi -q -d PERFORMANCE,POWER,TEMPERATURE
```

## 故障排除

### 常见问题

#### 1. CUDA not found
**错误**: `CUDA not found, skipping NVIDIA GPU miner`
**解决**: 
- 安装CUDA Toolkit
- 检查PATH环境变量
- 重新编译项目

#### 2. No CUDA devices found
**错误**: `No CUDA devices found`
**解决**:
- 检查NVIDIA驱动是否正确安装
- 运行 `nvidia-smi` 确认GPU可见
- 检查GPU是否被其他程序占用

#### 3. GPU device not available
**错误**: `GPU device 0 not available`
**解决**:
- 使用 `-list-gpus` 查看可用设备
- 确认GPU计算能力≥3.0
- 尝试使用其他GPU设备ID

#### 4. RPC connection failed
**错误**: `RPC connection failed`
**解决**:
- 确认bitquantumd正在运行
- 检查RPC端口和认证信息
- 确认防火墙设置

#### 5. 内存不足
**错误**: `CUDA error: out of memory`
**解决**:
- 减少 `-gpublocks` 参数
- 关闭其他GPU程序
- 升级到更大显存的GPU

### 调试模式

启用详细日志：
```bash
# 设置环境变量启用CUDA调试
export CUDA_LAUNCH_BLOCKING=1
export CUDA_DEVICE_ORDER=PCI_BUS_ID

./gpuminer-randomq -address=<address> -gpu=0
```

## 性能基准

### 预期哈希率 (H/s)

| GPU型号 | 计算能力 | 预期哈希率 | 功耗 |
|---------|----------|------------|------|
| GTX 1060 6GB | 6.1 | 50-80 H/s | 120W |
| GTX 1070 | 6.1 | 80-120 H/s | 150W |
| GTX 1080 Ti | 6.1 | 150-200 H/s | 250W |
| RTX 2070 | 7.5 | 120-180 H/s | 175W |
| RTX 3060 | 8.6 | 100-150 H/s | 170W |
| RTX 3070 | 8.6 | 180-250 H/s | 220W |
| RTX 3080 | 8.6 | 300-400 H/s | 320W |
| RTX 3090 | 8.6 | 400-500 H/s | 350W |
| RTX 4070 | 8.9 | 200-300 H/s | 200W |
| RTX 4080 | 8.9 | 400-550 H/s | 320W |
| RTX 4090 | 8.9 | 600-800 H/s | 450W |

*注：实际性能可能因系统配置、温度、电源等因素而异*

## 安全注意事项

1. **电源供应**: 确保电源功率足够，建议预留20%余量
2. **散热**: 监控GPU温度，保持在80°C以下
3. **过载保护**: 避免长时间100%负载运行
4. **定期维护**: 清理灰尘，检查风扇运行状态
5. **备份钱包**: 确保挖矿地址对应的钱包已备份

## 技术支持

- **问题报告**: 在项目GitHub页面提交issue
- **社区论坛**: 加入Bitquantum官方社区
- **文档更新**: 定期查看最新版本文档

---

*本文档适用于gpuminer-randomq v1.0及以上版本*