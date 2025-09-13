#!/usr/bin/env python3
"""
Test script to verify the mining fix
"""

import subprocess
import json
import time
import sys

def run_command(cmd, timeout=60):
    """Run a command and return the result"""
    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=timeout)
        return result.returncode == 0, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        return False, "", "Command timed out"
    except Exception as e:
        return False, "", str(e)

def test_mining_with_fix():
    """Test mining after the RandomQ fix"""
    print("=== Testing Mining After RandomQ Fix ===")
    print()
    
    test_address = "btq1qrv7jvtvjwhe33qvpqrns9pllwhzz8ytdafra6l"
    
    # Test 1: Single-threaded mining with moderate tries
    print("1. Testing single-threaded mining (1 thread, 10000 tries)...")
    cmd = f'bitquantum-cli generatetoaddress 1 "{test_address}" 10000 1'
    print(f"   Command: {cmd}")
    
    start_time = time.time()
    success, stdout, stderr = run_command(cmd, timeout=120)
    end_time = time.time()
    
    if success:
        try:
            result = json.loads(stdout)
            print("✅ Single-threaded mining successful!")
            print(f"   Blocks: {len(result.get('blocks', []))}")
            print(f"   Total hashes: {result.get('total_hashes', 'N/A')}")
            print(f"   Time taken: {end_time - start_time:.2f} seconds")
            print(f"   Hash rate: {result.get('average_hashrate', 'N/A')} H/s")
        except json.JSONDecodeError:
            print("✅ Mining completed but result parsing failed")
            print(f"   Raw output: {stdout}")
    else:
        print("❌ Single-threaded mining failed")
        print(f"   Error: {stderr}")
        print(f"   Time taken: {end_time - start_time:.2f} seconds")
    
    # Test 2: Multi-threaded mining
    print("\n2. Testing multi-threaded mining (4 threads, 10000 tries)...")
    cmd = f'bitquantum-cli generatetoaddress 1 "{test_address}" 10000 4'
    print(f"   Command: {cmd}")
    
    start_time = time.time()
    success, stdout, stderr = run_command(cmd, timeout=120)
    end_time = time.time()
    
    if success:
        try:
            result = json.loads(stdout)
            print("✅ Multi-threaded mining successful!")
            print(f"   Blocks: {len(result.get('blocks', []))}")
            print(f"   Total hashes: {result.get('total_hashes', 'N/A')}")
            print(f"   Time taken: {end_time - start_time:.2f} seconds")
            print(f"   Hash rate: {result.get('average_hashrate', 'N/A')} H/s")
            print(f"   Threads used: {result.get('threads_used', 'N/A')}")
        except json.JSONDecodeError:
            print("✅ Multi-threaded mining completed but result parsing failed")
            print(f"   Raw output: {stdout}")
    else:
        print("❌ Multi-threaded mining failed")
        print(f"   Error: {stderr}")
        print(f"   Time taken: {end_time - start_time:.2f} seconds")
    
    print("\n=== Test completed ===")

if __name__ == "__main__":
    test_mining_with_fix()
