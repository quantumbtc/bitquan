#!/usr/bin/env python3
"""
Debug script to test BitQuantum mining functionality
"""

import subprocess
import json
import time
import sys

def run_command(cmd):
    """Run a command and return the result"""
    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=30)
        return result.returncode == 0, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        return False, "", "Command timed out"
    except Exception as e:
        return False, "", str(e)

def test_mining():
    """Test mining functionality"""
    print("=== BitQuantum Mining Debug Test ===")
    print()
    
    # Test address
    test_address = "btq1qrv7jvtvjwhe33qvpqrns9pllwhzz8ytdafra6l"
    
    # Check if node is running
    print("1. Checking if node is running...")
    success, stdout, stderr = run_command("bitquantum-cli getblockchaininfo")
    if not success:
        print("❌ Error: Node not running or unreachable")
        print(f"Error: {stderr}")
        return False
    print("✅ Node is running")
    
    # Get blockchain info
    print("\n2. Getting blockchain info...")
    try:
        info = json.loads(stdout)
        print(f"   Blocks: {info.get('blocks', 'N/A')}")
        print(f"   Chain: {info.get('chain', 'N/A')}")
        print(f"   Difficulty: {info.get('difficulty', 'N/A')}")
        print(f"   Chainwork: {info.get('chainwork', 'N/A')}")
    except json.JSONDecodeError:
        print("   Could not parse blockchain info")
    
    # Test single-threaded mining with very low difficulty
    print("\n3. Testing single-threaded mining (1 thread, 1000 tries)...")
    cmd = f'bitquantum-cli generatetoaddress 1 "{test_address}" 1000 1'
    print(f"   Command: {cmd}")
    
    start_time = time.time()
    success, stdout, stderr = run_command(cmd)
    end_time = time.time()
    
    if success:
        try:
            result = json.loads(stdout)
            print("✅ Mining successful!")
            print(f"   Result: {json.dumps(result, indent=2)}")
            print(f"   Time taken: {end_time - start_time:.2f} seconds")
        except json.JSONDecodeError:
            print("✅ Mining completed but result parsing failed")
            print(f"   Raw output: {stdout}")
    else:
        print("❌ Mining failed")
        print(f"   Error: {stderr}")
        print(f"   Time taken: {end_time - start_time:.2f} seconds")
    
    # Test multi-threaded mining
    print("\n4. Testing multi-threaded mining (4 threads, 1000 tries)...")
    cmd = f'bitquantum-cli generatetoaddress 1 "{test_address}" 1000 4'
    print(f"   Command: {cmd}")
    
    start_time = time.time()
    success, stdout, stderr = run_command(cmd)
    end_time = time.time()
    
    if success:
        try:
            result = json.loads(stdout)
            print("✅ Multi-threaded mining successful!")
            print(f"   Result: {json.dumps(result, indent=2)}")
            print(f"   Time taken: {end_time - start_time:.2f} seconds")
        except json.JSONDecodeError:
            print("✅ Multi-threaded mining completed but result parsing failed")
            print(f"   Raw output: {stdout}")
    else:
        print("❌ Multi-threaded mining failed")
        print(f"   Error: {stderr}")
        print(f"   Time taken: {end_time - start_time:.2f} seconds")
    
    # Test with higher max tries
    print("\n5. Testing with higher max tries (10000 tries, 4 threads)...")
    cmd = f'bitquantum-cli generatetoaddress 1 "{test_address}" 10000 4'
    print(f"   Command: {cmd}")
    
    start_time = time.time()
    success, stdout, stderr = run_command(cmd)
    end_time = time.time()
    
    if success:
        try:
            result = json.loads(stdout)
            print("✅ High-tries mining successful!")
            print(f"   Result: {json.dumps(result, indent=2)}")
            print(f"   Time taken: {end_time - start_time:.2f} seconds")
        except json.JSONDecodeError:
            print("✅ High-tries mining completed but result parsing failed")
            print(f"   Raw output: {stdout}")
    else:
        print("❌ High-tries mining failed")
        print(f"   Error: {stderr}")
        print(f"   Time taken: {end_time - start_time:.2f} seconds")
    
    print("\n=== Test completed ===")
    return True

if __name__ == "__main__":
    test_mining()
