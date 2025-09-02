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
#include <thread>
#include <atomic>
#include <vector>
#include <limits>

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

bool FindRandomQNonce(CBlockHeader& block, unsigned int nBits, const uint256& powLimit, uint64_t /*maxAttempts*/)
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

} // namespace RandomQMining
