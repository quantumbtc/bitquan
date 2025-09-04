# Bitquantum Loop Mining Feature

## Overview

We have extended the `bitquantum-cli` `-generate` command to support loop/continuous mining. This allows you to continuously generate blocks until manually stopped.

## Features

### 1. Loop mining syntax
```bash
# Basic syntax
bitquantum-cli -generate loop [nblocks] [maxtries]

# Examples
bitquantum-cli -generate loop 1 1000    # generate 1 block per iteration, max 1000 tries
bitquantum-cli -generate loop 2 500     # generate 2 blocks per iteration, max 500 tries
bitquantum-cli -generate loop           # use defaults: 1 block per iteration
```

### 2. Parameters
- `loop`: enable loop mining mode
- `nblocks`: number of blocks per iteration (optional, default 1)
- `maxtries`: max tries per block (optional, default 1000000)

### 3. Usage

#### Linux/Mac
```bash
# Start loop mining
bitquantum-cli -generate loop 1 1000

# Or use the test script
chmod +x test_mining_loop.sh
./test_mining_loop.sh
```

#### Windows
```cmd
# Start loop mining
bitquantum-cli -generate loop 1 1000

# Or use the batch file
test_mining_loop.bat
```

## Capabilities

### 1. Automatic looping
- Mining automatically iterates
- Starts the next iteration immediately after each success
- No need to re-run commands manually

### 2. Real-time feedback
- Shows iteration progress
- Shows number of blocks generated
- Shows mining address info

### 3. Error handling
- Automatically handles mining errors
- Stops and reports errors on failure
- Graceful exit support (Ctrl+C)

### 4. Performance
- Small delay between iterations to avoid overload
- Uses existing RPCs for compatibility

## Use cases

### 1. Development testing
- Quickly generate test blocks
- Verify blockchain functionality
- Test mining rewards

### 2. Private networks
- Maintain a private chain
- Continuously generate blocks
- Keep the network active

### 3. Education and demos
- Demonstrate mining
- Show chain growth
- Teach blockchain concepts

## Notes

### 1. System requirements
- Ensure the node is running
- Ensure the wallet is unlocked
- Ensure sufficient compute resources

### 2. Safety considerations
- Loop mining continuously consumes resources
- Recommended for test environments
- Use with caution in production

### 3. Stopping
- Press `Ctrl+C` to stop
- Mining stops after the current block completes
- Does not interrupt an in-flight attempt

## Technical details

### 1. Modified files
- `src/bitquantum-cli.cpp`: primary changes
- Added loop mining logic
- Updated argument parsing

### 2. New functionality
- Loop mining detection
- Automatic parameter adjustment
- Continuous mining loop

### 3. Compatibility
- Preserves original `-generate` behavior
- Backward compatible with scripts
- No impact on other CLI features

## Troubleshooting

### 1. Common issues
- **Connection failure**: ensure the node is running
- **Permission error**: ensure the wallet is unlocked
- **Argument error**: check command syntax

### 2. Debugging
- Use `bitquantum-cli getblockchaininfo` to check node state
- Use `bitquantum-cli getwalletinfo` to check wallet state
- Check console output for errors

## Changelog

### v1.0.0
- Added loop mining
- Support custom block counts and max tries
- Added error handling and feedback
- Created test script and docs

## Contributing

If you find issues or have suggestions:
1. Check existing issues
2. Create a new issue
3. Submit improvements

## License

This project is licensed under MIT. See LICENSE.
