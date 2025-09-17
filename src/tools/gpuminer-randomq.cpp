// Copyright (c) 2024-present The Bitquantum Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <common/args.h>
#include <common/system.h>
#include <common/init.h>
#include <logging.h>
#include <util/check.h>
#include <util/strencodings.h>
#include <util/time.h>
#include <util/translation.h>
#include <rpc/request.h>
#include <univalue.h>
#include <crypto/randomq_mining.h>
#include <primitives/block.h>
#include <consensus/merkle.h>
#include <chainparamsbase.h>
#include <support/events.h>
#include <core_io.h>
#include <netbase.h>
#include <key_io.h>
#include <streams.h>
#include <serialize.h>
#include <uint256.h>

#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#endif

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/http.h>

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <csignal>
#include <fstream>

#ifdef OPENCL_FOUND
// Target OpenCL 2.0 APIs when available
#ifndef CL_TARGET_OPENCL_VERSION
#define CL_TARGET_OPENCL_VERSION 200
#endif
#include <CL/cl.h>
#endif

using namespace std::chrono_literals;

// Provide translation function symbol expected by common/init.cpp link
const TranslateFn G_TRANSLATION_FUN{nullptr};

// OpenCL GPU mining
namespace OpenCLMining {
    cl_context context = nullptr;
    cl_command_queue queue = nullptr;
    cl_program program = nullptr;
    cl_kernel kernel = nullptr;
    cl_mem header_buffer = nullptr;
    cl_mem nonce_buffer = nullptr;
    cl_mem result_buffer = nullptr;
    cl_mem target_buffer = nullptr;
    cl_mem found_flag_buffer = nullptr;
    cl_mem found_nonce_buffer = nullptr;
    bool initialized = false;
    std::mutex opencl_mutex;
    
    bool Initialize() {
        std::lock_guard<std::mutex> lock(opencl_mutex);
        if (initialized) return true;
        
#ifdef OPENCL_FOUND
        tfm::format(std::cout, "[GPU] Initializing OpenCL...\n");
        std::cout.flush();
        
        cl_int err;
        
        // Get platform count first
        cl_uint platform_count = 0;
        err = clGetPlatformIDs(0, nullptr, &platform_count);
        if (err != CL_SUCCESS || platform_count == 0) {
            tfm::format(std::cout, "[GPU] ERROR: No OpenCL platforms found (error: %d)\n", err);
            std::cout.flush();
            return false;
        }
        tfm::format(std::cout, "[GPU] Found %u OpenCL platform(s)\n", platform_count);
        std::cout.flush();
        
        // Get platform
        cl_platform_id platform;
        err = clGetPlatformIDs(1, &platform, nullptr);
        if (err != CL_SUCCESS) {
            tfm::format(std::cout, "[GPU] ERROR: Failed to get OpenCL platform (error: %d)\n", err);
            std::cout.flush();
            return false;
        }
        
        // Get platform info
        char platform_name[256] = {0};
        char platform_vendor[256] = {0};
        char platform_version[256] = {0};
        clGetPlatformInfo(platform, CL_PLATFORM_NAME, sizeof(platform_name), platform_name, nullptr);
        clGetPlatformInfo(platform, CL_PLATFORM_VENDOR, sizeof(platform_vendor), platform_vendor, nullptr);
        clGetPlatformInfo(platform, CL_PLATFORM_VERSION, sizeof(platform_version), platform_version, nullptr);
        tfm::format(std::cout, "[GPU] Platform: %s (%s) - %s\n", platform_name, platform_vendor, platform_version);
        std::cout.flush();
        
        // Get device count first
        cl_uint device_count = 0;
        err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 0, nullptr, &device_count);
        if (err != CL_SUCCESS || device_count == 0) {
            tfm::format(std::cout, "[GPU] ERROR: No GPU devices found (error: %d)\n", err);
            std::cout.flush();
            return false;
        }
        tfm::format(std::cout, "[GPU] Found %u GPU device(s)\n", device_count);
        std::cout.flush();
        
        // Get device
        cl_device_id device;
        err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);
        if (err != CL_SUCCESS) {
            tfm::format(std::cout, "[GPU] ERROR: Failed to get GPU device (error: %d)\n", err);
            std::cout.flush();
            return false;
        }
        
        // Get device info
        char device_name[256] = {0};
        char device_vendor[256] = {0};
        char device_version[256] = {0};
        cl_ulong global_mem_size = 0;
        cl_uint compute_units = 0;
        size_t max_work_group_size = 0;
        clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(device_name), device_name, nullptr);
        clGetDeviceInfo(device, CL_DEVICE_VENDOR, sizeof(device_vendor), device_vendor, nullptr);
        clGetDeviceInfo(device, CL_DEVICE_VERSION, sizeof(device_version), device_version, nullptr);
        clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(global_mem_size), &global_mem_size, nullptr);
        clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(compute_units), &compute_units, nullptr);
        clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(max_work_group_size), &max_work_group_size, nullptr);
        
        tfm::format(std::cout, "[GPU] Device: %s (%s)\n", device_name, device_vendor);
        tfm::format(std::cout, "[GPU] Version: %s\n", device_version);
        tfm::format(std::cout, "[GPU] Global Memory: %.2f MB\n", (double)global_mem_size / (1024.0 * 1024.0));
        tfm::format(std::cout, "[GPU] Compute Units: %u\n", compute_units);
        tfm::format(std::cout, "[GPU] Max Work Group Size: %zu\n", max_work_group_size);
        std::cout.flush();
        
        // Create context
        tfm::format(std::cout, "[GPU] Creating OpenCL context...\n");
        std::cout.flush();
        context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &err);
        if (err != CL_SUCCESS) {
            tfm::format(std::cout, "[GPU] ERROR: Failed to create OpenCL context (error: %d)\n", err);
            std::cout.flush();
            return false;
        }
        
        // Create command queue (prefer OpenCL 2.0 API)
        tfm::format(std::cout, "[GPU] Creating command queue...\n");
        std::cout.flush();
#if defined(CL_VERSION_2_0)
        const cl_queue_properties props[] = {0};
        queue = clCreateCommandQueueWithProperties(context, device, props, &err);
        if (err == CL_SUCCESS) {
            tfm::format(std::cout, "[GPU] Using OpenCL 2.0+ command queue\n");
        }
#else
        queue = clCreateCommandQueue(context, device, 0, &err);
        if (err == CL_SUCCESS) {
            tfm::format(std::cout, "[GPU] Using OpenCL 1.x command queue\n");
        }
