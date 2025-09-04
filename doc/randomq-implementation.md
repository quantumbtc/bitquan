# RandomQ Post-Quantum Algorithm Implementation

This document describes the RandomQ post-quantum algorithm implemented in Bitquantum Core, replacing the traditional SHA256D proof-of-work algorithm.

## Overview

RandomQ is an improved variant of a Monero-inspired post-quantum algorithm, implementing the following hashing pipeline:

```
Block header -> SHA256 -> RandomQ -> SHA256 -> Final hash
```

This design provides post-quantum resistance while staying compatible with the existing Bitquantum protocol.

## Algorithm Features

### 1. Post-quantum resistance
- Based on lattice cryptography and randomized algorithms
- Resists quantum attacks such as Shor's and Grover's
- Uses a 200-byte internal state and 8192 rounds

### 2. Compatibility
- No changes to the Bitquantum protocol
- Preserves the existing block header structure
- Compatible with existing mining software interfaces

### 3. Performance
- Optimized C++ implementation
- Multi-threaded mining support
- Configurable number of rounds

## Implementation Architecture

### Core components

1. **CRandomQ class** (`src/crypto/randomq.h/cpp`)
   - Core implementation of the RandomQ algorithm
   - Manages internal state and round function
   - Supports seed initialization and nonce configuration

2. **CRandomQHash class** (`src/crypto/randomq_hash.h`)
   - Implements the full SHA256 -> RandomQ -> SHA256 pipeline
   - Provides a standardized hashing interface
   - Supports streaming input

3. **RandomQMining namespace** (`src/crypto/randomq_mining.h/cpp`)
   - Mining utilities
   - POW verification and nonce search
   - Optimized hashing routines

### Modified files

- `src/primitives/block.cpp` – update block header hash
- `src/rpc/mining.cpp` – update mining logic
- `src/test/util/mining.cpp` – update test mining code
- `src/bitquantum-util.cpp` – update mining tool
- `src/crypto/CMakeLists.txt` – add new sources

## Usage

### Basic hashing

```cpp
#include <crypto/randomq_hash.h>

CRandomQHash hasher;
hasher.Write(input_data);
hasher.SetRandomQNonce(nonce_value);
hasher.SetRandomQRounds(8192);

uint256 result;
hasher.Finalize(result.begin());
```

### Mining verification

```cpp
#include <crypto/randomq_mining.h>

bool valid = RandomQMining::CheckRandomQProofOfWork(
    block_header, 
    nBits, 
    powLimit
);
```

### Find a valid nonce

```cpp
bool found = RandomQMining::FindRandomQNonce(
    block_header, 
    nBits, 
    powLimit, 
    max_attempts
);
```

## Configuration

### RandomQ parameters

- **Rounds**: default 8192, adjustable via `SetRandomQRounds()`
- **Internal state**: 200 bytes (25 x 64-bit words)
- **Seed**: optional initialization seed
- **Nonce**: 32-bit random value for mining

### Performance tuning

- Fewer rounds: faster but less secure
- More rounds: more secure but slower
- Recommended 8192+ rounds in production

## Security Considerations

### Post-quantum properties

1. **Lattice-based**: hard mathematical problems, quantum-resistant
2. **Randomization**: randomized state transitions increase attack complexity
3. **Large state space**: 200-byte state provides sufficient entropy

### Attack mitigations

- **Precomputation**: mitigated via random seed and nonce
- **Collision**: dual SHA256 provides additional protection
- **Quantum**: specifically designed to resist quantum attacks

## Testing

Run RandomQ tests:

```bash
make check
# Or run a specific test
src/test/test_bitquantum --run_test=randomq_tests
```

Coverage:
- Basic hashing
- Mining verification
- Consistency checks
- Performance optimizations

## Benchmarks

### Hash throughput
- SHA256D: ~1000 MB/s (reference)
- RandomQ: ~10–50 MB/s (depends on rounds)

### Mining efficiency
- Requires more compute than SHA256D
- Provides significantly improved post-quantum security

## Future work

1. **Hardware acceleration**: ASIC/FPGA implementations
2. **Parameter optimization**: tune rounds/state size per security needs
3. **Parallelization**: further multi-threading optimizations
4. **Standardization**: promote a standard RandomQ spec

## Contributing

Contributions are welcome. Please:

1. Follow existing code style
2. Add appropriate tests
3. Update related documentation
4. Pass all test suites

## License

This project is under the MIT license. See COPYING for details.
