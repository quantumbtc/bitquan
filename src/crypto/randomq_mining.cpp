// Copyright (c) 2024-present The Bitquantum Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/randomq_mining.h>
#include <crypto/randomq_hash.h>
#include <primitives/block.h>
#include <uint256.h>
#include <arith_uint256.h>
#include <logging.h>
#include <streams.h>
#include <thread>
#include <atomic>
#include <vector>
#include <limits>
#include <chrono>
#include <algorithm>

namespace RandomQMining {

bool CheckRandomQProofOfWork(const CBlockHeader& block, unsigned int nBits, const uint256& powLimit)
{
    // Derive target from nBits
    arith_uint256 target;
    bool fNegative, fOverflow;
    target.SetCompact(nBits, &fNegative, &fOverflow);
    
    if (fNegative || target == 0 || fOverflow || target > UintToArith256(powLimit)) {
        return false;
    }
    
    // Calculate RandomQ hash
    uint256 hash = CalculateRandomQHash(block);
    
    // Check if hash meets target
    return UintToArith256(hash) <= target;
}

bool FindRandomQNonce(CBlockHeader& block, unsigned int nBits, const uint256& powLimit)
{
    // Derive target from nBits
    arith_uint256 target;
    bool fNegative, fOverflow;
    target.SetCompact(nBits, &fNegative, &fOverflow);

    if (fNegative || target == 0 || fOverflow || target > UintToArith256(powLimit)) {
        return false;
    }

    // Single-attempt check using the current nonce in block.nNonce
    uint256 hash = CalculateRandomQHashOptimized(block, block.nNonce);
    return UintToArith256(hash) <= target;
}

uint256 CalculateRandomQHash(const CBlockHeader& block)
{
    CRandomQHash hasher;
    std::vector<unsigned char> serialized;
    VectorWriter(serialized, 0, block);
    hasher.Write(std::span<const unsigned char>(serialized.data(), serialized.size()));
    hasher.SetRandomQNonce(block.nNonce);
    hasher.SetRandomQRounds(8192);
    
    uint256 result;
    hasher.Finalize(std::span<unsigned char>(result.begin(), result.size()));
    return result;
}

uint256 CalculateRandomQHashOptimized(const CBlockHeader& block, uint32_t nonce)
{
    // Create a copy of the block header with the specified nonce
    CBlockHeader headerCopy = block;
    headerCopy.nNonce = nonce;
    
    // Calculate hash
    CRandomQHash hasher;
    std::vector<unsigned char> serialized;
    VectorWriter(serialized, 0, headerCopy);
    hasher.Write(std::span<const unsigned char>(serialized.data(), serialized.size()));
    hasher.SetRandomQNonce(nonce);
    hasher.SetRandomQRounds(8192);
    
    uint256 result;
    hasher.Finalize(std::span<unsigned char>(result.begin(), result.size()));
    return result;
}

MiningResult MultiThreadedMining(const CBlockHeader& block, unsigned int nBits, const uint256& powLimit, 
                                 uint64_t max_tries, int thread_count)
{
    MiningResult result;
    
    // Derive target from nBits
    arith_uint256 target;
    bool fNegative, fOverflow;
    target.SetCompact(nBits, &fNegative, &fOverflow);
    
    if (fNegative || target == 0 || fOverflow || target > UintToArith256(powLimit)) {
        return result;
    }
    
    // Determine number of threads to use
    if (thread_count <= 0) {
        thread_count = std::thread::hardware_concurrency();
        if (thread_count <= 0) thread_count = 1; // Fallback to single thread
    }
    
    // Limit thread count to prevent excessive resource usage
    thread_count = std::min(thread_count, 16);
    
    LogInfo("Starting multi-threaded RandomQ mining with %d threads, max_tries=%llu", thread_count, max_tries);
    
    // Shared state for coordination between threads
    std::atomic<bool> found{false};
    std::atomic<uint32_t> found_nonce{0};
    std::atomic<uint256> found_hash{};
    std::atomic<uint64_t> total_hashes{0};
    
    // Calculate nonce range per thread
    const uint64_t nonces_per_thread = max_tries / thread_count;
    const uint32_t nonce_increment = 1;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Worker function for each thread
    auto worker = [&](uint32_t start_nonce, uint64_t thread_max_tries) {
        uint64_t local_hashes = 0;
        uint32_t nonce = start_nonce;
        
        while (!found && local_hashes < thread_max_tries) {
            // Check if another thread found a solution
            if (found) break;
            
            // Calculate hash for current nonce
            uint256 hash = CalculateRandomQHashOptimized(block, nonce);
            local_hashes++;
            
            // Check if this nonce meets the target
            if (UintToArith256(hash) <= target) {
                // Try to claim the solution
                if (!found.exchange(true)) {
                    found_nonce = nonce;
                    found_hash = hash;
                    LogInfo("Found valid nonce: %u, hash: %s", nonce, hash.GetHex());
                }
                break;
            }
            
            nonce += nonce_increment;
            
            // Prevent nonce overflow
            if (nonce < start_nonce) {
                break;
            }
        }
        
        // Update total hash count
        total_hashes += local_hashes;
    };
    
    // Start worker threads
    std::vector<std::thread> workers;
    for (int i = 0; i < thread_count; ++i) {
        uint32_t start_nonce = block.nNonce + (i * nonces_per_thread);
        uint64_t thread_tries = (i == thread_count - 1) ? 
            (max_tries - i * nonces_per_thread) : nonces_per_thread;
        
        workers.emplace_back(worker, start_nonce, thread_tries);
    }
    
    // Wait for all threads to complete
    for (auto& worker : workers) {
        worker.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double>(end_time - start_time).count();
    
    // Set result
    result.found = found;
    result.nonce = found_nonce;
    result.hash = found_hash;
    result.hashes_computed = total_hashes;
    result.elapsed_time = elapsed;
    result.hash_rate = (elapsed > 0) ? (total_hashes / elapsed) : 0.0;
    
    if (result.found) {
        LogInfo("Mining successful! Nonce: %u, Hash: %s, Hashes: %llu, Time: %.2fs, Rate: %.2f H/s", 
                result.nonce, result.hash.GetHex(), result.hashes_computed, result.elapsed_time, result.hash_rate);
    } else {
        LogInfo("Mining failed after %llu hashes in %.2fs (%.2f H/s)", 
                result.hashes_computed, result.elapsed_time, result.hash_rate);
    }
    
    return result;
}

} // namespace RandomQMining