#endif
        if (err != CL_SUCCESS) {
            tfm::format(std::cout, "[GPU] ERROR: Failed to create command queue (error: %d)\n", err);
            std::cout.flush();
            return false;
        }
        std::cout.flush();
        
        // Create buffers
        tfm::format(std::cout, "[GPU] Creating OpenCL buffers...\n");
        std::cout.flush();
        header_buffer = clCreateBuffer(context, CL_MEM_READ_ONLY, 80, nullptr, &err);
        if (err != CL_SUCCESS) {
            tfm::format(std::cout, "[GPU] ERROR: Failed to create header buffer (error: %d)\n", err);
            std::cout.flush();
            return false;
        }
        
        nonce_buffer = clCreateBuffer(context, CL_MEM_READ_WRITE, 4, nullptr, &err);
        if (err != CL_SUCCESS) {
            tfm::format(std::cout, "[GPU] ERROR: Failed to create nonce buffer (error: %d)\n", err);
            std::cout.flush();
            return false;
        }
        
        result_buffer = clCreateBuffer(context, CL_MEM_WRITE_ONLY, 32, nullptr, &err);
        if (err != CL_SUCCESS) {
            tfm::format(std::cout, "[GPU] ERROR: Failed to create result buffer (error: %d)\n", err);
            std::cout.flush();
            return false;
        }
        
        target_buffer = clCreateBuffer(context, CL_MEM_READ_ONLY, 32, nullptr, &err);
        if (err != CL_SUCCESS) {
            tfm::format(std::cout, "[GPU] ERROR: Failed to create target buffer (error: %d)\n", err);
            std::cout.flush();
            return false;
        }
        
        found_flag_buffer = clCreateBuffer(context, CL_MEM_READ_WRITE, 4, nullptr, &err);
        if (err != CL_SUCCESS) {
            tfm::format(std::cout, "[GPU] ERROR: Failed to create found flag buffer (error: %d)\n", err);
            std::cout.flush();
            return false;
        }
        
        found_nonce_buffer = clCreateBuffer(context, CL_MEM_READ_WRITE, 4, nullptr, &err);
        if (err != CL_SUCCESS) {
            tfm::format(std::cout, "[GPU] ERROR: Failed to create found nonce buffer (error: %d)\n", err);
            std::cout.flush();
            return false;
        }
        tfm::format(std::cout, "[GPU] All buffers created successfully\n");
        std::cout.flush();
        
        // Load kernel from file
        tfm::format(std::cout, "[GPU] Loading OpenCL kernel...\n");
        std::cout.flush();
        // Try multiple possible kernel paths
        std::vector<std::string> kernel_paths = {
            "randomq_kernel.cl",                     // From current directory
            "src/tools/randomq_kernel.cl",           // From project root
            "../src/tools/randomq_kernel.cl",        // From build directory
            "../../src/tools/randomq_kernel.cl",     // From nested build directory
            "./src/tools/randomq_kernel.cl",         // Explicit relative path
            "../bitquan/src/tools/randomq_kernel.cl" // Alternative build path
        };
        
        std::string kernel_path;
        bool kernel_found = false;
        
        // Try to find the kernel file
        for (const auto& path : kernel_paths) {
            std::ifstream test_file(path);
            if (test_file.is_open()) {
                kernel_path = path;
                kernel_found = true;
                test_file.close();
                tfm::format(std::cout, "[GPU] Found kernel at: %s\n", path.c_str());
                break;
            } else {
                tfm::format(std::cout, "[GPU] Kernel not found at: %s\n", path.c_str());
            }
        }
        
        // If not found, try to copy from source directory
        if (!kernel_found) {
            std::string source_kernel = "src/tools/randomq_kernel.cl";
            std::string dest_kernel = "randomq_kernel.cl";
            
            std::ifstream src(source_kernel, std::ios::binary);
            if (src.is_open()) {
                std::ofstream dst(dest_kernel, std::ios::binary);
                if (dst.is_open()) {
                    dst << src.rdbuf();
                    src.close();
                    dst.close();
                    
                    // Try again with the copied file
                    std::ifstream test_copied(dest_kernel);
                    if (test_copied.is_open()) {
                        kernel_path = dest_kernel;
                        kernel_found = true;
                        test_copied.close();
                        tfm::format(std::cout, "[GPU] Copied and found kernel at: %s\n", dest_kernel.c_str());
                    }
                } else {
                    tfm::format(std::cout, "[GPU] Failed to create kernel copy at: %s\n", dest_kernel.c_str());
                }
            } else {
                tfm::format(std::cout, "[GPU] Source kernel not found at: %s\n", source_kernel.c_str());
            }
        }
        
        if (!kernel_found) {
            tfm::format(std::cout, "[GPU] Kernel file not found in any of the following paths:\n");
            for (const auto& path : kernel_paths) {
                tfm::format(std::cout, "[GPU]   - %s\n", path.c_str());
            }
            tfm::format(std::cout, "[GPU] Using embedded fallback kernel (WARNING: This is NOT the real RandomQ algorithm!)\n");
            std::cout.flush();
            // Fallback to embedded REAL RandomQ kernel
            tfm::format(std::cout, "[GPU] Using embedded REAL RandomQ kernel (full SHA256+RandomQ+SHA256 pipeline)\n");
            std::cout.flush();
            const char* kernel_source = R"(
// Full RandomQ OpenCL Kernel - Embedded Version
#pragma OPENCL EXTENSION cl_khr_int64_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable

typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned char uchar;

// RandomQ constants (from CPU)
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

// SHA256 utilities
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

// SHA256 implementation
void sha256_general(const __private uchar* msg, uint len, __private uchar out32[32]) {
    __private uchar tmp[256];
    for (int i = 0; i < 256; ++i) tmp[i] = 0;
    for (uint i = 0; i < len; ++i) tmp[i] = msg[i];
    tmp[len] = (uchar)0x80;
    ulong bitlen = ((ulong)len) * 8UL;
    uint blocks = (uint)((len + 9 + 63) / 64);
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
        for (int t = 0; t < 16; ++t) {
            uint base = block*64 + t*4;
            uint w = ((uint)tmp[base + 0] << 24) | ((uint)tmp[base + 1] << 16) |
                     ((uint)tmp[base + 2] << 8) | ((uint)tmp[base + 3] << 0);
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

    out32[0] = (uchar)((h0 >> 24) & 0xFF);  out32[1] = (uchar)((h0 >> 16) & 0xFF);
    out32[2] = (uchar)((h0 >> 8) & 0xFF);   out32[3] = (uchar)((h0 >> 0) & 0xFF);
    out32[4] = (uchar)((h1 >> 24) & 0xFF);  out32[5] = (uchar)((h1 >> 16) & 0xFF);
    out32[6] = (uchar)((h1 >> 8) & 0xFF);   out32[7] = (uchar)((h1 >> 0) & 0xFF);
    out32[8] = (uchar)((h2 >> 24) & 0xFF);  out32[9] = (uchar)((h2 >> 16) & 0xFF);
    out32[10] = (uchar)((h2 >> 8) & 0xFF);  out32[11] = (uchar)((h2 >> 0) & 0xFF);
    out32[12] = (uchar)((h3 >> 24) & 0xFF); out32[13] = (uchar)((h3 >> 16) & 0xFF);
    out32[14] = (uchar)((h3 >> 8) & 0xFF);  out32[15] = (uchar)((h3 >> 0) & 0xFF);
    out32[16] = (uchar)((h4 >> 24) & 0xFF); out32[17] = (uchar)((h4 >> 16) & 0xFF);
    out32[18] = (uchar)((h4 >> 8) & 0xFF);  out32[19] = (uchar)((h4 >> 0) & 0xFF);
    out32[20] = (uchar)((h5 >> 24) & 0xFF); out32[21] = (uchar)((h5 >> 16) & 0xFF);
    out32[22] = (uchar)((h5 >> 8) & 0xFF);  out32[23] = (uchar)((h5 >> 0) & 0xFF);
    out32[24] = (uchar)((h6 >> 24) & 0xFF); out32[25] = (uchar)((h6 >> 16) & 0xFF);
    out32[26] = (uchar)((h6 >> 8) & 0xFF);  out32[27] = (uchar)((h6 >> 0) & 0xFF);
    out32[28] = (uchar)((h7 >> 24) & 0xFF); out32[29] = (uchar)((h7 >> 16) & 0xFF);
    out32[30] = (uchar)((h7 >> 8) & 0xFF);  out32[31] = (uchar)((h7 >> 0) & 0xFF);
}

// CRandomQ implementation
typedef struct {
    ulong state[25];
    ulong nonce;
    ulong rounds;
} CRANDOMQ_CTX;

inline void CRandomQ_Reset(__private CRANDOMQ_CTX* ctx) {
    for (int i = 0; i < 25; ++i) ctx->state[i] = (ulong)0;
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
    for (int i = 0; i < 25; ++i) {
        ulong v = ctx->state[i];
        ulong rotated = (v << 13) | (v >> (64 - 13));
        ulong next = ctx->state[(i + 1) % 25];
        ctx->state[i] = rotated ^ next ^ (v + next);
        ctx->state[i] += RANDOMQ_CONSTANTS[i];
    }
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
        uint words = chunk_size / 8;
        if (words > 8) words = 8;
        for (uint i = 0; i < words; ++i) {
            ulong chunk = (ulong)0;
            for (uint j = 0; j < 8; ++j) {
                uint idx = offset + i*8 + j;
                uchar b = (idx < len) ? input[idx] : (uchar)0;
                chunk |= ((ulong)b) << (j * 8);
            }
            ctx->state[i] ^= chunk;
        }
        CRandomQ_RandomQRound(ctx);
        offset += chunk_size;
    }
}

inline void CRandomQ_StateToHash(__private CRANDOMQ_CTX* ctx, __private uchar out[32]) {
    __private uchar tmp[200];
    for (int i = 0; i < 25; ++i) {
        ulong v = ctx->state[i];
        for (int j = 0; j < 8; ++j) {
            tmp[i*8 + j] = (uchar)((v >> (j * 8)) & 0xFF);
        }
    }
    sha256_general(tmp, 200u, out);
}

inline void CRandomQ_Finalize(__private CRANDOMQ_CTX* ctx, __private uchar out[32]) {
    ctx->state[0] ^= (ulong)ctx->nonce;
    for (ulong i = 0; i < ctx->rounds; ++i) {
        CRandomQ_RandomQRound(ctx);
    }
    CRandomQ_StateToHash(ctx, out);
}

// Mining kernel
__kernel void randomq_mining(
    __global const uchar* header80,
    __global const uint* nonce_base,
    __global const uchar* target32,
    __global volatile uint* found_flag,
    __global uint* found_nonce,
    __global uchar* result_hash
) {
    uint gid = get_global_id(0);
    uint base = *nonce_base;
    ulong current_nonce = (ulong)(base + gid);

    __private uchar local_header[80];
    for (int i = 0; i < 80; ++i) local_header[i] = header80[i];
    local_header[76] = (uchar)((current_nonce) & 0xFF);
    local_header[77] = (uchar)((current_nonce >> 8) & 0xFF);
    local_header[78] = (uchar)((current_nonce >> 16) & 0xFF);
    local_header[79] = (uchar)((current_nonce >> 24) & 0xFF);

    __private uchar first_sha[32];
    sha256_general(local_header, 80u, first_sha);

    CRANDOMQ_CTX ctx;
    CRandomQ_Reset(&ctx);
    CRandomQ_SetRounds(&ctx, (ulong)8192);
    CRandomQ_SetNonce(&ctx, current_nonce);
    CRandomQ_Write(&ctx, first_sha, 32u);
    __private uchar randomq_out[32];
    CRandomQ_Finalize(&ctx, randomq_out);

    __private uchar final32[32];
    sha256_general(randomq_out, 32u, final32);

    bool meets_target = true;
    for (int i = 0; i < 32; ++i) {
        uchar hb = final32[i];
        uchar tb = target32[i];
        if (hb > tb) { meets_target = false; break; }
        else if (hb < tb) break;
    }

    if (meets_target) {
        uint old = atomic_cmpxchg((volatile __global uint*)found_flag, 0u, 1u);
        if (old == 0u) {
            *found_nonce = (uint)current_nonce;
            for (int i = 0; i < 32; ++i) result_hash[i] = final32[i];
        }
    }
}
            )";
            program = clCreateProgramWithSource(context, 1, &kernel_source, nullptr, &err);
        } else {
            tfm::format(std::cout, "[GPU] Loading kernel from file: %s\n", kernel_path.c_str());
            std::cout.flush();
            std::ifstream kernel_file(kernel_path);
            std::string kernel_source((std::istreambuf_iterator<char>(kernel_file)),
                                    std::istreambuf_iterator<char>());
            kernel_file.close();
            const char* source_str = kernel_source.c_str();
            program = clCreateProgramWithSource(context, 1, &source_str, nullptr, &err);
        }
        if (err != CL_SUCCESS) {
            tfm::format(std::cout, "[GPU] ERROR: Failed to create program from source (error: %d)\n", err);
            std::cout.flush();
            return false;
        }
        
        tfm::format(std::cout, "[GPU] Compiling OpenCL kernel...\n");
        std::cout.flush();
        err = clBuildProgram(program, 1, &device, nullptr, nullptr, nullptr);
        if (err != CL_SUCCESS) {
            // Get build log
            size_t log_size;
            clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &log_size);
            std::vector<char> log(log_size);
            clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size, log.data(), nullptr);
            tfm::format(std::cout, "[GPU] ERROR: Kernel compilation failed (error: %d)\n", err);
            tfm::format(std::cout, "[GPU] Build log: %s\n", log.data());
            std::cout.flush();
            return false;
        }
        
        tfm::format(std::cout, "[GPU] Creating kernel object...\n");
        std::cout.flush();
        kernel = clCreateKernel(program, "randomq_mining", &err);
        if (err != CL_SUCCESS) {
            tfm::format(std::cout, "[GPU] ERROR: Failed to create kernel (error: %d)\n", err);
            std::cout.flush();
            return false;
        }
        
        tfm::format(std::cout, "[GPU] OpenCL initialization completed successfully!\n");
        std::cout.flush();
        initialized = true;
        return true;
