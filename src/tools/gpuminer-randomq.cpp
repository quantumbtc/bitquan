// Copyright (c) 2024-present The Bitquantum Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <array>
#include <thread>
#include <stdexcept>
#include <cstdint>
#include <cstring>

#include "crypto/randomq_mining.h"
#include "primitives/block.h"
#include "uint256.h"
#include "arith_uint256.h"
#include "serialize.h"
#include "streams.h"

#define CL_TARGET_OPENCL_VERSION 200
#include <CL/cl.h>

/* ============================================================
   GPU Miner for Bitquantum RandomQ
   1. 读取 OpenCL 完整 RandomQ 内核
   2. 编译并创建 kernel
   3. 将区块头、target、nonce 传入 GPU 并启动计算
   ============================================================ */

static std::string LoadKernelSource(const std::string& path)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) throw std::runtime_error("Failed to open kernel source: " + path);
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

struct GpuMinerContext {
    cl_context ctx = nullptr;
    cl_device_id device = nullptr;
    cl_command_queue queue = nullptr;
    cl_program program = nullptr;
    cl_kernel kernel = nullptr;
};

static void InitOpenCL(GpuMinerContext& gctx, const std::string& kernel_path)
{
    cl_int err;
    cl_uint num_platforms = 0;
    cl_platform_id platform = nullptr;
    err = clGetPlatformIDs(1, &platform, &num_platforms);
    if (err != CL_SUCCESS || num_platforms == 0)
        throw std::runtime_error("No OpenCL platform found");

    // 选择第一个 GPU
    cl_uint num_devices = 0;
    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &gctx.device, &num_devices);
    if (err != CL_SUCCESS || num_devices == 0)
        throw std::runtime_error("No OpenCL GPU device found");

    gctx.ctx = clCreateContext(nullptr, 1, &gctx.device, nullptr, nullptr, &err);
    if (err != CL_SUCCESS) throw std::runtime_error("Failed to create OpenCL context");

    #if defined(CL_VERSION_2_0)
    gctx.queue = clCreateCommandQueueWithProperties(gctx.ctx, gctx.device, nullptr, &err);
    #else
    gctx.queue = clCreateCommandQueue(gctx.ctx, gctx.device, 0, &err);
    #endif
    if (err != CL_SUCCESS) throw std::runtime_error("Failed to create OpenCL queue");

    std::string src = LoadKernelSource(kernel_path);
    const char* src_ptr = src.c_str();
    size_t src_len = src.size();
    gctx.program = clCreateProgramWithSource(gctx.ctx, 1, &src_ptr, &src_len, &err);
    if (err != CL_SUCCESS) throw std::runtime_error("Failed to create program");

    err = clBuildProgram(gctx.program, 1, &gctx.device, nullptr, nullptr, nullptr);
    if (err != CL_SUCCESS) {
        // 打印编译日志
        size_t log_size = 0;
        clGetProgramBuildInfo(gctx.program, gctx.device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &log_size);
        std::string log(log_size, '\0');
        clGetProgramBuildInfo(gctx.program, gctx.device, CL_PROGRAM_BUILD_LOG,
                              log_size, log.data(), nullptr);
        throw std::runtime_error("OpenCL build failed:\n" + log);
    }

    gctx.kernel = clCreateKernel(gctx.program, "randomq_mining_full", &err);
    if (err != CL_SUCCESS) throw std::runtime_error("Failed to create kernel");
}

/**
 * 在 GPU 上执行一次完整 RandomQ 计算
 */
