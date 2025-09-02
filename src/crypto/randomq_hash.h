// Copyright (c) 2024-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CRYPTO_RANDOMQ_HASH_H
#define BITCOIN_CRYPTO_RANDOMQ_HASH_H

#include <crypto/sha256.h>
#include <crypto/randomq.h>
#include <span>
#include <uint256.h>
#include <crypto/common.h>

/** A hasher class for Bitcoin's RandomQ hash (SHA256->RandomQ->SHA256). */
class CRandomQHash {
private:
    CSHA256 sha256_first;
    CRandomQ randomq;
    CSHA256 sha256_second;

public:
    static const size_t OUTPUT_SIZE = CSHA256::OUTPUT_SIZE;

    void Finalize(std::span<unsigned char> output) {
        assert(output.size() == OUTPUT_SIZE);
        
        // First SHA256
        unsigned char first_hash[CSHA256::OUTPUT_SIZE];
        sha256_first.Finalize(first_hash);
        
        // RandomQ
        unsigned char randomq_hash[CRandomQ::OUTPUT_SIZE];
        randomq.Reset();
        randomq.Write(std::span<const unsigned char>(first_hash, CSHA256::OUTPUT_SIZE));
        randomq.Finalize(randomq_hash);
        
        // Second SHA256
        sha256_second.Write(randomq_hash, CRandomQ::OUTPUT_SIZE);
        sha256_second.Finalize(output.data());
    }

    CRandomQHash& Write(std::span<const unsigned char> input) {
        sha256_first.Write(input.data(), input.size());
        return *this;
    }

    CRandomQHash& Reset() {
        sha256_first.Reset();
        randomq.Reset();
        sha256_second.Reset();
        return *this;
    }
    
    /** Set RandomQ parameters */
    void SetRandomQRounds(uint64_t rounds) {
        randomq.SetRounds(rounds);
    }
    
    void SetRandomQNonce(uint64_t nonce) {
        randomq.SetNonce(nonce);
    }
};

/** Compute the RandomQ hash of an object. */
template<typename T>
inline uint256 RandomQHash256(const T& in1)
{
    uint256 result;
    CRandomQHash().Write(MakeUCharSpan(in1)).Finalize(result.begin());
    return result;
}

/** Compute the RandomQ hash of the concatenation of two objects. */
template<typename T1, typename T2>
inline uint256 RandomQHash256(const T1& in1, const T2& in2) {
    uint256 result;
    CRandomQHash().Write(MakeUCharSpan(in1)).Write(MakeUCharSpan(in2)).Finalize(result.begin());
    return result;
}

#endif // BITCOIN_CRYPTO_RANDOMQ_HASH_H
