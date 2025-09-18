// randomq_kernel_full.cl
// Full SHA256 + CRandomQ (exact port of your CRandomQ.cpp) + SHA256 pipeline
#pragma OPENCL EXTENSION cl_khr_int64_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable

typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned char uchar;

// ---------- RandomQ constants (from CPU) ----------
__constant ulong RANDOMQ_CONSTANTS[25] = {
    (ulong)0x6a09e667f3bcc908UL, (ulong)0xbb67ae8584caa73bUL, (ulong)0x3c6ef372fe94f82bUL,
    (ulong)0xa54ff53a5f1d36f1UL, (ulong)0x510e527fade682d1UL, (ulong)0x9b05688c2b3e6c1fUL,
    (ulong)0x1f83d9abfb41bd6bUL, (ulong)0x5be0cd19137e2179UL, (ulong)0x428a2f98d728ae22UL,
    (ulong)0x7137449123ef65cdUL, (ulong)0xb5c0fbcfec4d3b2fUL, (ulong)0xe9b5dba58189dbbcUL,
    (ulong)0x3956c25bf348b538UL, (ulong)0x59f111f1b605d019UL, (ulong)0x923f82a4af194f9bUL,
    (ulong)0xab1c5ed5da6d8118UL, (ulong)0xd807aa98a3030242UL, (ulong)0x12835b0145706fbeUL,
    (ulong)0x243185be4ee4b28cUL, (ulong)0x550c7dc3d5ffb4e2UL, (ulong)0x72be5d74f27b896fUL,
    (ulong)0x80deb1fe3b1696b1UL, (ulong)0x9bdc06a725c71235UL, (ulong)0xc19bf174cf692694UL,
    (ulong)0xe49b69c19ef14ad2UL
};

// ---------- SHA256 utilities ----------
static inline uint rotr32(uint x, uint r) { return (x >> r) | (x << (32 - r)); }
static inline uint Ch(uint x,uint y,uint z){return (x & y) ^ ((~x) & z);}
static inline uint Maj(uint x,uint y,uint z){return (x & y) ^ (x & z) ^ (y & z);}
static inline uint Sigma0(uint x){return rotr32(x,2)^rotr32(x,13)^rotr32(x,22);}
static inline uint Sigma1(uint x){return rotr32(x,6)^rotr32(x,11)^rotr32(x,25);}
static inline uint sigma0(uint x){return rotr32(x,7)^rotr32(x,18)^(x>>3);}
static inline uint sigma1(uint x){return rotr32(x,17)^rotr32(x,19)^(x>>10);}

__constant uint K256[64] = {
 0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
 0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
 0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
 0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
 0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
 0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
 0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
 0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U
};

