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
#include <chrono>

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
    
    // For MinGW compatibility, use single-threaded mining
    // Multi-threading with proper synchronization requires atomic operations
    // which are problematic in MinGW cross-compilation environment
    LogInfo("Starting RandomQ mining (single-threaded for MinGW compatibility), max_tries=%llu", max_tries);
    
    // Single-threaded mining loop
    uint64_t hashes_computed = 0;
    uint32_t nonce = block.nNonce;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (uint64_t i = 0; i < max_tries; ++i) {
        // Calculate hash for current nonce
        uint256 hash = CalculateRandomQHashOptimized(block, nonce);
        hashes_computed++;
        
        // Check if this nonce meets the target
        if (UintToArith256(hash) <= target) {
            result.found = true;
            result.nonce = nonce;
            result.hash = hash;
            result.hashes_computed = hashes_computed;
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration<double>(end_time - start_time).count();
            result.elapsed_time = elapsed;
            result.hash_rate = (elapsed > 0) ? (hashes_computed / elapsed) : 0.0;
            
            LogInfo("Found valid nonce: %u, hash: %s, Hashes: %llu, Time: %.2fs, Rate: %.2f H/s", 
                    result.nonce, result.hash.GetHex(), result.hashes_computed, result.elapsed_time, result.hash_rate);
            return result;
        }
        
        nonce++;
        
        // Prevent nonce overflow
        if (nonce == 0) {
            break;
        }
    }
    
    // Mining failed
    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double>(end_time - start_time).count();
    result.hashes_computed = hashes_computed;
    result.elapsed_time = elapsed;
    result.hash_rate = (elapsed > 0) ? (hashes_computed / elapsed) : 0.0;
    
    LogInfo("Mining failed after %llu hashes in %.2fs (%.2f H/s)", 
            result.hashes_computed, result.elapsed_time, result.hash_rate);
    
    return result;
}

} // namespace RandomQMining
