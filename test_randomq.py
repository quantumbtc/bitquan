#!/usr/bin/env python3
"""
RandomQ算法测试脚本
用于验证RandomQ哈希算法的基本功能
"""

import hashlib
import struct

def sha256(data):
    """计算SHA256哈希"""
    return hashlib.sha256(data).digest()

def simulate_randomq(input_data, nonce=0, rounds=8192):
    """
    模拟RandomQ算法的简化版本
    注意：这是一个Python实现，用于测试概念，不是完整的C++实现
    """
    # 第一步：SHA256
    first_hash = sha256(input_data)
    
    # 第二步：模拟RandomQ（简化版本）
    # 使用一个简单的状态转换函数
    state = list(struct.unpack('<8Q', first_hash))  # 64字节转换为8个64位整数
    
    # 添加nonce
    state[0] ^= nonce
    
    # 模拟RandomQ轮函数
    for _ in range(min(rounds, 100)):  # 限制轮数以避免过慢
        # 简单的状态混合
        new_state = []
        for i in range(8):
            # 旋转和混合
            rotated = ((state[i] << 13) | (state[i] >> 51)) & 0xFFFFFFFFFFFFFFFF
            next_val = state[(i + 1) % 8]
            mixed = rotated ^ next_val ^ (state[i] + next_val)
            new_state.append(mixed)
        state = new_state
    
    # 将状态转换回字节
    state_bytes = b''
    for val in state:
        state_bytes += struct.pack('<Q', val)
    
    # 第三步：SHA256
    final_hash = sha256(state_bytes)
    
    return final_hash

def test_randomq_basic():
    """测试RandomQ基本功能"""
    print("=== RandomQ基本功能测试 ===")
    
    # 测试1：空输入
    empty_hash = simulate_randomq(b'')
    print(f"空输入哈希: {empty_hash.hex()}")
    
    # 测试2：简单字符串
    test_str = b"Hello, RandomQ!"
    str_hash = simulate_randomq(test_str)
    print(f"字符串哈希: {str_hash.hex()}")
    
    # 测试3：不同nonce
    hash1 = simulate_randomq(test_str, nonce=0)
    hash2 = simulate_randomq(test_str, nonce=1)
    print(f"Nonce 0: {hash1.hex()}")
    print(f"Nonce 1: {hash2.hex()}")
    print(f"哈希是否不同: {hash1 != hash2}")
    
    # 测试4：一致性
    hash3 = simulate_randomq(test_str, nonce=0)
    print(f"一致性测试: {hash1 == hash3}")

def test_mining_simulation():
    """模拟挖矿过程"""
    print("\n=== 挖矿模拟测试 ===")
    
    # 模拟区块头（简化）
    block_header = b"BlockHeader123456789"
    target = 0x1d00ffff  # 难度目标
    
    print(f"目标难度: 0x{target:08x}")
    
    # 尝试不同的nonce
    for nonce in range(100):
        hash_result = simulate_randomq(block_header, nonce=nonce)
        hash_int = int.from_bytes(hash_result[:4], 'little')
        
        if hash_int < target:
            print(f"找到有效nonce: {nonce}")
            print(f"哈希值: {hash_result.hex()}")
            print(f"哈希整数: {hash_int:08x}")
            break
    else:
        print("在前100个nonce中未找到有效解")

def test_performance():
    """性能测试"""
    print("\n=== 性能测试 ===")
    
    import time
    
    test_data = b"Performance test data" * 1000  # 20KB数据
    
    # 测试不同轮数的性能
    for rounds in [100, 1000, 10000]:
        start_time = time.time()
        simulate_randomq(test_data, rounds=rounds)
        end_time = time.time()
        
        duration = end_time - start_time
        print(f"{rounds}轮: {duration:.4f}秒")

if __name__ == "__main__":
    test_randomq_basic()
    test_mining_simulation()
    test_performance()
    
    print("\n=== 测试完成 ===")
    print("注意：这是Python模拟实现，实际C++实现可能有所不同")