// General SHA256 implementation for message length <= 256 bytes (sufficient for our uses)
void sha256_general(const __private uchar* msg, uint len, __private uchar out32[32]) {
    // tmp buffer up to 4 blocks (4*64=256)
    __private uchar tmp[256];
    for (int i = 0; i < 256; ++i) tmp[i] = 0;
    for (uint i = 0; i < len; ++i) tmp[i] = msg[i];
    tmp[len] = (uchar)0x80;
    ulong bitlen = ((ulong)len) * 8UL;
    // compute number of blocks = ceil((len + 9)/64)
    uint blocks = (uint)((len + 9 + 63) / 64);
    // set length in last 8 bytes of the last block (big-endian)
    uint last_index = blocks * 64;
    tmp[last_index - 8] = (uchar)((bitlen >> 56) & 0xFF);
    tmp[last_index - 7] = (uchar)((bitlen >> 48) & 0xFF);
    tmp[last_index - 6] = (uchar)((bitlen >> 40) & 0xFF);
    tmp[last_index - 5] = (uchar)((bitlen >> 32) & 0xFF);
    tmp[last_index - 4] = (uchar)((bitlen >> 24) & 0xFF);
    tmp[last_index - 3] = (uchar)((bitlen >> 16) & 0xFF);
    tmp[last_index - 2] = (uchar)((bitlen >> 8) & 0xFF);
    tmp[last_index - 1] = (uchar)(bitlen & 0xFF);

    uint h0 = 0x6a09e667U, h1 = 0xbb67ae85U, h2 = 0x3c6ef372U, h3 = 0xa54ff53aU;
    uint h4 = 0x510e527fU, h5 = 0x9b05688cU, h6 = 0x1f83d9abU, h7 = 0x5be0cd19U;

    uint W[64];

    for (uint block = 0; block < blocks; ++block) {
        // prepare W[0..15]
        for (int t = 0; t < 16; ++t) {
            uint base = block*64 + t*4;
            uint w = ((uint)tmp[base + 0] << 24) |
                     ((uint)tmp[base + 1] << 16) |
                     ((uint)tmp[base + 2] << 8) |
                     ((uint)tmp[base + 3] << 0);
            W[t] = w;
        }
        for (int t = 16; t < 64; ++t) {
            uint s0 = sigma0(W[t-15]);
            uint s1 = sigma1(W[t-2]);
            W[t] = W[t-16] + s0 + W[t-7] + s1;
        }

        uint a = h0, b = h1, c = h2, d = h3, e = h4, f = h5, g = h6, h = h7;
        for (int t = 0; t < 64; ++t) {
            uint T1 = h + Sigma1(e) + Ch(e,f,g) + K256[t] + W[t];
            uint T2 = Sigma0(a) + Maj(a,b,c);
            h = g; g = f; f = e; e = d + T1; d = c; c = b; b = a; a = T1 + T2;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e; h5 += f; h6 += g; h7 += h;
    }

    // write out big-endian bytes
    out32[0] = (uchar)((h0 >> 24) & 0xFF);
    out32[1] = (uchar)((h0 >> 16) & 0xFF);
    out32[2] = (uchar)((h0 >> 8) & 0xFF);
    out32[3] = (uchar)((h0 >> 0) & 0xFF);

    out32[4] = (uchar)((h1 >> 24) & 0xFF);
    out32[5] = (uchar)((h1 >> 16) & 0xFF);
    out32[6] = (uchar)((h1 >> 8) & 0xFF);
    out32[7] = (uchar)((h1 >> 0) & 0xFF);

    out32[8] = (uchar)((h2 >> 24) & 0xFF);
    out32[9] = (uchar)((h2 >> 16) & 0xFF);
    out32[10] = (uchar)((h2 >> 8) & 0xFF);
    out32[11] = (uchar)((h2 >> 0) & 0xFF);

    out32[12] = (uchar)((h3 >> 24) & 0xFF);
    out32[13] = (uchar)((h3 >> 16) & 0xFF);
    out32[14] = (uchar)((h3 >> 8) & 0xFF);
    out32[15] = (uchar)((h3 >> 0) & 0xFF);

    out32[16] = (uchar)((h4 >> 24) & 0xFF);
    out32[17] = (uchar)((h4 >> 16) & 0xFF);
    out32[18] = (uchar)((h4 >> 8) & 0xFF);
    out32[19] = (uchar)((h4 >> 0) & 0xFF);

    out32[20] = (uchar)((h5 >> 24) & 0xFF);
    out32[21] = (uchar)((h5 >> 16) & 0xFF);
    out32[22] = (uchar)((h5 >> 8) & 0xFF);
    out32[23] = (uchar)((h5 >> 0) & 0xFF);

    out32[24] = (uchar)((h6 >> 24) & 0xFF);
    out32[25] = (uchar)((h6 >> 16) & 0xFF);
    out32[26] = (uchar)((h6 >> 8) & 0xFF);
    out32[27] = (uchar)((h6 >> 0) & 0xFF);

    out32[28] = (uchar)((h7 >> 24) & 0xFF);
    out32[29] = (uchar)((h7 >> 16) & 0xFF);
    out32[30] = (uchar)((h7 >> 8) & 0xFF);
    out32[31] = (uchar)((h7 >> 0) & 0xFF);
}

// ---------- CRandomQ implementation (exactly ported) ----------
typedef struct {
    ulong state[25];
    ulong nonce;
    ulong rounds;
} CRANDOMQ_CTX;

inline void CRandomQ_Reset(__private CRANDOMQ_CTX* ctx) {
    // Initialize state with constants (like CPU version)
    for (int i = 0; i < 25; ++i) ctx->state[i] = RANDOMQ_CONSTANTS[i];
    ctx->nonce = (ulong)0;
    ctx->rounds = (ulong)8192;
}

inline void CRandomQ_SetRounds(__private CRANDOMQ_CTX* ctx, ulong rounds) {
    ctx->rounds = rounds;
}

inline void CRandomQ_SetNonce(__private CRANDOMQ_CTX* ctx, ulong nonce) {
    ctx->nonce = nonce;
}

inline void CRandomQ_RandomQRound(__private CRANDOMQ_CTX* ctx) {
    // Rotate and mix
    for (int i = 0; i < 25; ++i) {
        ulong v = ctx->state[i];
        ulong rotated = (v << 13) | (v >> (64 - 13));
        ulong next = ctx->state[(i + 1) % 25];
        // state[i] = rotated ^ next ^ (state[i] + next);
        ctx->state[i] = rotated ^ next ^ (v + next);
        // add constant
        ctx->state[i] += RANDOMQ_CONSTANTS[i];
    }
    // additional mixing
    for (int i = 0; i < 25; i += 2) {
        ulong temp = ctx->state[i];
        ctx->state[i] = ctx->state[i] ^ ctx->state[(i + 1) % 25];
        ctx->state[(i + 1) % 25] = ctx->state[(i + 1) % 25] ^ temp;
    }
}

inline void CRandomQ_Write(__private CRANDOMQ_CTX* ctx, const __private uchar* input, uint len) {
    if (len == 0) return;
    uint offset = 0;
    while (offset < len) {
        uint chunk_size = (len - offset) > 64 ? 64 : (len - offset);
        // mix input chunk into state (up to 8 uint64 words)
        uint words = chunk_size / 8;
        if (words > 8) words = 8;
        for (uint i = 0; i < words; ++i) {
            ulong chunk = (ulong)0;
            for (uint j = 0; j < 8; ++j) {
                uint idx = offset + i*8 + j;
                uchar b = (idx < len) ? input[idx] : (uchar)0;
                chunk |= ((ulong)b) << (j * 8); // little-endian assembly
            }
            ctx->state[i] ^= chunk;
        }
        // run one round
        CRandomQ_RandomQRound(ctx);
        offset += chunk_size;
    }
}

inline void CRandomQ_StateToHash(__private CRANDOMQ_CTX* ctx, __private uchar out[32]) {
    // Use sha256 on the 25*8 = 200 state bytes; bytes are written little-endian per CPU
    __private uchar tmp[200];
    for (int i = 0; i < 25; ++i) {
        ulong v = ctx->state[i];
        for (int j = 0; j < 8; ++j) {
            tmp[i*8 + j] = (uchar)((v >> (j * 8)) & 0xFF); // little-endian bytes
        }
    }
    // compute sha256 over tmp (200 bytes)
    sha256_general(tmp, 200u, out);
}

inline void CRandomQ_Finalize(__private CRANDOMQ_CTX* ctx, __private uchar out[32]) {
    // mix nonce
    ctx->state[0] ^= (ulong)ctx->nonce;
    // run rounds
    for (ulong i = 0; i < ctx->rounds; ++i) {
        CRandomQ_RandomQRound(ctx);
    }
    // convert state to hash via SHA256
    CRandomQ_StateToHash(ctx, out);
}

// ---------- Mining kernel ----------
__kernel void randomq_mining(
    __global const uchar* header80, // 80 bytes header (little-endian)
    __global const uint* nonce_base, // uint32 base nonce
    __global const uchar* target32, // 32 bytes target (big-endian)
    __global volatile uint* found_flag,
    __global uint* found_nonce,
    __global uchar* result_hash // 32 bytes output for first finder
) {
    uint gid = get_global_id(0);
    // compute current nonce as 32-bit then widen to 64
    uint base = *nonce_base;
    ulong current_nonce = (ulong)(base + gid);

    // build local header and inject nonce (header80 provided as little-endian)
    __private uchar local_header[80];
    for (int i = 0; i < 80; ++i) local_header[i] = header80[i];
    // nonce in header bytes 76..79 little-endian
    local_header[76] = (uchar)((current_nonce) & 0xFF);
    local_header[77] = (uchar)((current_nonce >> 8) & 0xFF);
    local_header[78] = (uchar)((current_nonce >> 16) & 0xFF);
    local_header[79] = (uchar)((current_nonce >> 24) & 0xFF);

    // Correct algorithm: SHA256 -> RandomQ -> SHA256 (like CRandomQHash)
    
    // Step 1: First SHA256(header)
    __private uchar first_sha[32];
    sha256_general(local_header, 80u, first_sha);
    
    // Step 2: RandomQ(first_sha)
    CRANDOMQ_CTX ctx;
    CRandomQ_Reset(&ctx);
    CRandomQ_SetRounds(&ctx, (ulong)8192);
    CRandomQ_SetNonce(&ctx, current_nonce);
    CRandomQ_Write(&ctx, first_sha, 32u); // Write SHA256 result, not header
    __private uchar randomq_out[32];
    CRandomQ_Finalize(&ctx, randomq_out); // This does StateToHash with SHA256 internally
    
    // Step 3: Second SHA256(randomq_out)
    __private uchar final32[32];
    sha256_general(randomq_out, 32u, final32);

    // Compare final32 (big-endian) with target32 (big-endian) MSB->LSB
    bool meets_target = true;
    for (int i = 0; i < 32; ++i) {
        uchar hb = final32[i];
        uchar tb = target32[i];
        if (hb > tb) { meets_target = false; break; }
        else if (hb < tb) break;
    }

    if (meets_target) {
        // atomic set found_flag
        uint old = atomic_cmpxchg((volatile __global uint*)found_flag, 0u, 1u);
        if (old == 0u) {
            *found_nonce = (uint)current_nonce;
            for (int i = 0; i < 32; ++i) result_hash[i] = final32[i];
        }
    }
}