#else
        return false;
#endif
    }
    
    void Cleanup() {
        std::lock_guard<std::mutex> lock(opencl_mutex);
        if (!initialized) return;
        
#ifdef OPENCL_FOUND
        if (kernel) clReleaseKernel(kernel);
        if (program) clReleaseProgram(program);
        if (header_buffer) clReleaseMemObject(header_buffer);
        if (nonce_buffer) clReleaseMemObject(nonce_buffer);
        if (result_buffer) clReleaseMemObject(result_buffer);
        if (target_buffer) clReleaseMemObject(target_buffer);
        if (found_flag_buffer) clReleaseMemObject(found_flag_buffer);
        if (found_nonce_buffer) clReleaseMemObject(found_nonce_buffer);
        if (queue) clReleaseCommandQueue(queue);
        if (context) clReleaseContext(context);
#endif
        
        initialized = false;
    }
    
    bool MineNonce(const CBlockHeader& block, uint32_t start_nonce, uint32_t& found_nonce, 
                   const arith_uint256& target, size_t work_size = 1024) {
        if (!initialized) return false;
        
#ifdef OPENCL_FOUND
        std::lock_guard<std::mutex> lock(opencl_mutex);
        
        cl_int err;
        
        // Serialize block header
        std::vector<unsigned char> serialized;
        VectorWriter(serialized, 0, block);
        std::vector<unsigned char> header_data(80);
        memcpy(header_data.data(), serialized.data(), 80);
        
        // Write header to buffer
        err = clEnqueueWriteBuffer(queue, header_buffer, CL_TRUE, 0, 80, 
                                  header_data.data(), 0, nullptr, nullptr);
        if (err != CL_SUCCESS) return false;
        
        // Write start nonce
        err = clEnqueueWriteBuffer(queue, nonce_buffer, CL_TRUE, 0, 4, 
                                  &start_nonce, 0, nullptr, nullptr);
        if (err != CL_SUCCESS) return false;
        
        // Write target (convert to big-endian for GPU comparison)
        uint256 target_uint = ArithToUint256(target);
        std::vector<unsigned char> target_bytes(32);
        memcpy(target_bytes.data(), target_uint.begin(), 32);
        std::reverse(target_bytes.begin(), target_bytes.end()); // Convert to big-endian
        err = clEnqueueWriteBuffer(queue, target_buffer, CL_TRUE, 0, 32, 
                                  target_bytes.data(), 0, nullptr, nullptr);
        if (err != CL_SUCCESS) return false;
        
        // Initialize found flag
        uint32_t found_flag = 0;
        err = clEnqueueWriteBuffer(queue, found_flag_buffer, CL_TRUE, 0, 4, 
                                  &found_flag, 0, nullptr, nullptr);
        if (err != CL_SUCCESS) return false;
        
        // Set kernel arguments (matching randomq_kernel.cl parameter order)
        err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &header_buffer);      // header80
        if (err != CL_SUCCESS) return false;
        err = clSetKernelArg(kernel, 1, sizeof(cl_mem), &nonce_buffer);       // nonce_base
        if (err != CL_SUCCESS) return false;
        err = clSetKernelArg(kernel, 2, sizeof(cl_mem), &target_buffer);      // target32
        if (err != CL_SUCCESS) return false;
        err = clSetKernelArg(kernel, 3, sizeof(cl_mem), &found_flag_buffer);  // found_flag
        if (err != CL_SUCCESS) return false;
        err = clSetKernelArg(kernel, 4, sizeof(cl_mem), &found_nonce_buffer); // found_nonce
        if (err != CL_SUCCESS) return false;
        err = clSetKernelArg(kernel, 5, sizeof(cl_mem), &result_buffer);      // result_hash
        if (err != CL_SUCCESS) return false;
        
        // Execute kernel
        size_t global_work_size = work_size;
        err = clEnqueueNDRangeKernel(queue, kernel, 1, nullptr, &global_work_size, 
                                    nullptr, 0, nullptr, nullptr);
        if (err != CL_SUCCESS) return false;
        
        // Read found flag and nonce
        uint32_t found_flag_result = 0;
        uint32_t found_nonce_result = 0;
        err = clEnqueueReadBuffer(queue, found_flag_buffer, CL_TRUE, 0, 4, 
                                 &found_flag_result, 0, nullptr, nullptr);
        if (err != CL_SUCCESS) return false;
        
        if (found_flag_result != 0) {
            err = clEnqueueReadBuffer(queue, found_nonce_buffer, CL_TRUE, 0, 4, 
                                     &found_nonce_result, 0, nullptr, nullptr);
            if (err != CL_SUCCESS) return false;
            found_nonce = found_nonce_result;
            return true;
        }
        
        return false;
