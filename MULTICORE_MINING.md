# BitQuantum Multi-Core CPU Mining

This document describes the enhanced multi-core CPU mining capabilities added to BitQuantum's `generatetoaddress` command.

## Overview

The `generatetoaddress` command now supports multi-core CPU mining using the RandomQ post-quantum algorithm. This enhancement allows users to utilize multiple CPU cores simultaneously to increase mining performance and hash rates.

## Features

### 1. Multi-Threaded Mining
- **Automatic Thread Detection**: By default, the system automatically detects the number of available CPU cores
- **Configurable Thread Count**: Users can specify the exact number of threads to use
- **Thread Safety**: All mining operations are thread-safe with proper synchronization
- **Performance Optimization**: Optimized nonce distribution across threads to maximize efficiency

### 2. Enhanced RPC Interface
- **New `threads` Parameter**: Added optional parameter to control thread count
- **Mining Statistics**: Detailed statistics including hash rate, total hashes, and timing
- **Progress Reporting**: Real-time logging of mining progress and performance

### 3. Backward Compatibility
- **Default Behavior**: Single-threaded mining when no thread count is specified
- **Existing Commands**: All existing RPC commands continue to work unchanged
- **Fallback Support**: Automatic fallback to single-threaded mining if multi-threading fails

## Usage

### Basic Syntax
```bash
bitquantum-cli generatetoaddress <nblocks> <address> [maxtries] [threads]
```

### Parameters
- `nblocks`: Number of blocks to generate (required)
- `address`: Target address for coinbase transactions (required)
- `maxtries`: Maximum hash attempts per block (optional, default: 1000000)
- `threads`: Number of CPU threads to use (optional, default: 0 = auto-detect)

### Examples

#### 1. Single-threaded mining (default)
```bash
bitquantum-cli generatetoaddress 1 btq1qrv7jvtvjwhe33qvpqrns9pllwhzz8ytdafra6l
```

#### 2. Multi-threaded mining with auto-detection
```bash
bitquantum-cli generatetoaddress 1 btq1qrv7jvtvjwhe33qvpqrns9pllwhzz8ytdafra6l 1000000 0
```

#### 3. Multi-threaded mining with 4 threads
```bash
bitquantum-cli generatetoaddress 1 btq1qrv7jvtvjwhe33qvpqrns9pllwhzz8ytdafra6l 1000000 4
```

#### 4. High-performance mining with 8 threads
```bash
bitquantum-cli generatetoaddress 5 btq1qrv7jvtvjwhe33qvpqrns9pllwhzz8ytdafra6l 2000000 8
```

## Response Format

The enhanced `generatetoaddress` command now returns detailed mining statistics:

```json
{
  "blocks": [
    "0000000000000000000000000000000000000000000000000000000000000000"
  ],
  "total_hashes": 1500000,
  "total_time": 45.67,
  "average_hashrate": 32850.5,
  "threads_used": 4
}
```

### Response Fields
- `blocks`: Array of generated block hashes
- `total_hashes`: Total number of hashes computed during mining
- `total_time`: Total mining time in seconds
- `average_hashrate`: Average hash rate in hashes per second
- `threads_used`: Number of CPU threads actually used

## Performance Considerations

### Thread Count Guidelines
- **1-2 threads**: Suitable for low-power systems or background mining
- **4-8 threads**: Optimal for most desktop and server systems
- **8+ threads**: High-performance systems with many CPU cores
- **Auto-detection (0)**: Recommended for most users

### System Requirements
- **CPU**: Multi-core processor recommended
- **Memory**: Additional memory per thread (minimal overhead)
- **Cooling**: Adequate cooling for sustained multi-core usage

### Performance Tips
1. **Monitor CPU Temperature**: Ensure adequate cooling for multi-core mining
2. **Adjust Thread Count**: Find the optimal thread count for your system
3. **Consider Power Usage**: More threads = higher power consumption
4. **Test Performance**: Use the provided test script to benchmark your system

## Implementation Details

### Multi-Threading Architecture
- **Thread Pool**: Dynamic thread creation and management
- **Work Distribution**: Nonce ranges distributed evenly across threads
- **Synchronization**: Atomic operations for thread-safe coordination
- **Early Termination**: Threads stop immediately when a solution is found

### RandomQ Algorithm Integration
- **Optimized Hashing**: Efficient RandomQ hash calculation per thread
- **Nonce Management**: Proper nonce space partitioning
- **Target Verification**: Thread-safe proof-of-work verification
- **Memory Efficiency**: Minimal memory overhead per thread

### Error Handling
- **Thread Safety**: All operations are thread-safe
- **Graceful Degradation**: Fallback to single-threaded mining on errors
- **Resource Management**: Proper cleanup of thread resources
- **Validation**: Comprehensive input validation and error reporting

## Testing

### Test Script
A test script is provided to verify multi-core mining functionality:

```bash
./test_multicore_mining.sh
```

This script tests:
- Single-threaded mining
- Multi-threaded mining with auto-detection
- Multi-threaded mining with specific thread counts
- Performance comparison between single and multi-threaded mining

### Manual Testing
1. **Basic Functionality**: Test with different thread counts
2. **Performance Testing**: Compare hash rates across different configurations
3. **Error Handling**: Test with invalid parameters
4. **Resource Usage**: Monitor CPU and memory usage during mining

## Troubleshooting

### Common Issues
1. **High CPU Usage**: Normal for multi-threaded mining
2. **System Overheating**: Reduce thread count or improve cooling
3. **Memory Usage**: Monitor for memory leaks (should be minimal)
4. **Thread Creation Errors**: Check system limits and permissions

### Performance Issues
1. **Low Hash Rate**: Verify thread count and CPU performance
2. **System Instability**: Reduce thread count or check cooling
3. **Memory Problems**: Monitor memory usage and system resources

### Debug Information
- **Logging**: Detailed mining statistics in debug logs
- **Thread Information**: Thread count and performance metrics
- **Error Messages**: Clear error reporting for troubleshooting

## Future Enhancements

### Planned Features
- **GPU Mining Support**: CUDA/OpenCL acceleration
- **Advanced Statistics**: More detailed performance metrics
- **Mining Pools**: Integration with mining pool protocols
- **Power Management**: Dynamic thread scaling based on system load

### Configuration Options
- **Thread Affinity**: CPU core binding for optimal performance
- **Priority Control**: Mining thread priority management
- **Resource Limits**: Configurable memory and CPU limits
- **Monitoring**: Real-time performance monitoring and alerts

## Conclusion

The multi-core CPU mining enhancement significantly improves BitQuantum's mining capabilities by utilizing modern multi-core processors effectively. Users can now achieve higher hash rates and faster block generation while maintaining the security and integrity of the RandomQ post-quantum algorithm.

For questions or support, please refer to the BitQuantum documentation or community forums.
