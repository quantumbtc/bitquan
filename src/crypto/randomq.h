// Copyright (c) 2024-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CRYPTO_RANDOMQ_H
#define BITCOIN_CRYPTO_RANDOMQ_H

#include <cstdint>
#include <span>

/** A hasher class for RandomQ (Monero's anti-quantum algorithm). */
class CRandomQ
{
private:
    // RandomQ internal state
    uint64_t state[25]; // 200-byte internal state
    uint64_t nonce;
    uint64_t rounds;

public:
    static const size_t OUTPUT_SIZE = 32; // 256-bit output

    CRandomQ();
    ~CRandomQ();

    /** Initialize RandomQ with a seed */
    void Initialize(const uint8_t* seed, size_t seed_len);
    
    /** Process input data */
    CRandomQ& Write(std::span<const uint8_t> input);
    
    /** Finalize and get the hash result */
    void Finalize(uint8_t hash[OUTPUT_SIZE]);
    
    /** Reset the hasher state */
    CRandomQ& Reset();
    
    /** Set the number of rounds for RandomQ */
    void SetRounds(uint64_t rounds);
    
    /** Set the nonce value */
    void SetNonce(uint64_t nonce);

private:
    /** RandomQ core algorithm implementation */
    void RandomQRound();
    
    /** Convert internal state to output hash */
    void StateToHash(uint8_t hash[OUTPUT_SIZE]);
};

/** Compute the RandomQ hash of an object. */
template<typename T>
inline uint256 RandomQHash(const T& in1)
{
    uint256 result;
    CRandomQ().Write(MakeUCharSpan(in1)).Finalize(result.begin());
    return result;
}

/** Compute the RandomQ hash of the concatenation of two objects. */
template<typename T1, typename T2>
inline uint256 RandomQHash(const T1& in1, const T2& in2) {
    uint256 result;
    CRandomQ().Write(MakeUCharSpan(in1)).Write(MakeUCharSpan(in2)).Finalize(result.begin());
    return result;
}

#endif // BITCOIN_CRYPTO_RANDOMQ_H
