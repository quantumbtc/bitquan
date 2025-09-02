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

bool FindRandomQNonce(CBlockHeader& block, unsigned int nBits, const uint256& powLimit, uint64_t maxAttempts)
{
    // Derive target from nBits
    arith_uint256 target;
    bool fNegative, fOverflow;
    target.SetCompact(nBits, &fNegative, &fOverflow);

    if (fNegative || target == 0 || fOverflow || target > UintToArith256(powLimit)) {
        return false;
    }

    // Determine threading parameters
    const unsigned int n_tasks = std::max(1u, std::thread::hardware_concurrency());
    const uint64_t per_task_attempts = (maxAttempts == 0) ? std::numeric_limits<uint64_t>::max() : (maxAttempts + n_tasks - 1) / n_tasks;

    std::atomic<bool> found{false};
    std::atomic<uint32_t> proposed_nonce{0};

    // Capture a header template; only nonce changes per attempt
    const CBlockHeader header_template = block;

    auto worker = [&](uint32_t offset, uint32_t step) {
        // Start nonce for this worker
        uint32_t nonce = offset;

        // Calculate a finish boundary to avoid overflow and keep strides aligned
        uint32_t finish = std::numeric_limits<uint32_t>::max() - step;
        finish = finish - (finish % step) + offset;

        uint64_t attempts_done = 0;
        while (!found.load(std::memory_order_relaxed) && attempts_done < per_task_attempts && nonce < finish) {
            // Batch in chunks to minimize atomic checks
            const uint32_t next = (finish - nonce < 5000 * step) ? finish : nonce + 5000 * step;
            do {
                // Compute hash with current nonce
                uint256 hash = CalculateRandomQHashOptimized(header_template, nonce);
                if (UintToArith256(hash) <= target) {
                    if (!found.exchange(true)) {
                        proposed_nonce.store(nonce, std::memory_order_relaxed);
                    }
                    return;
                }
                nonce += step;
                ++attempts_done;
            } while (!found.load(std::memory_order_relaxed) && attempts_done < per_task_attempts && nonce != next);
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(n_tasks);
    for (unsigned int i = 0; i < n_tasks; ++i) {
        threads.emplace_back(worker, i, n_tasks);
    }
    for (auto& t : threads) t.join();

    if (found.load()) {
        block.nNonce = proposed_nonce.load();
        return true;
    }

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
