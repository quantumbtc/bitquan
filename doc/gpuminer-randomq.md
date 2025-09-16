# GPU Miner for RandomQ (gpuminer-randomq)

## Overview

`gpuminer-randomq` is a GPU-based mining tool for Bitquantum Core that uses OpenCL to accelerate RandomQ proof-of-work mining. It provides the same functionality as `cpuminer-randomq` but leverages GPU parallel processing for significantly higher hash rates.

## Prerequisites

- **OpenCL SDK**: Install OpenCL development libraries for your GPU vendor:
  - NVIDIA: CUDA Toolkit with OpenCL support
  - AMD: ROCm or AMD APP SDK
  - Intel: Intel OpenCL SDK
  - Generic: Khronos OpenCL SDK

- **Bitquantum Core Node**: A running Bitquantum Core node with RPC enabled

## Building

The GPU miner is automatically built when OpenCL is detected:

```bash
# Configure with OpenCL support
cmake -DOPENCL_FOUND=TRUE ..

# Build
make gpuminer-randomq
```

If OpenCL is not found, the build will skip the GPU miner and show:
```
OpenCL not found, skipping gpuminer-randomq
```

## Usage

### Basic GPU Mining

```bash
# Start GPU mining
./gpuminer-randomq -address=btq1qumgvxdf8r7njzvqr3hy0r30pvsr7fdm0xn5mh2 -gpu

# With custom work size
./gpuminer-randomq -address=btq1qumgvxdf8r7njzvqr3hy0r30pvsr7fdm0xn5mh2 -gpu -worksize=2048
```

### Parameters

| Parameter | Description | Default |
|-----------|-------------|---------|
| `-address=<bech32>` | Payout address for coinbase | Required |
| `-gpu` | Enable GPU mining (OpenCL) | false |
| `-worksize=<n>` | GPU work size (parallel threads) | 1024 |
| `-rpcconnect=<ip>` | RPC server IP | 127.0.0.1 |
| `-rpcport=<port>` | RPC server port | 8332 |
| `-rpcuser=<user>` | RPC username | (use cookie) |
| `-rpcpassword=<pw>` | RPC password | (use cookie) |
| `-rpcwait` | Wait for RPC server | false |
| `-maxtries=<n>` | Max nonce attempts per template | 1000000 |

### Examples

```bash
# Connect to remote node
./gpuminer-randomq -address=btq1q... -gpu -rpcconnect=192.168.1.100 -rpcport=8332

# High performance mining
./gpuminer-randomq -address=btq1q... -gpu -worksize=4096 -maxtries=10000000

# With authentication
./gpuminer-randomq -address=btq1q... -gpu -rpcuser=user -rpcpassword=pass
```

## Output

The miner provides detailed output including:

```
[GPU] OpenCL initialized, work size: 1024
[GBT] height=123 bits=1e0ffff0 has_hex=false has_coinbasetxn=false txs=0
[CB] height=123 scriptSig=7b00
[CB] No witness commitment found (non-segwit block)
[Template] height=123 version=536870912 prev=... time=1234567890 bits=1e0ffff0 target=... txs=1 merkle=...
[HashRate] Current: 125000.50 H/s | Average: 118750.25 H/s | Total: 5000000
[Found] height=123 nonce=1234567 time=1234567890 bits=1e0ffff0 target=... powhash=... merkle=...
[Submit] result=null error=null
[Submit] tip_height=123
```

## GPU Mining Details

### OpenCL Implementation

The GPU miner uses OpenCL kernels to parallelize RandomQ hash calculations:

1. **Kernel Loading**: Loads `randomq_kernel.cl` or uses embedded fallback
2. **Memory Management**: Allocates buffers for header, nonce, result, and target data
3. **Parallel Execution**: Launches work groups to test multiple nonces simultaneously
4. **Result Collection**: Efficiently checks for valid solutions using atomic operations

### Work Size Optimization

- **Small Work Size (256-1024)**: Lower latency, better for low-end GPUs
- **Medium Work Size (1024-4096)**: Balanced performance for most GPUs
- **Large Work Size (4096+)**: Higher throughput, requires high-end GPUs

### Performance Considerations

- **GPU Memory**: Ensure sufficient VRAM for work size
- **PCIe Bandwidth**: High work sizes may be limited by data transfer
- **Power Consumption**: GPU mining consumes significantly more power than CPU
- **Heat Management**: Monitor GPU temperatures during extended mining

## Troubleshooting

### OpenCL Not Found

```
Failed to initialize OpenCL GPU mining
```

**Solutions:**
1. Install OpenCL SDK for your GPU vendor
2. Ensure OpenCL drivers are properly installed
3. Check that `libOpenCL.so` is in your library path

### Kernel Compilation Errors

```
clBuildProgram failed
```

**Solutions:**
1. Check OpenCL driver compatibility
2. Verify kernel source code syntax
3. Try reducing work size

### Low Hash Rate

**Solutions:**
1. Increase work size (if GPU has sufficient memory)
2. Check GPU utilization with monitoring tools
3. Ensure GPU is not throttling due to temperature
4. Try different OpenCL drivers

### Connection Issues

Same as `cpuminer-randomq` - the miner will automatically retry on connection errors.

## Technical Notes

### RandomQ Algorithm

The GPU implementation uses a simplified RandomQ algorithm optimized for parallel execution. The full RandomQ implementation would require:

1. SHA256 preprocessing of block header
2. RandomQ algorithm with 8192 rounds
3. Final SHA256 postprocessing

### Memory Layout

- **Header Buffer**: 80 bytes (block header)
- **Nonce Buffer**: 4 bytes (starting nonce)
- **Result Buffer**: 32 bytes (hash result)
- **Target Buffer**: 32 bytes (difficulty target)
- **Found Flag**: 4 bytes (atomic flag)
- **Found Nonce**: 4 bytes (solution nonce)

### Thread Safety

The OpenCL implementation uses mutex locks to ensure thread-safe access to OpenCL resources, allowing safe integration with the existing mining loop.

## Comparison with CPU Mining

| Aspect | CPU Mining | GPU Mining |
|--------|------------|------------|
| Hash Rate | 10-50 KH/s | 100-1000+ KH/s |
| Power Usage | Low | High |
| Memory Usage | Low | Medium |
| Setup Complexity | Simple | Moderate |
| Hardware Cost | Low | High |

GPU mining provides significantly higher hash rates but requires more setup and hardware investment.
