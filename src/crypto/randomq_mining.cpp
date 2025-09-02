// Copyright (c) 2024-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/randomq_mining.h>
#include <crypto/randomq_hash.h>
#include <primitives/block.h>
#include <uint256.h>
#include <arith_uint256.h>
#include <logging.h>
#include <streams.h>

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

bool FindRandomQNonce(CBlockHeader& block, unsigned int nBits, const uint256& powLimit, uint64_t maxAttempts)
{
    // Derive target from nBits
    arith_uint256 target;
    bool fNegative, fOverflow;
    target.SetCompact(nBits, &fNegative, &fOverflow);
    
    if (fNegative || target == 0 || fOverflow || target > UintToArith256(powLimit)) {
        return false;
    }
    
    uint64_t attempts = 0;
    uint32_t originalNonce = block.nNonce;
    
    while (attempts < maxAttempts) {
        // Calculate hash with current nonce
        uint256 hash = CalculateRandomQHashOptimized(block, block.nNonce);
        
        // Check if hash meets target
        if (UintToArith256(hash) <= target) {
            LogDebug(BCLog::VALIDATION, "RandomQ nonce found: %u after %lu attempts", block.nNonce, attempts);
            return true;
        }
        
        // Try next nonce
        block.nNonce++;
        attempts++;
        
        // Reset nonce if we've exhausted 32-bit range
        if (block.nNonce == 0) {
            block.nNonce = 1;
        }
    }
    
    // Restore original nonce if no solution found
    block.nNonce = originalNonce;
    LogDebug(BCLog::VALIDATION, "RandomQ nonce not found after %lu attempts", attempts);
    return false;
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

} // namespace RandomQMining