static bool RunKernelBatch(GpuMinerContext& gctx,
                           const std::array<unsigned char,80>& header_le,
                           uint32_t start_nonce,
                           const std::array<unsigned char,32>& target_be,
                           uint32_t global_work_size,
                           std::array<unsigned char,32>& out_hash,
                           uint32_t& out_found_nonce)
{
    cl_int err;
    cl_mem header_buf = clCreateBuffer(gctx.ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                       80, (void*)header_le.data(), &err);
    cl_mem base_nonce_buf = clCreateBuffer(gctx.ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                           sizeof(uint32_t), &start_nonce, &err);
    cl_mem target_buf = clCreateBuffer(gctx.ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                       32, (void*)target_be.data(), &err);

    uint32_t found_flag = 0;
    cl_mem found_flag_buf = clCreateBuffer(gctx.ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                           sizeof(uint32_t), &found_flag, &err);
    uint32_t found_nonce = 0;
    cl_mem found_nonce_buf = clCreateBuffer(gctx.ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                           sizeof(uint32_t), &found_nonce, &err);
    unsigned char result_init[32] = {0};
    cl_mem result_buf = clCreateBuffer(gctx.ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                       32, result_init, &err);

    clSetKernelArg(gctx.kernel, 0, sizeof(cl_mem), &header_buf);
    clSetKernelArg(gctx.kernel, 1, sizeof(cl_mem), &base_nonce_buf);
    clSetKernelArg(gctx.kernel, 2, sizeof(cl_mem), &target_buf);
    clSetKernelArg(gctx.kernel, 3, sizeof(cl_mem), &found_flag_buf);
    clSetKernelArg(gctx.kernel, 4, sizeof(cl_mem), &found_nonce_buf);
    clSetKernelArg(gctx.kernel, 5, sizeof(cl_mem), &result_buf);

    size_t gws = global_work_size;
    err = clEnqueueNDRangeKernel(gctx.queue, gctx.kernel, 1, nullptr, &gws, nullptr, 0, nullptr, nullptr);
    clFinish(gctx.queue);

    clEnqueueReadBuffer(gctx.queue, result_buf, CL_TRUE, 0, 32, out_hash.data(), 0, nullptr, nullptr);
    clEnqueueReadBuffer(gctx.queue, found_nonce_buf, CL_TRUE, 0, sizeof(uint32_t), &out_found_nonce, 0, nullptr, nullptr);
    clEnqueueReadBuffer(gctx.queue, found_flag_buf, CL_TRUE, 0, sizeof(uint32_t), &found_flag, 0, nullptr, nullptr);

    clReleaseMemObject(header_buf);
    clReleaseMemObject(base_nonce_buf);
    clReleaseMemObject(target_buf);
    clReleaseMemObject(found_flag_buf);
    clReleaseMemObject(found_nonce_buf);
    clReleaseMemObject(result_buf);

    return (found_flag != 0);
}

/**
 * 将 nBits 转换为 target（big-endian 字节序）
 */
static std::array<unsigned char,32> TargetFromBits(unsigned int nBits)
{
    arith_uint256 target;
    bool fNeg, fOverflow;
    target.SetCompact(nBits, &fNeg, &fOverflow);
    uint256 t256 = ArithToUint256(target);
    std::array<unsigned char,32> out{};
    std::memcpy(out.data(), t256.begin(), 32);
    // 转为 big-endian
    std::reverse(out.begin(), out.end());
    return out;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: gpuminer-randomq <kernel-path>\n";
        return 1;
    }
    std::string kernel_path = argv[1]; // 例如 src/opencl/randomq_kernel_full.cl

    try {
        GpuMinerContext gctx;
        InitOpenCL(gctx, kernel_path);

        // 这里示例：你需要从 RPC 或其他地方获取真实区块头
        CBlockHeader block;      // TODO: 填充真实区块头
		block.nNonce = 1379716;
		block.nBits = 0x1e0ffff0;
		block.nTime = 1756857263;
		block.hashPrevBlock = uint256::FromHex("00000c62fac2d483d65c37331a3a73c6f315de2541e7384e94e36d3b1491604f").value_or(uint256{});
		block.hashMerkleRoot = uint256::FromHex("b0e14069031ce67080e53fe3d2cdbc23d0949fd85efac43e67ffdcf07d66d541").value_or(uint256{});
		block.nVersion = 1;
        unsigned int nBits = 0x1e0ffff0; // 示例
        uint32_t start_nonce = 0;

        // 序列化区块头 (little-endian)
        std::vector<unsigned char> ser;
        VectorWriter(ser, 0, block);
        std::array<unsigned char,80> header_le{};
        std::memcpy(header_le.data(), ser.data(), std::min<size_t>(80, ser.size()));

        auto target_be = TargetFromBits(nBits);

        std::array<unsigned char,32> gpu_hash{};
        uint32_t found_nonce = 0;

        bool found = RunKernelBatch(gctx, header_le, start_nonce,
                                    target_be, 1024,
                                    gpu_hash, found_nonce);

        if (found) {
            std::cout << "[GPU] Found nonce: " << found_nonce << "\nHash: ";
            for (unsigned char b : gpu_hash) printf("%02x", b);
            std::cout << std::endl;
        } else {
            std::cout << "[GPU] No solution in this batch.\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "GPU miner error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
