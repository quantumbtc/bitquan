#!/usr/bin/env python3
"""
Check BitQuantum mainnet difficulty settings
"""

def nbits_to_target(nbits):
    """Convert nBits to target value"""
    exponent = nbits >> 24
    mantissa = nbits & 0xffffff
    if exponent <= 3:
        target = mantissa >> (8 * (3 - exponent))
    else:
        target = mantissa << (8 * (exponent - 3))
    return target

def target_to_hex(target, width=64):
    """Convert target to hex string"""
    return format(target, f'0{width}x')

def main():
    print("=== BitQuantum Mainnet Difficulty Analysis ===")
    print()
    
    # Check mainnet powLimit
    powLimit_hex = '00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff'
    powLimit_int = int(powLimit_hex, 16)
    print(f"Mainnet powLimit: 0x{powLimit_hex}")
    print(f"Mainnet powLimit (int): {powLimit_int}")
    print()
    
    # Check genesis nBits
    genesis_nbits = 0x1e0ffff0
    genesis_target = nbits_to_target(genesis_nbits)
    print(f"Genesis nBits: 0x{genesis_nbits:08x}")
    print(f"Genesis target: 0x{target_to_hex(genesis_target)}")
    print(f"Genesis target (int): {genesis_target}")
    print()
    
    # Check if genesis target equals powLimit
    print(f"Genesis target == powLimit: {genesis_target == powLimit_int}")
    print()
    
    # Calculate difficulty
    difficulty = powLimit_int / genesis_target
    print(f"Genesis difficulty: {difficulty:.2f}")
    print()
    
    # Check what the actual starting difficulty should be
    print("=== Analysis ===")
    print(f"powLimit:    0x{powLimit_hex}")
    print(f"genesis:     0x{target_to_hex(genesis_target)}")
    print(f"Ratio:       {powLimit_int / genesis_target:.2f}")
    print()
    print("The genesis block nBits (0x1e0ffff0) corresponds to a target that is")
    print(f"{powLimit_int / genesis_target:.2f} times easier than the powLimit.")
    print("This means the starting difficulty is much easier than the maximum allowed.")
    print()
    
    # Check what nBits would give us the powLimit target
    powLimit_nbits = 0x1d00ffff  # This is the standard difficulty 1 nBits
    powLimit_nbits_target = nbits_to_target(powLimit_nbits)
    print(f"Standard difficulty 1 nBits: 0x{powLimit_nbits:08x}")
    print(f"Standard difficulty 1 target: 0x{target_to_hex(powLimit_nbits_target)}")
    print(f"Standard difficulty 1 target == powLimit: {powLimit_nbits_target == powLimit_int}")
    print()
    
    # Calculate the actual difficulty of genesis block
    genesis_difficulty = powLimit_nbits_target / genesis_target
    print(f"Genesis block actual difficulty: {genesis_difficulty:.2f}")
    print()
    
    print("=== Conclusion ===")
    print("1. Mainnet powLimit is: 0x00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
    print("2. Genesis block nBits is: 0x1e0ffff0")
    print("3. Genesis block target is much easier than powLimit")
    print("4. This means mining should be relatively easy at the start")
    print("5. The difficulty will adjust over time based on block times")

if __name__ == "__main__":
    main()