#else
        return false;
#endif
    }
}

static const char DEFAULT_RPCCONNECT[] = "127.0.0.1";
static const int DEFAULT_HTTP_CLIENT_TIMEOUT = 900;

static std::atomic<bool> g_stop{false};

static void SetupMinerArgs(ArgsManager& argsman)
{
	const auto defaultBaseParams = CreateBaseChainParams(ChainType::MAIN);
	const auto testnetBaseParams = CreateBaseChainParams(ChainType::TESTNET);
	const auto testnet4BaseParams = CreateBaseChainParams(ChainType::TESTNET4);
	const auto signetBaseParams = CreateBaseChainParams(ChainType::SIGNET);
	const auto regtestBaseParams = CreateBaseChainParams(ChainType::REGTEST);

	argsman.AddArg("-rpcconnect=<ip>", strprintf("Send RPCs to node at <ip> (default: %s)", DEFAULT_RPCCONNECT), ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
	argsman.AddArg("-rpcport=<port>", strprintf("RPC port (default: %u, testnet: %u, testnet4: %u, signet: %u, regtest: %u)", defaultBaseParams->RPCPort(), testnetBaseParams->RPCPort(), testnet4BaseParams->RPCPort(), signetBaseParams->RPCPort(), regtestBaseParams->RPCPort()), ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
	argsman.AddArg("-rpcuser=<user>", "RPC username", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
	argsman.AddArg("-rpcpassword=<pw>", "RPC password (omit to use cookie)", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
	argsman.AddArg("-rpccookiefile=<loc>", "RPC cookie file (defaults to datadir)", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
	argsman.AddArg("-rpcwait", "Wait for RPC server to be ready", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
	argsman.AddArg("-rpcwaittimeout=<n>", "Timeout in seconds to wait for RPC server (0 = forever)", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
	argsman.AddArg("-gpu", "Use GPU mining (OpenCL)", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
	argsman.AddArg("-worksize=<n>", "GPU work size (default: 1024)", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
	argsman.AddArg("-address=<bech32>", "Payout address for coinbase", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
	argsman.AddArg("-maxtries=<n>", "Max nonce attempts before refreshing template (default: 1000000)", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
}

// Minimal RPC client modeled after bitquantum-cli
namespace {

struct HTTPReply {
	int status = 0;
	std::string body;
};

static void http_request_done(struct evhttp_request* req, void* ctx)
{
	HTTPReply* reply = static_cast<HTTPReply*>(ctx);
	if (!req) {
		reply->status = 0;
		reply->body = "";
		return;
	}
	reply->status = evhttp_request_get_response_code(req);
	struct evbuffer* buf = evhttp_request_get_input_buffer(req);
	if (buf) {
		size_t len = evbuffer_get_length(buf);
		reply->body.assign((const char*)evbuffer_pullup(buf, len), len);
	}
}

static std::string GetAuth()
{
	if (!gArgs.GetArg("-rpcpassword", "").empty()) {
		return gArgs.GetArg("-rpcuser", "") + ":" + gArgs.GetArg("-rpcpassword", "");
	}
	std::string userpass;
	if (!GetAuthCookie(&userpass)) return std::string();
	return userpass;
}

static UniValue DoRpcRequest(const std::string& strMethod, const UniValue& params)
{
	std::string host = gArgs.GetArg("-rpcconnect", DEFAULT_RPCCONNECT);
	int port = gArgs.GetIntArg("-rpcport", -1);
	if (port == -1) {
		const auto defaultBaseParams = CreateBaseChainParams(ChainType::MAIN);
		port = defaultBaseParams->RPCPort();
	}
	
	std::string path = "/";
	std::string rpcuser = gArgs.GetArg("-rpcuser", "");
	std::string rpcpassword = gArgs.GetArg("-rpcpassword", "");
	
	raii_event_base base = obtain_event_base();
	raii_evhttp_connection conn = obtain_evhttp_connection_base(base.get(), host.c_str(), port);
	HTTPReply http_reply;
	raii_evhttp_request req = obtain_evhttp_request(http_request_done, (void*)&http_reply);
	
	if (req == nullptr) throw std::runtime_error("create http request failed");
	
	struct evkeyvalq* output_headers = evhttp_request_get_output_headers(req.get());
	assert(output_headers != nullptr);
	evhttp_add_header(output_headers, "Host", host.c_str());
	evhttp_add_header(output_headers, "Connection", "close");
	evhttp_add_header(output_headers, "Content-Type", "application/json");
	
	std::string strRequest = JSONRPCRequestObj(strMethod, params, 1).write() + "\n";
	struct evbuffer* output_buffer = evhttp_request_get_output_buffer(req.get());
	assert(output_buffer != nullptr);
	evbuffer_add(output_buffer, strRequest.data(), strRequest.size());
	
	// Authorization: prefer explicit user/pass, fallback to cookie
	std::string auth_pair;
	if (!rpcuser.empty() && !rpcpassword.empty()) {
		auth_pair = rpcuser + ":" + rpcpassword;
	} else {
		auth_pair = GetAuth();
	}
	if (!auth_pair.empty()) {
		std::string auth_header = std::string("Basic ") + EncodeBase64(auth_pair);
		evhttp_add_header(output_headers, "Authorization", auth_header.c_str());
	}
	
	int r = evhttp_make_request(conn.get(), req.get(), EVHTTP_REQ_POST, path.c_str());
	req.release();
	if (r != 0) throw std::runtime_error("send http request failed");
	
	event_base_dispatch(base.get());
	
	// Use numeric codes to avoid macro conflicts between libevent and internal headers
	if (http_reply.status == 0) throw std::runtime_error("couldn't connect to server");
	else if (http_reply.status == 401) throw std::runtime_error("incorrect rpcuser or rpcpassword");
	else if (http_reply.status >= 400 && http_reply.status != 400 && http_reply.status != 404 && http_reply.status != 500) throw std::runtime_error(strprintf("server returned HTTP error %d", http_reply.status));
	else if (http_reply.body.empty()) throw std::runtime_error("no response from server");
	
	UniValue valReply(UniValue::VSTR);
	if (!valReply.read(http_reply.body)) throw std::runtime_error("couldn't parse reply from server");
	const UniValue& reply_obj = valReply.get_obj();
	if (reply_obj.empty()) throw std::runtime_error("expected reply to have result, error and id properties");
	
	return reply_obj;
}

static UniValue RpcCall(const std::string& strMethod, const std::vector<std::string>& params)
{
	UniValue params_arr(UniValue::VARR);
	for (const auto& param : params) {
		params_arr.push_back(param);
	}
	return DoRpcRequest(strMethod, params_arr);
}

static UniValue RpcCallWait(const std::string& strMethod, const std::vector<std::string>& params)
{
	UniValue reply;
	int timeout = gArgs.GetIntArg("-rpcwaittimeout", DEFAULT_HTTP_CLIENT_TIMEOUT);
	
	if (timeout <= 0) timeout = DEFAULT_HTTP_CLIENT_TIMEOUT;
	
	reply = RpcCall(strMethod, params);
	return reply;
}

static UniValue RpcCallWaitParams(const std::string& strMethod, const UniValue& params)
{
	UniValue reply;
	int timeout = gArgs.GetIntArg("-rpcwaittimeout", DEFAULT_HTTP_CLIENT_TIMEOUT);
	
	if (timeout <= 0) timeout = DEFAULT_HTTP_CLIENT_TIMEOUT;
	
	reply = DoRpcRequest(strMethod, params);
	return reply;
}

} // namespace

// Block building functions (same as cpuminer-randomq)
static bool BuildBlockFromGBT(const UniValue& gbt_res, CBlock& block, std::string& tmpl_hex)
{
	// Check for hex template first
	const UniValue hex = gbt_res.find_value("hex");
	if (!hex.isNull() && hex.isStr()) {
		tmpl_hex = hex.get_str();
		if (!DecodeHexBlk(block, tmpl_hex)) {
			throw std::runtime_error("failed to decode hex template");
		}
		return true;
	}
	
	// Build block from template fields
	block.nVersion = gbt_res["version"].getInt<int>();
	{
		auto prev_opt = uint256::FromHex(gbt_res["previousblockhash"].get_str());
		if (!prev_opt) throw std::runtime_error("invalid previousblockhash hex");
		block.hashPrevBlock = *prev_opt;
	}
	block.nTime = gbt_res["curtime"].getInt<uint32_t>();
	block.nBits = strtoul(gbt_res["bits"].get_str().c_str(), nullptr, 16);
	block.nNonce = 0;
	
	bool built_local_coinbase = false;
	
	// Check for coinbasetxn first
	const UniValue coinbasetxn = gbt_res.find_value("coinbasetxn");
	if (coinbasetxn.isObject() && coinbasetxn.exists("data") && coinbasetxn.find_value("data").isStr()) {
		CMutableTransaction mtx;
		if (!DecodeHexTx(mtx, coinbasetxn.find_value("data").get_str())) {
			throw std::runtime_error("failed to decode coinbase txn from template");
		}
		block.vtx.push_back(MakeTransactionRef(std::move(mtx)));
	} else {
		// Build coinbase locally
		CAmount cb_value = 0;
		if (!gbt_res["coinbasevalue"].isNull()) {
			cb_value = gbt_res["coinbasevalue"].getInt<int64_t>();
		}
		if (gbt_res["height"].isNull()) throw std::runtime_error("GBT missing height for local coinbase");
		int32_t height = gbt_res["height"].getInt<int>();
		CMutableTransaction coinbase;
		coinbase.version = 1;
		// scriptSig: BIP34 height (following BIP34 rules exactly)
		CScript sig;
		if (height <= 16) {
			// For height <= 16, use OP_N encoding + OP_0 dummy (BIP34 rule)
			sig << (height == 0 ? OP_0 : (opcodetype)(OP_1 + height - 1));
			sig << OP_0; // dummy to make scriptSig size 2
		} else {
			// For height > 16, use CScriptNum encoding
			sig << CScriptNum(height);
		}
		coinbase.vin.emplace_back(CTxIn(COutPoint(), sig));
		// Debug: print scriptSig
		{
			std::vector<unsigned char> ss(sig.begin(), sig.end());
			tfm::format(std::cout, "[CB] height=%d scriptSig=%s\n", height, HexStr(ss).c_str());
			std::cout.flush();
		}
		// payout
		{
			const std::string addr_str = gArgs.GetArg("-address", "");
			CTxDestination dest = DecodeDestination(addr_str);
			if (!IsValidDestination(dest)) throw std::runtime_error("invalid mining address for coinbase");
			CScript payout = GetScriptForDestination(dest);
			coinbase.vout.emplace_back(CTxOut(cb_value, payout));
		}
		// Don't add witness data here - we'll add it later if we have witness commitment
		block.vtx.push_back(MakeTransactionRef(std::move(coinbase)));
		built_local_coinbase = true;
	}
	// other transactions
	const UniValue txs = gbt_res.find_value("transactions");
	if (txs.isArray()) {
		for (size_t i = 0; i < txs.size(); ++i) {
			const UniValue& txo = txs[i];
			if (!txo.isObject()) continue;
			const UniValue data = txo.find_value("data");
			if (!data.isStr()) continue;
			CMutableTransaction mtx;
			if (!DecodeHexTx(mtx, data.get_str())) throw std::runtime_error("failed to decode tx from template");
			block.vtx.push_back(MakeTransactionRef(std::move(mtx)));
		}
	}
	// Handle witness commitment if present (following bitquantum-cli logic exactly)
	if (built_local_coinbase) {
		const UniValue commit = gbt_res.find_value("default_witness_commitment");
		bool has_witness_commitment = !commit.isNull() && !block.vtx.empty();
		
		// Check if any transaction has witness data
		bool has_witness_data = false;
		for (const auto& tx : block.vtx) {
			for (const auto& vin : tx->vin) {
				if (!vin.scriptWitness.IsNull()) {
					has_witness_data = true;
					break;
				}
			}
			if (has_witness_data) break;
		}
		
		if (has_witness_commitment) {
			tfm::format(std::cout, "[CB] Found witness commitment, adding witness data\n");
			std::cout.flush();
			
			// First, set witness reserved value for coinbase
			CMutableTransaction coinbase_mtx;
			coinbase_mtx.version = block.vtx[0]->version;
			coinbase_mtx.vin = block.vtx[0]->vin;
			coinbase_mtx.vout = block.vtx[0]->vout;
			coinbase_mtx.nLockTime = block.vtx[0]->nLockTime;
			
			// Set witness reserved value for coinbase BEFORE calculating witness merkle root
			if (!coinbase_mtx.vin.empty()) {
				coinbase_mtx.vin[0].scriptWitness.stack.resize(1);
				coinbase_mtx.vin[0].scriptWitness.stack[0].resize(32, 0); // 32 bytes of zeros
			}
			
			// Update block with coinbase that has witness data
			block.vtx[0] = MakeTransactionRef(std::move(coinbase_mtx));
			
			// Now calculate witness merkle root with proper coinbase witness
			uint256 witness_merkle_root = BlockWitnessMerkleRoot(block);
			
			// Create witness commitment according to BIP141
			// The commitment is: SHA256(witness_merkle_root || witness_merkle_root)
			uint256 commitment = Hash(witness_merkle_root, witness_merkle_root);
			
			// Create witness commitment output with proper BIP141 format
			// Format: OP_RETURN <36-byte commitment>
			// The commitment should be: 0x6a24aa21a9ed + 32-byte commitment
			std::vector<unsigned char> commitment_data;
			commitment_data.push_back(0x6a); // OP_RETURN
			commitment_data.push_back(0x24); // 36 bytes
			commitment_data.push_back(0xaa); // witness commitment marker
			commitment_data.push_back(0x21); // 33 bytes  
			commitment_data.push_back(0xa9); // witness commitment marker
			commitment_data.push_back(0xed); // witness commitment marker
			// Add the 32-byte commitment
			commitment_data.insert(commitment_data.end(), commitment.begin(), commitment.end());
			
			CScript opret(commitment_data.begin(), commitment_data.end());
			
			// Add witness commitment to coinbase
			CMutableTransaction final_coinbase_mtx;
			final_coinbase_mtx.version = block.vtx[0]->version;
			final_coinbase_mtx.vin = block.vtx[0]->vin;
			final_coinbase_mtx.vout = block.vtx[0]->vout;
			final_coinbase_mtx.nLockTime = block.vtx[0]->nLockTime;
			
			final_coinbase_mtx.vout.emplace_back(CTxOut(0, opret));
			
			// Keep witness data from previous step
			if (!final_coinbase_mtx.vin.empty()) {
				final_coinbase_mtx.vin[0].scriptWitness.stack.resize(1);
				final_coinbase_mtx.vin[0].scriptWitness.stack[0].resize(32, 0); // 32 bytes of zeros
			}
			
			block.vtx[0] = MakeTransactionRef(std::move(final_coinbase_mtx));
		} else if (has_witness_data) {
			// If there's witness data but no witness commitment, remove witness data
			tfm::format(std::cout, "[CB] Found witness data but no witness commitment. Removing witness data.\n");
			std::cout.flush();
			
			// Create new block without witness data
			CBlock new_block;
			new_block.nVersion = block.nVersion;
			new_block.hashPrevBlock = block.hashPrevBlock;
			new_block.hashMerkleRoot = block.hashMerkleRoot;
			new_block.nTime = block.nTime;
			new_block.nBits = block.nBits;
			new_block.nNonce = block.nNonce;
			
			for (const auto& tx : block.vtx) {
				CMutableTransaction mtx;
				mtx.version = tx->version;
				mtx.vin = tx->vin;
				mtx.vout = tx->vout;
				mtx.nLockTime = tx->nLockTime;
				// Don't copy witness data
				new_block.vtx.push_back(MakeTransactionRef(std::move(mtx)));
			}
			
			block = new_block;
		} else {
			tfm::format(std::cout, "[CB] No witness commitment found (non-segwit block)\n");
			std::cout.flush();
		}
	}
	// Finalize merkle root
	block.hashMerkleRoot = BlockMerkleRoot(block);
	return true;
}

static std::string UpdateNonceInBlockHex(const std::string& tmpl_hex, uint32_t nonce)
{
	// Block header is 80 bytes; nonce at bytes 76..79 (little-endian)
	if (tmpl_hex.size() < 160) throw std::runtime_error("template hex too short");
	std::string out = tmpl_hex;
	const size_t off = 76 * 2; // hex chars offset
	unsigned char b0 = (unsigned char)(nonce & 0xFF);
	unsigned char b1 = (unsigned char)((nonce >> 8) & 0xFF);
	unsigned char b2 = (unsigned char)((nonce >> 16) & 0xFF);
	unsigned char b3 = (unsigned char)((nonce >> 24) & 0xFF);
	auto write_byte = [&](size_t pos, unsigned char b){
		static const char* hexd = "0123456789abcdef";
		out[pos+0] = hexd[(b >> 4) & 0xF];
		out[pos+1] = hexd[b & 0xF];
	};
	write_byte(off + 0, b0);
	write_byte(off + 2, b1);
	write_byte(off + 4, b2);
	write_byte(off + 6, b3);
	return out;
}

static std::string BuildFullBlockHex(const CBlock& block)
{
	std::vector<unsigned char> bytes;
	// Start with header
	VectorWriter vw(bytes, 0, static_cast<const CBlockHeader&>(block));
	// Varint count using the same stream
	WriteCompactSize(vw, block.vtx.size());
	// Append each tx raw bytes
	for (const auto& txref : block.vtx) {
		const std::string tx_hex = EncodeHexTx(*txref);
		std::vector<unsigned char> tx_bytes = ParseHex(tx_hex);
		vw.write(std::as_bytes(std::span<const unsigned char>(tx_bytes.data(), tx_bytes.size())));
	}
	return HexStr(bytes);
}

static void MinerLoop()
{
	const std::string payout = gArgs.GetArg("-address", "");
	if (payout.empty()) {
		throw std::runtime_error("-address is required");
	}

	const int maxtries = gArgs.GetIntArg("-maxtries", 1000000);
	const bool use_gpu = gArgs.GetBoolArg("-gpu", false);
	const size_t work_size = gArgs.GetIntArg("-worksize", 1024);
	
	if (use_gpu) {
		if (!OpenCLMining::Initialize()) {
			throw std::runtime_error("Failed to initialize OpenCL GPU mining");
		}
		tfm::format(std::cout, "[GPU] OpenCL initialized, work size: %zu\n", work_size);
		std::cout.flush();
	}

	std::atomic<uint64_t> total_hashes{0};
	std::atomic<uint64_t> window_hashes{0};
	uint64_t start_time = (uint64_t)GetTime();
	std::thread reporter([&](){
		while (!g_stop.load()) {
			uint64_t now = (uint64_t)GetTime();
			uint64_t elapsed = now - start_time;
			double avg = elapsed ? (double)total_hashes.load() / (double)elapsed : 0.0;
			double cur = window_hashes.exchange(0) / 5.0;
			tfm::format(std::cout, "[HashRate] Current: %.2f H/s | Average: %.2f H/s | Total: %llu\n", cur, avg, (unsigned long long)total_hashes.load());
			std::cout.flush();
			for (int i = 0; i < 5 && !g_stop.load(); ++i) std::this_thread::sleep_for(1s);
		}
	});

	try {
	while (!g_stop.load()) {
		// getblocktemplate (object param with rules)
		UniValue rules(UniValue::VARR); rules.push_back("segwit");
		UniValue caps(UniValue::VARR); caps.push_back("coinbasetxn");
		UniValue req(UniValue::VOBJ);
		req.pushKV("rules", rules);
		req.pushKV("capabilities", caps);
		UniValue params_arr(UniValue::VARR); params_arr.push_back(req);
		UniValue gbt = RpcCallWaitParams("getblocktemplate", params_arr);
		// Debug: print getblocktemplate summary
		{
			const UniValue err0 = gbt.find_value("error");
			const UniValue res0 = gbt.find_value("result");
			if (!err0.isNull()) {
				tfm::format(std::cout, "[GBT] error=%s\n", err0.write().c_str());
			} else if (!res0.isNull()) {
				bool has_hex = !res0.find_value("hex").isNull();
				bool has_cbtx = !res0.find_value("coinbasetxn").isNull();
				int hgt = res0.find_value("height").isNull() ? -1 : res0.find_value("height").getInt<int>();
				std::string bits_s = res0.find_value("bits").isNull() ? "" : res0.find_value("bits").get_str();
				unsigned txs_n = (unsigned)(res0.find_value("transactions").isArray() ? res0.find_value("transactions").size() : 0);
				tfm::format(std::cout, "[GBT] height=%d bits=%s has_hex=%s has_coinbasetxn=%s txs=%u\n",
					hgt,
					bits_s.c_str(),
					has_hex?"true":"false",
					has_cbtx?"true":"false",
					txs_n);
				std::cout.flush();
			}
		}
		const UniValue err = gbt.find_value("error");
		if (!err.isNull()) {
			// Check if it's a connection error that we should retry
			std::string error_msg = err.write();
			if (error_msg.find("not connected") != std::string::npos || 
				error_msg.find("connection") != std::string::npos ||
				error_msg.find("timeout") != std::string::npos) {
				tfm::format(std::cout, "[Info] Node connection lost, retrying in 5 seconds...\n");
				std::cout.flush();
				for (int i = 0; i < 5 && !g_stop.load(); ++i) {
					std::this_thread::sleep_for(1s);
				}
				continue; // Retry the loop
			}
			throw std::runtime_error(error_msg);
		}
		const UniValue res = gbt.find_value("result");
		if (res.isNull()) {
			tfm::format(std::cout, "[Info] GBT returned null, retrying in 5 seconds...\n");
			std::cout.flush();
			for (int i = 0; i < 5 && !g_stop.load(); ++i) {
				std::this_thread::sleep_for(1s);
			}
			continue; // Retry the loop
		}

		CBlock block;
		std::string tmpl_hex;
		if (!BuildBlockFromGBT(res, block, tmpl_hex)) {
			// Fall back to generatetoaddress path for simplicity (server mines)
			UniValue reply = RpcCallWait("generatetoaddress", {"1", payout, std::to_string(maxtries)});
			( void )reply;
			continue;
		}

		block.hashMerkleRoot = BlockMerkleRoot(block);

		// Print template/header info once per template fetch
		{
			int32_t height = res.find_value("height").isNull() ? -1 : res.find_value("height").getInt<int>();
			arith_uint256 target; bool neg=false, of=false; target.SetCompact(block.nBits, &neg, &of);
			tfm::format(std::cout,
				"[Template] height=%d version=%d prev=%s time=%u bits=%08x target=%s txs=%u merkle=%s\n",
				height,
				block.nVersion,
				block.hashPrevBlock.GetHex().c_str(),
				(unsigned)block.nTime,
				(unsigned)block.nBits,
				target.GetHex().c_str(),
				(unsigned)block.vtx.size(),
				block.hashMerkleRoot.GetHex().c_str());
			std::cout.flush();
		}

		std::atomic<bool> found{false};
		uint32_t found_nonce = 0;
		uint32_t start_nonce = block.nNonce;

		if (use_gpu) {
			// GPU mining
			arith_uint256 target; bool neg=false, of=false; target.SetCompact(block.nBits, &neg, &of);
			uint32_t current_nonce = start_nonce;
			
			for (int64_t i = 0; i < maxtries && !g_stop.load() && !found.load(); ++i) {
				uint32_t test_nonce = 0;
				if (OpenCLMining::MineNonce(block, current_nonce, test_nonce, target, work_size)) {
					found_nonce = test_nonce;
					found.store(true);
					break;
				}
				current_nonce += work_size;
				window_hashes.fetch_add(work_size, std::memory_order_relaxed);
				total_hashes.fetch_add(work_size, std::memory_order_relaxed);
				
				if (current_nonce < start_nonce) {
					// overflow, bump time
					uint32_t current_time = static_cast<uint32_t>(GetTime());
					block.nTime = current_time;
				}
			}
		} else {
			// CPU mining (fallback)
			for (int64_t i = 0; i < maxtries && !g_stop.load() && !found.load(); ++i) {
				const uint256 h = RandomQMining::CalculateRandomQHashOptimized(block, block.nNonce);
				window_hashes.fetch_add(1, std::memory_order_relaxed);
				total_hashes.fetch_add(1, std::memory_order_relaxed);
				arith_uint256 target; bool neg=false, of=false; target.SetCompact(block.nBits, &neg, &of);
				if (!neg && !of && target != 0 && UintToArith256(h) <= target) {
					found_nonce = block.nNonce;
					found.store(true);
					break;
				}
				block.nNonce += 1;
				if (block.nNonce < start_nonce) {
					// overflow, bump time
					uint32_t current_time = static_cast<uint32_t>(GetTime());
					block.nTime = current_time;
				}
			}
		}

		if (found.load()) {
			block.nNonce = found_nonce;
			// Print found header info
			{
				const uint256 powhash = RandomQMining::CalculateRandomQHashOptimized(block, block.nNonce);
				arith_uint256 target; bool neg=false, of=false; target.SetCompact(block.nBits, &neg, &of);
				tfm::format(std::cout,
					"[Found] height=%d nonce=%u time=%u bits=%08x target=%s powhash=%s merkle=%s\n",
					res.find_value("height").isNull() ? -1 : res.find_value("height").getInt<int>(),
					(unsigned)block.nNonce,
					(unsigned)block.nTime,
					(unsigned)block.nBits,
					target.GetHex().c_str(),
					powhash.GetHex().c_str(),
					block.hashMerkleRoot.GetHex().c_str());
				std::cout.flush();
			}
			// Submit by patching nonce bytes in template hex or full-encode locally
			std::string sub_hex;
			if (!tmpl_hex.empty()) {
				sub_hex = UpdateNonceInBlockHex(tmpl_hex, block.nNonce);
			} else {
				sub_hex = BuildFullBlockHex(block);
			}
			UniValue sub = RpcCallWait("submitblock", {sub_hex});
			// Print raw submit result (robust)
			{
				const UniValue err = sub.find_value("error");
				const UniValue resv = sub.find_value("result");
				if (!err.isNull()) {
					std::string emsg = err.isObject() && !err.find_value("message").isNull() ? err.find_value("message").get_str() : err.write();
					tfm::format(std::cout, "[Submit] result=%s error=%s\n", resv.isNull() ? "null" : resv.write().c_str(), emsg.c_str());
					// Check if it's a connection error
					if (emsg.find("not connected") != std::string::npos || 
						emsg.find("connection") != std::string::npos ||
						emsg.find("timeout") != std::string::npos) {
						tfm::format(std::cout, "[Info] Submit failed due to connection error, will retry next template\n");
						std::cout.flush();
					}
				} else {
					tfm::format(std::cout, "[Submit] result=%s error=null\n", resv.isNull() ? "null" : resv.write().c_str());
				}
				// Also print full JSON-RPC response for debugging
				tfm::format(std::cout, "[SubmitRaw] %s\n", sub.write().c_str());
				std::cout.flush();
			}
			// Print tip info after submit
			try {
				const UniValue bci = RpcCallWait("getblockchaininfo", {});
				const UniValue err2 = bci.find_value("error");
				if (err2.isNull()) {
					const UniValue res2 = bci.find_value("result");
					if (!res2.isNull() && !res2.find_value("blocks").isNull()) {
						int tip = res2.find_value("blocks").getInt<int>();
						tfm::format(std::cout, "[Submit] tip_height=%d\n", tip);
						std::cout.flush();
					}
				}
			} catch (...) {}
		} else {
			// refresh template
		}
	}
	} catch (const std::exception& e) {
		g_stop.store(true);
		tfm::format(std::cerr, "mining loop error: %s\n", e.what());
	}

	reporter.join();
	if (use_gpu) {
		OpenCLMining::Cleanup();
	}
}

int main(int argc, char* argv[])
{
	// Print startup banner
	tfm::format(std::cout, "=== Bitquantum GPU Miner (RandomQ) ===\n");
	tfm::format(std::cout, "[Startup] Version: 1.0.0\n");
	tfm::format(std::cout, "[Startup] Build: %s\n", "Development Build");
#ifdef WIN32
	tfm::format(std::cout, "[Startup] Platform: Windows\n");
#elif defined(__linux__)
	tfm::format(std::cout, "[Startup] Platform: Linux\n");
#elif defined(__APPLE__)
	tfm::format(std::cout, "[Startup] Platform: macOS\n");
#else
	tfm::format(std::cout, "[Startup] Platform: Unknown\n");
#endif
#ifdef OPENCL_FOUND
	tfm::format(std::cout, "[Startup] OpenCL Support: Available\n");
#else
	tfm::format(std::cout, "[Startup] OpenCL Support: Not Available (CPU only)\n");
#endif
	std::cout.flush();

#ifdef WIN32
	common::WinCmdLineArgs winArgs;
	std::tie(argc, argv) = winArgs.get();
#endif

	tfm::format(std::cout, "[Startup] Initializing environment...\n");
	std::cout.flush();
	SetupEnvironment();
	
	tfm::format(std::cout, "[Startup] Setting up networking...\n");
	std::cout.flush();
	if (!SetupNetworking()) {
		tfm::format(std::cerr, "Error: networking init failed\n");
		return EXIT_FAILURE;
	}
	
	try {
		tfm::format(std::cout, "[Startup] Parsing command line arguments...\n");
		std::cout.flush();
		SetupMinerArgs(gArgs);
		std::string error;
		if (!gArgs.ParseParameters(argc, argv, error)) {
			if (error != "") tfm::format(std::cerr, "Error parsing command line: %s\n", error);
			return EXIT_FAILURE;
		}

		// Print parsed configuration
		tfm::format(std::cout, "[Config] Chain: %s\n", ChainTypeToString(gArgs.GetChainType()).c_str());
		tfm::format(std::cout, "[Config] RPC Connect: %s:%d\n", 
			gArgs.GetArg("-rpcconnect", "127.0.0.1").c_str(),
			gArgs.GetIntArg("-rpcport", 8332));
		tfm::format(std::cout, "[Config] Mining Address: %s\n", 
			gArgs.GetArg("-address", "NOT SET").c_str());
		tfm::format(std::cout, "[Config] GPU Mining: %s\n", 
			gArgs.GetBoolArg("-gpu", false) ? "Enabled" : "Disabled");
		if (gArgs.GetBoolArg("-gpu", false)) {
			tfm::format(std::cout, "[Config] GPU Work Size: %d\n", 
				gArgs.GetIntArg("-worksize", 1024));
		}
		tfm::format(std::cout, "[Config] Max Tries: %d\n", 
			gArgs.GetIntArg("-maxtries", 1000000));
		std::cout.flush();

		SelectBaseParams(gArgs.GetChainType());
		if (auto cfgerr = common::InitConfig(gArgs, nullptr)) {
			// ignore config failure for standalone mode
		}
		
		tfm::format(std::cout, "[Startup] Setting up signal handlers...\n");
		std::cout.flush();
		std::signal(SIGINT, [](int){ g_stop.store(true); });
#ifdef SIGTERM
		std::signal(SIGTERM, [](int){ g_stop.store(true); });
#endif
		
		tfm::format(std::cout, "[Startup] Starting mining loop...\n");
		tfm::format(std::cout, "=====================================\n");
		std::cout.flush();
		MinerLoop();
	} catch (const std::exception& e) {
		tfm::format(std::cerr, "gpuminer-randomq error: %s\n", e.what());
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}