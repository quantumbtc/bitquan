// Copyright (c) 2024-present The Bitquantum Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITQUANTUM_CRYPTO_RANDOMQ_MINING_H
#define BITQUANTUM_CRYPTO_RANDOMQ_MINING_H

#include <crypto/randomq_hash.h>
#include <primitives/block.h>
#include <uint256.h>
#include <arith_uint256.h>

/** RandomQ mining utilities */
namespace RandomQMining {

/** Check if a block hash meets the proof-of-work requirement */
bool CheckRandomQProofOfWork(const CBlockHeader& block, unsigned int nBits, const uint256& powLimit);

/** Find a valid nonce for RandomQ mining */
bool FindRandomQNonce(CBlockHeader& block, unsigned int nBits, const uint256& powLimit);

/** Calculate RandomQ hash for a block header */
uint256 CalculateRandomQHash(const CBlockHeader& block);

/** Optimized RandomQ hash calculation for mining */
uint256 CalculateRandomQHashOptimized(const CBlockHeader& block, uint32_t nonce);

/** Multi-threaded RandomQ mining result */
struct MiningResult {
    bool found = false;
    uint32_t nonce = 0;
    uint256 hash;
    uint64_t hashes_computed = 0;
    double elapsed_time = 0.0;
    double hash_rate = 0.0;
};

/** Multi-threaded RandomQ mining with configurable thread count */
MiningResult MultiThreadedMining(const CBlockHeader& block, unsigned int nBits, const uint256& powLimit, 
                                 uint64_t max_tries, int thread_count = 0);

} // namespace RandomQMining

#endif // BITQUANTUM_CRYPTO_RANDOMQ_MINING_H
