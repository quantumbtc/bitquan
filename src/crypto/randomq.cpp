// Copyright (c) 2024-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/randomq.h>
#include <crypto/sha256.h>
#include <cstring>
#include <algorithm>

// RandomQ constants
static const uint64_t RANDOMQ_CONSTANTS[25] = {
    0x6a09e667f3bcc908, 0xbb67ae8584caa73b, 0x3c6ef372fe94f82b,
    0xa54ff53a5f1d36f1, 0x510e527fade682d1, 0x9b05688c2b3e6c1f,
    0x1f83d9abfb41bd6b, 0x5be0cd19137e2179, 0x428a2f98d728ae22,
    0x7137449123ef65cd, 0xb5c0fbcfec4d3b2f, 0xe9b5dba58189dbbc,
    0x3956c25bf348b538, 0x59f111f1b605d019, 0x923f82a4af194f9b,
    0xab1c5ed5da6d8118, 0xd807aa98a3030242, 0x12835b0145706fbe,
    0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2, 0x72be5d74f27b896f,
    0x80deb1fe3b1696b1, 0x9bdc06a725c71235, 0xc19bf174cf692694,
    0xe49b69c19ef14ad2
};

CRandomQ::CRandomQ() : nonce(0), rounds(8192)
{
    Reset();
}

CRandomQ::~CRandomQ()
{
    // Clear sensitive data
    std::fill(std::begin(state), std::end(state), 0);
    nonce = 0;
    rounds = 0;
}

void CRandomQ::Initialize(const uint8_t* seed, size_t seed_len)
{
    Reset();
    
    // Initialize state with constants
    std::copy(std::begin(RANDOMQ_CONSTANTS), std::end(RANDOMQ_CONSTANTS), state);
    
    // Mix in the seed
    if (seed && seed_len > 0) {
        size_t offset = 0;
        for (size_t i = 0; i < 25 && offset < seed_len; i++) {
            uint64_t seed_chunk = 0;
            for (size_t j = 0; j < 8 && offset + j < seed_len; j++) {
                seed_chunk |= static_cast<uint64_t>(seed[offset + j]) << (j * 8);
            }
            state[i] ^= seed_chunk;
            offset += 8;
        }
    }
}

CRandomQ& CRandomQ::Write(std::span<const unsigned char> input)
{
    if (input.empty()) return *this;
    
    // Process input in 64-byte chunks
    size_t offset = 0;
    while (offset < input.size()) {
        size_t chunk_size = std::min<size_t>(64, input.size() - offset);
        
        // Mix input chunk into state
        for (size_t i = 0; i < chunk_size / 8 && i < 8; i++) {
            uint64_t chunk = 0;
            for (size_t j = 0; j < 8 && offset + i * 8 + j < input.size(); j++) {
                chunk |= static_cast<uint64_t>(input[offset + i * 8 + j]) << (j * 8);
            }
            state[i] ^= chunk;
        }
        
        // Run one round of RandomQ
        RandomQRound();
        
        offset += chunk_size;
    }
    
    return *this;
}

void CRandomQ::Finalize(unsigned char hash[OUTPUT_SIZE])
{
    // Mix in the nonce
    state[0] ^= nonce;
    
    // Run final rounds
    for (uint64_t i = 0; i < rounds; i++) {
        RandomQRound();
    }
    
    // Convert state to hash
    StateToHash(hash);
}

CRandomQ& CRandomQ::Reset()
{
    std::fill(std::begin(state), std::end(state), 0);
    nonce = 0;
    return *this;
}

void CRandomQ::SetRounds(uint64_t rounds)
{
    this->rounds = rounds;
}

void CRandomQ::SetNonce(uint64_t nonce)
{
    this->nonce = nonce;
}

void CRandomQ::RandomQRound()
{
    // RandomQ round function - simplified version based on Monero's RandomX
    // This is a simplified implementation for demonstration purposes
    
    // Rotate and mix state elements
    for (int i = 0; i < 25; i++) {
        // Rotate left by 13 bits
        uint64_t rotated = (state[i] << 13) | (state[i] >> 51);
        
        // Mix with next element (wrapping around)
        uint64_t next = state[(i + 1) % 25];
        state[i] = rotated ^ next ^ (state[i] + next);
        
        // Add constant
        state[i] += RANDOMQ_CONSTANTS[i];
    }
    
    // Additional mixing step
    for (int i = 0; i < 25; i += 2) {
        uint64_t temp = state[i];
        state[i] = state[i] ^ state[(i + 1) % 25];
        state[(i + 1) % 25] = state[(i + 1) % 25] ^ temp;
    }
}

void CRandomQ::StateToHash(unsigned char hash[OUTPUT_SIZE])
{
    // Use SHA256 to finalize the RandomQ state
    CSHA256 sha256;
    
    // Write state bytes
    for (int i = 0; i < 25; i++) {
        uint8_t bytes[8];
        for (int j = 0; j < 8; j++) {
            bytes[j] = static_cast<uint8_t>(state[i] >> (j * 8));
        }
        sha256.Write(bytes, 8);
    }
    
    // Finalize SHA256
    sha256.Finalize(hash);
}
