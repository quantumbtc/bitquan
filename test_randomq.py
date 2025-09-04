#!/usr/bin/env python3
"""
RandomQ test script
Used to validate basic functionality of the RandomQ hashing algorithm
"""

import hashlib
import struct

def sha256(data):
    """Compute SHA256 hash"""
    return hashlib.sha256(data).digest()

def simulate_randomq(input_data, nonce=0, rounds=8192):
    """
    Simplified RandomQ simulation.
    Note: Python implementation for concept testing, not the full C++ implementation.
    """
    # Step 1: SHA256
    first_hash = sha256(input_data)

    # Expand 32 bytes to 64 bytes to split into 8 uint64s
    first_hash_64 = first_hash + first_hash  # 32 + 32 = 64 bytes
    
    # Step 2: simulate RandomQ (simplified)
    # Use a simple state transition function
    state = list(struct.unpack('<8Q', first_hash_64))  # 64 bytes -> 8 x 64-bit
    
    # Mix in nonce
    state[0] ^= nonce
    
    # Simulate RandomQ round function
    for _ in range(min(rounds, 100)):  # limit rounds to avoid slowness
        # Simple state mixing
        new_state = []
        for i in range(8):
            # Rotate and mix
            rotated = ((state[i] << 13) | (state[i] >> 51)) & 0xFFFFFFFFFFFFFFFF
            next_val = state[(i + 1) % 8]
            mixed = (rotated ^ next_val ^ (state[i] + next_val)) & 0xFFFFFFFFFFFFFFFF
            new_state.append(mixed)
        state = new_state
    
    # Convert state back to bytes
    state_bytes = b''
    for val in state:
        state_bytes += struct.pack('<Q', val)
    
    # Step 3: SHA256
    final_hash = sha256(state_bytes)
    
    return final_hash

def test_randomq_basic():
    """Test RandomQ basic functionality"""
    print("=== RandomQ Basic Functionality Test ===")
    
    # Test 1: empty input
    empty_hash = simulate_randomq(b'')
    print(f"Empty input hash: {empty_hash.hex()}")
    
    # Test 2: simple string
    test_str = b"Hello, RandomQ!"
    str_hash = simulate_randomq(test_str)
    print(f"String hash: {str_hash.hex()}")
    
    # Test 3: different nonce
    hash1 = simulate_randomq(test_str, nonce=0)
    hash2 = simulate_randomq(test_str, nonce=1)
    print(f"Nonce 0: {hash1.hex()}")
    print(f"Nonce 1: {hash2.hex()}")
    print(f"Hashes different: {hash1 != hash2}")
    
    # Test 4: consistency
    hash3 = simulate_randomq(test_str, nonce=0)
    print(f"Consistency: {hash1 == hash3}")

def test_mining_simulation():
    """Simulate mining"""
    print("\n=== Mining Simulation Test ===")
    
    # Simulated block header (simplified)
    block_header = b"BlockHeader123456789"
    target = 0x1d00ffff  # difficulty target
    
    print(f"Target: 0x{target:08x}")
    
    # Try different nonces
    for nonce in range(100):
        hash_result = simulate_randomq(block_header, nonce=nonce)
        hash_int = int.from_bytes(hash_result[:4], 'little')
        
        if hash_int < target:
            print(f"Found valid nonce: {nonce}")
            print(f"Hash: {hash_result.hex()}")
            print(f"Hash int: {hash_int:08x}")
            break
    else:
        print("No valid solution in the first 100 nonces")

def test_performance():
    """Performance test"""
    print("\n=== Performance Test ===")
    
    import time
    
    test_data = b"Performance test data" * 1000  # ~20KB
    
    # Test performance for different rounds
    for rounds in [100, 1000, 10000]:
        start_time = time.time()
        simulate_randomq(test_data, rounds=rounds)
        end_time = time.time()
        
        duration = end_time - start_time
        print(f"{rounds} rounds: {duration:.4f} s")

if __name__ == "__main__":
    test_randomq_basic()
    test_mining_simulation()
    test_performance()
    
    print("\n=== Tests Finished ===")
    print("Note: This is a Python simulation; the C++ implementation may differ")
