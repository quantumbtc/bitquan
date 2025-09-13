#!/usr/bin/env python3

# Calculate BitQuantum difficulty values

# powLimit from chainparams.cpp
powLimit_hex = "00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
powLimit_int = int(powLimit_hex, 16)

print("=== BitQuantum Mainnet Difficulty Analysis ===")
print()
print(f"powLimit (hex): 0x{powLimit_hex}")
print(f"powLimit (int): {powLimit_int}")
print()

# Genesis nBits: 0x1e0ffff0
genesis_nbits = 0x1e0ffff0
exponent = genesis_nbits >> 24  # 0x1e = 30
mantissa = genesis_nbits & 0xffffff  # 0x0ffff0

print(f"Genesis nBits: 0x{genesis_nbits:08x}")
print(f"Exponent: {exponent}")
print(f"Mantissa: 0x{mantissa:06x}")
print()

# Calculate target
if exponent <= 3:
    target = mantissa >> (8 * (3 - exponent))
else:
    target = mantissa << (8 * (exponent - 3))

print(f"Genesis target: 0x{target:064x}")
print(f"Genesis target (int): {target}")
print()

# Check if they are equal
print(f"Genesis target == powLimit: {target == powLimit_int}")
print()

# Calculate difficulty ratio
if target != 0:
    difficulty_ratio = powLimit_int / target
    print(f"Difficulty ratio (powLimit/genesis): {difficulty_ratio:.2f}")
    print(f"This means genesis is {difficulty_ratio:.2f} times easier than powLimit")
else:
    print("Genesis target is 0 (invalid)")

print()
print("=== Standard Bitcoin Difficulty 1 ===")
# Standard Bitcoin difficulty 1 nBits
btc_difficulty_1_nbits = 0x1d00ffff
btc_exponent = btc_difficulty_1_nbits >> 24  # 0x1d = 29
btc_mantissa = btc_difficulty_1_nbits & 0xffffff  # 0x00ffff

btc_target = btc_mantissa << (8 * (btc_exponent - 3))
print(f"Bitcoin difficulty 1 nBits: 0x{btc_difficulty_1_nbits:08x}")
print(f"Bitcoin difficulty 1 target: 0x{btc_target:064x}")
print(f"Bitcoin difficulty 1 target == powLimit: {btc_target == powLimit_int}")

print()
print("=== Conclusion ===")
print("1. Mainnet powLimit: 0x00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
print("2. Genesis nBits: 0x1e0ffff0")
print("3. Genesis target is much easier than powLimit")
print("4. This means mining should be relatively easy at the start")
print("5. The difficulty will adjust over time based on block times")
