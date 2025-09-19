#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <stdint.h>

// CUDA version of RandomQ constants
__constant__ uint64_t RANDOMQ_CONSTANTS[25] = {
    0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL,
    0xa54ff53a5f1d36f1ULL, 0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
    0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL, 0x428a2f98d728ae22ULL,
    0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL,
    0xab1c5ed5da6d8118ULL, 0xd807aa98a3030242ULL, 0x12835b0145706fbeULL,
    0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL, 0x72be5d74f27b896fULL,
    0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL
};

// SHA256 constants
__constant__ uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

// CUDA device functions
__device__ inline uint32_t rotr32(uint32_t x, uint32_t r) { 
    return (x >> r) | (x << (32 - r)); 
}

__device__ inline uint32_t Ch(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ ((~x) & z);
}

__device__ inline uint32_t Maj(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

__device__ inline uint32_t Sigma0(uint32_t x) {
    return rotr32(x, 2) ^ rotr32(x, 13) ^ rotr32(x, 22);
}

__device__ inline uint32_t Sigma1(uint32_t x) {
    return rotr32(x, 6) ^ rotr32(x, 11) ^ rotr32(x, 25);
}

__device__ inline uint32_t sigma0(uint32_t x) {
    return rotr32(x, 7) ^ rotr32(x, 18) ^ (x >> 3);
}

__device__ inline uint32_t sigma1(uint32_t x) {
    return rotr32(x, 17) ^ rotr32(x, 19) ^ (x >> 10);
}

// SHA256 implementation for CUDA
__device__ void sha256_cuda(const uint8_t* msg, uint32_t len, uint8_t* out32) {
    uint8_t tmp[256];
    
    // Initialize with zeros
    for (int i = 0; i < 256; ++i) tmp[i] = 0;
    
    // Copy message
    for (uint32_t i = 0; i < len; ++i) tmp[i] = msg[i];
    
    // Add padding
    tmp[len] = 0x80;
    uint64_t bitlen = ((uint64_t)len) * 8ULL;
    
    // Calculate number of blocks
    uint32_t blocks = (len + 9 + 63) / 64;
    uint32_t last_index = blocks * 64;
    
    // Set length in last 8 bytes (big-endian)
    tmp[last_index - 8] = (uint8_t)((bitlen >> 56) & 0xFF);
    tmp[last_index - 7] = (uint8_t)((bitlen >> 48) & 0xFF);
    tmp[last_index - 6] = (uint8_t)((bitlen >> 40) & 0xFF);
    tmp[last_index - 5] = (uint8_t)((bitlen >> 32) & 0xFF);
    tmp[last_index - 4] = (uint8_t)((bitlen >> 24) & 0xFF);
    tmp[last_index - 3] = (uint8_t)((bitlen >> 16) & 0xFF);
    tmp[last_index - 2] = (uint8_t)((bitlen >> 8) & 0xFF);
    tmp[last_index - 1] = (uint8_t)(bitlen & 0xFF);
    
    // Initialize hash values
    uint32_t h0 = 0x6a09e667, h1 = 0xbb67ae85, h2 = 0x3c6ef372, h3 = 0xa54ff53a;
    uint32_t h4 = 0x510e527f, h5 = 0x9b05688c, h6 = 0x1f83d9ab, h7 = 0x5be0cd19;
    
    uint32_t W[64];
    
    // Process blocks
    for (uint32_t block = 0; block < blocks; ++block) {
        // Prepare W[0..15]
        for (int t = 0; t < 16; ++t) {
            uint32_t base = block * 64 + t * 4;
            uint32_t w = ((uint32_t)tmp[base + 0] << 24) |
                         ((uint32_t)tmp[base + 1] << 16) |
                         ((uint32_t)tmp[base + 2] << 8) |
                         ((uint32_t)tmp[base + 3] << 0);
            W[t] = w;
        }
        
        // Extend W[16..63]
        for (int t = 16; t < 64; ++t) {
            uint32_t s0 = sigma0(W[t-15]);
            uint32_t s1 = sigma1(W[t-2]);
            W[t] = W[t-16] + s0 + W[t-7] + s1;
        }
        
        // Initialize working variables
        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4, f = h5, g = h6, h = h7;
        
        // Main loop
        for (int t = 0; t < 64; ++t) {
            uint32_t T1 = h + Sigma1(e) + Ch(e, f, g) + K256[t] + W[t];
            uint32_t T2 = Sigma0(a) + Maj(a, b, c);
            h = g; g = f; f = e; e = d + T1; d = c; c = b; b = a; a = T1 + T2;
        }
        
        // Add to hash values
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e; h5 += f; h6 += g; h7 += h;
    }
    
    // Output hash (big-endian)
    out32[0] = (uint8_t)((h0 >> 24) & 0xFF);
    out32[1] = (uint8_t)((h0 >> 16) & 0xFF);
    out32[2] = (uint8_t)((h0 >> 8) & 0xFF);
    out32[3] = (uint8_t)((h0 >> 0) & 0xFF);
    
    out32[4] = (uint8_t)((h1 >> 24) & 0xFF);
    out32[5] = (uint8_t)((h1 >> 16) & 0xFF);
    out32[6] = (uint8_t)((h1 >> 8) & 0xFF);
    out32[7] = (uint8_t)((h1 >> 0) & 0xFF);
    
    out32[8] = (uint8_t)((h2 >> 24) & 0xFF);
    out32[9] = (uint8_t)((h2 >> 16) & 0xFF);
    out32[10] = (uint8_t)((h2 >> 8) & 0xFF);
    out32[11] = (uint8_t)((h2 >> 0) & 0xFF);
    
    out32[12] = (uint8_t)((h3 >> 24) & 0xFF);
    out32[13] = (uint8_t)((h3 >> 16) & 0xFF);
    out32[14] = (uint8_t)((h3 >> 8) & 0xFF);
    out32[15] = (uint8_t)((h3 >> 0) & 0xFF);
    
    out32[16] = (uint8_t)((h4 >> 24) & 0xFF);
    out32[17] = (uint8_t)((h4 >> 16) & 0xFF);
    out32[18] = (uint8_t)((h4 >> 8) & 0xFF);
    out32[19] = (uint8_t)((h4 >> 0) & 0xFF);
    
    out32[20] = (uint8_t)((h5 >> 24) & 0xFF);
    out32[21] = (uint8_t)((h5 >> 16) & 0xFF);
    out32[22] = (uint8_t)((h5 >> 8) & 0xFF);
    out32[23] = (uint8_t)((h5 >> 0) & 0xFF);
    
    out32[24] = (uint8_t)((h6 >> 24) & 0xFF);
    out32[25] = (uint8_t)((h6 >> 16) & 0xFF);
    out32[26] = (uint8_t)((h6 >> 8) & 0xFF);
    out32[27] = (uint8_t)((h6 >> 0) & 0xFF);
    
    out32[28] = (uint8_t)((h7 >> 24) & 0xFF);
    out32[29] = (uint8_t)((h7 >> 16) & 0xFF);
    out32[30] = (uint8_t)((h7 >> 8) & 0xFF);
    out32[31] = (uint8_t)((h7 >> 0) & 0xFF);
}

// RandomQ context structure
struct CRANDOMQ_CTX {
    uint64_t state[25];
    uint64_t nonce;
    uint64_t rounds;
};

__device__ inline void CRandomQ_Reset(CRANDOMQ_CTX* ctx) {
    for (int i = 0; i < 25; ++i) ctx->state[i] = RANDOMQ_CONSTANTS[i];
    ctx->nonce = 0ULL;
    ctx->rounds = 8192ULL;
}

__device__ inline void CRandomQ_SetRounds(CRANDOMQ_CTX* ctx, uint64_t rounds) {
    ctx->rounds = rounds;
}

__device__ inline void CRandomQ_SetNonce(CRANDOMQ_CTX* ctx, uint64_t nonce) {
    ctx->nonce = nonce;
}

__device__ inline void CRandomQ_RandomQRound(CRANDOMQ_CTX* ctx) {
    // Rotate and mix
    for (int i = 0; i < 25; ++i) {
        uint64_t v = ctx->state[i];
        uint64_t rotated = (v << 13) | (v >> (64 - 13));
        uint64_t next = ctx->state[(i + 1) % 25];
        ctx->state[i] = rotated ^ next ^ (v + next);
        // Add constant
        ctx->state[i] += RANDOMQ_CONSTANTS[i];
    }
    
    // Additional mixing
    for (int i = 0; i < 25; i += 2) {
        uint64_t temp = ctx->state[i];
        ctx->state[i] = ctx->state[i] ^ ctx->state[(i + 1) % 25];
        ctx->state[(i + 1) % 25] = ctx->state[(i + 1) % 25] ^ temp;
    }
}

__device__ inline void CRandomQ_Write(CRANDOMQ_CTX* ctx, const uint8_t* input, uint32_t len) {
    if (len == 0) return;
    uint32_t offset = 0;
    while (offset < len) {
        uint32_t chunk_size = (len - offset) > 64 ? 64 : (len - offset);
        // Mix input chunk into state (up to 8 uint64 words)
        uint32_t words = chunk_size / 8;
        if (words > 8) words = 8;
        for (uint32_t i = 0; i < words; ++i) {
            uint64_t chunk = 0ULL;
            for (uint32_t j = 0; j < 8; ++j) {
                uint32_t idx = offset + i * 8 + j;
                uint8_t b = (idx < len) ? input[idx] : 0;
                chunk |= ((uint64_t)b) << (j * 8); // little-endian assembly
            }
            ctx->state[i] ^= chunk;
        }
        // Run one round
        CRandomQ_RandomQRound(ctx);
        offset += chunk_size;
    }
}

__device__ inline void CRandomQ_StateToHash(CRANDOMQ_CTX* ctx, uint8_t out[32]) {
    // Use sha256 on the 25*8 = 200 state bytes; bytes are written little-endian
    uint8_t tmp[200];
    for (int i = 0; i < 25; ++i) {
        uint64_t v = ctx->state[i];
        for (int j = 0; j < 8; ++j) {
            tmp[i * 8 + j] = (uint8_t)((v >> (j * 8)) & 0xFF); // little-endian bytes
        }
    }
    // Compute sha256 over tmp (200 bytes)
    sha256_cuda(tmp, 200, out);
}

__device__ inline void CRandomQ_Finalize(CRANDOMQ_CTX* ctx, uint8_t out[32]) {
    // Mix nonce
    ctx->state[0] ^= ctx->nonce;
    // Run rounds
    for (uint64_t i = 0; i < ctx->rounds; ++i) {
        CRandomQ_RandomQRound(ctx);
    }
    // Convert state to hash via SHA256
    CRandomQ_StateToHash(ctx, out);
}

// Main CUDA kernel for RandomQ mining
__global__ void randomq_mining_kernel(
    uint8_t* header,        // 80 bytes block header
    uint32_t* nonce_base,   // base nonce
    uint8_t* target,        // 32 bytes target (little-endian)
    uint32_t* found_flag,   // found flag
    uint32_t* found_nonce,  // found nonce
    uint8_t* result_hash    // 32 bytes result hash
) {
    uint32_t gid = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t current_nonce = *nonce_base + gid;
    
    // Build local header and inject nonce
    uint8_t local_header[80];
    for (int i = 0; i < 80; ++i) local_header[i] = header[i];
    
    // Nonce in header bytes 76..79 (little-endian)
    local_header[76] = (uint8_t)((current_nonce) & 0xFF);
    local_header[77] = (uint8_t)((current_nonce >> 8) & 0xFF);
    local_header[78] = (uint8_t)((current_nonce >> 16) & 0xFF);
    local_header[79] = (uint8_t)((current_nonce >> 24) & 0xFF);
    
    // Step 1: First SHA256(header)
    uint8_t first_sha[32];
    sha256_cuda(local_header, 80, first_sha);
    
    // Step 2: CRandomQ processing
    CRANDOMQ_CTX ctx;
    CRandomQ_Reset(&ctx);
    CRandomQ_SetRounds(&ctx, 8192ULL);
    CRandomQ_SetNonce(&ctx, (uint64_t)current_nonce);
    CRandomQ_Write(&ctx, first_sha, 32);
    uint8_t randomq_out[32];
    CRandomQ_Finalize(&ctx, randomq_out);
    
    // Step 3: Final SHA256(randomq_out)
    uint8_t final32[32];
    sha256_cuda(randomq_out, 32, final32);
    
    // Convert final32 to little-endian for comparison
    uint8_t final_le[32];
    for (int i = 0; i < 32; ++i) {
        final_le[i] = final32[31 - i];
    }
    
    // Compare final_le (little-endian) with target (little-endian) from MSB to LSB
    bool meets_target = true;
    for (int i = 31; i >= 0; --i) {
        uint8_t hb = final_le[i];
        uint8_t tb = target[i];
        if (hb > tb) {
            meets_target = false;
            break;
        } else if (hb < tb) {
            meets_target = true;
            break;
        }
        // If equal, continue to next byte
    }
    
    if (meets_target) {
        // Atomic set found_flag
        uint32_t old = atomicCAS(found_flag, 0, 1);
        if (old == 0) {
            *found_nonce = current_nonce;
            for (int i = 0; i < 32; ++i) result_hash[i] = final_le[i];
        }
    }
}

// Host function to launch the kernel
extern "C" void launch_randomq_kernel(
    uint8_t* d_header,
    uint32_t* d_nonce_base,
    uint8_t* d_target,
    uint32_t* d_found_flag,
    uint32_t* d_found_nonce,
    uint8_t* d_result_hash,
    uint32_t grid_size,
    uint32_t block_size,
    cudaStream_t stream
) {
    randomq_mining_kernel<<<grid_size, block_size, 0, stream>>>(
        d_header, d_nonce_base, d_target, d_found_flag, d_found_nonce, d_result_hash
    );
}

// Device query function
extern "C" void cuda_device_query() {
    int device_count = 0;
    cudaGetDeviceCount(&device_count);
    
    printf("CUDA Device Query:\n");
    printf("Found %d CUDA device(s)\n", device_count);
    
    for (int i = 0; i < device_count; ++i) {
        cudaDeviceProp prop;
        cudaGetDeviceProperties(&prop, i);
        
        printf("Device %d: %s\n", i, prop.name);
        printf("  Compute Capability: %d.%d\n", prop.major, prop.minor);
        printf("  Total Global Memory: %.1f MB\n", (double)prop.totalGlobalMem / (1024 * 1024));
        printf("  Multiprocessors: %d\n", prop.multiProcessorCount);
        printf("  Max Threads per Block: %d\n", prop.maxThreadsPerBlock);
        printf("  Max Grid Size: %d x %d x %d\n", prop.maxGridSize[0], prop.maxGridSize[1], prop.maxGridSize[2]);
        printf("  Max Block Size: %d x %d x %d\n", prop.maxThreadsDim[0], prop.maxThreadsDim[1], prop.maxThreadsDim[2]);
        printf("  Warp Size: %d\n", prop.warpSize);
        printf("  Memory Clock Rate: %.1f MHz\n", prop.memoryClockRate * 1e-3);
        printf("  Memory Bus Width: %d bits\n", prop.memoryBusWidth);
        printf("  Peak Memory Bandwidth: %.1f GB/s\n", 
               2.0 * prop.memoryClockRate * (prop.memoryBusWidth / 8) / 1.0e6);
        printf("\n");
    }
}
