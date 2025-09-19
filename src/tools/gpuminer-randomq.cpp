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
#include <iomanip>

#include "crypto/randomq_mining.h"
#include "crypto/sha256.h"
#include "primitives/block.h"
#include "uint256.h"
#include "arith_uint256.h"
#include "serialize.h"
#include "streams.h"
#include "consensus/amount.h"
#include "script/script.h"

#define CL_TARGET_OPENCL_VERSION 200
#include <CL/cl.h>

/* ============================================================
   GPU Miner for Bitquantum RandomQ
   1. 读取 OpenCL 完整 RandomQ 内核
   2. 编译并创建 kernel
   3. 将区块头、target、nonce 传入 GPU 并启动计算
   ============================================================ */

static std::string LoadKernelSource(const std::string& primary_path)
{
    // Try explicit path first
    auto try_read = [](const std::string& p) -> std::optional<std::string> {
        std::ifstream f(p, std::ios::in | std::ios::binary);
        if (!f) return std::nullopt;
        std::ostringstream oss; oss << f.rdbuf();
        return oss.str();
    };

    // Candidate search order
    std::vector<std::string> candidates;
    if (!primary_path.empty()) candidates.push_back(primary_path);

    // Environment override
    if (const char* env = std::getenv("BTQ_OPENCL_KERNEL")) {
        candidates.emplace_back(env);
    }

    // Common relative paths
    candidates.emplace_back("randomq_kernel.cl");
    candidates.emplace_back("./randomq_kernel.cl");
    candidates.emplace_back("src/tools/randomq_kernel.cl");
    candidates.emplace_back("../src/tools/randomq_kernel.cl");
    candidates.emplace_back("../../src/tools/randomq_kernel.cl");

    for (const auto& p : candidates) {
        if (auto s = try_read(p)) return *s;
    }

    std::ostringstream msg;
    msg << "Failed to open kernel source. Tried: ";
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (i) msg << ", ";
        msg << candidates[i];
    }
    msg << ". Use --kernel=PATH or set BTQ_OPENCL_KERNEL.";
    throw std::runtime_error(msg.str());
}

struct GpuMinerContext {
    cl_context ctx = nullptr;
    cl_device_id device = nullptr;
    cl_command_queue queue = nullptr;
    cl_program program = nullptr;
    cl_kernel kernel = nullptr;
    cl_kernel debug_kernel = nullptr;
}; 

static void InitOpenCL(GpuMinerContext& gctx, const std::string& kernel_path, bool force_cpu = false)
{
    cl_int err;
    cl_uint num_platforms = 0;
    cl_platform_id platform = nullptr;
    err = clGetPlatformIDs(1, &platform, &num_platforms);
    if (err != CL_SUCCESS || num_platforms == 0)
        throw std::runtime_error("No OpenCL platform found");

    // Try GPU first, then fallback to CPU if GPU doesn't work or if forced
    cl_uint num_devices = 0;
    if (force_cpu) {
        std::cout << "[GPU] Forcing CPU OpenCL device..." << std::endl;
        err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &gctx.device, &num_devices);
        if (err != CL_SUCCESS || num_devices == 0)
            throw std::runtime_error("No OpenCL CPU device found");
    } else {
        err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &gctx.device, &num_devices);
        if (err != CL_SUCCESS || num_devices == 0) {
            std::cout << "[GPU] No GPU device found, trying CPU..." << std::endl;
            err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &gctx.device, &num_devices);
            if (err != CL_SUCCESS || num_devices == 0)
                throw std::runtime_error("No OpenCL device found");
        }
    }

    // Print GPU device information
    char device_name[256] = {0};
    char device_vendor[256] = {0};
    char driver_version[256] = {0};
    cl_device_type device_type;
    
    clGetDeviceInfo(gctx.device, CL_DEVICE_NAME, sizeof(device_name), device_name, nullptr);
    clGetDeviceInfo(gctx.device, CL_DEVICE_VENDOR, sizeof(device_vendor), device_vendor, nullptr);
    clGetDeviceInfo(gctx.device, CL_DRIVER_VERSION, sizeof(driver_version), driver_version, nullptr);
    clGetDeviceInfo(gctx.device, CL_DEVICE_TYPE, sizeof(device_type), &device_type, nullptr);
    
    std::cout << "[GPU] Device: " << device_name << std::endl;
    std::cout << "[GPU] Vendor: " << device_vendor << std::endl;
    std::cout << "[GPU] Driver: " << driver_version << std::endl;
    std::cout << "[GPU] Type: ";
    if (device_type & CL_DEVICE_TYPE_CPU) std::cout << "CPU ";
    if (device_type & CL_DEVICE_TYPE_GPU) std::cout << "GPU ";
    if (device_type & CL_DEVICE_TYPE_ACCELERATOR) std::cout << "ACCELERATOR ";
    std::cout << std::endl;
    
    // Check for Intel GPU and warn about known issues
    std::string vendor_str(device_vendor);
    std::string device_str(device_name);
    if (vendor_str.find("Intel") != std::string::npos) {
        std::cout << "[GPU] WARNING: Intel GPU detected. Intel OpenCL drivers have known issues with complex kernels." << std::endl;
        if (device_str.find("Iris") != std::string::npos || device_str.find("UHD") != std::string::npos) {
            std::cout << "[GPU] INFO: This is an integrated GPU. Performance may be limited." << std::endl;
        }
    }

    gctx.ctx = clCreateContext(nullptr, 1, &gctx.device, nullptr, nullptr, &err);
    if (err != CL_SUCCESS) throw std::runtime_error("Failed to create OpenCL context");

    #if defined(CL_VERSION_2_0)
    cl_queue_properties props[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    gctx.queue = clCreateCommandQueueWithProperties(gctx.ctx, gctx.device, props, &err);
    #else
    gctx.queue = clCreateCommandQueue(gctx.ctx, gctx.device, CL_QUEUE_PROFILING_ENABLE, &err);
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

    // List all available kernels in the program for debugging
    cl_uint num_kernels = 0;
    err = clCreateKernelsInProgram(gctx.program, 0, nullptr, &num_kernels);
    if (err == CL_SUCCESS && num_kernels > 0) {
        std::cout << "[GPU] Found " << num_kernels << " kernels in program:" << std::endl;
        std::vector<cl_kernel> kernels(num_kernels);
        err = clCreateKernelsInProgram(gctx.program, num_kernels, kernels.data(), nullptr);
        if (err == CL_SUCCESS) {
            for (cl_uint i = 0; i < num_kernels; ++i) {
                size_t name_size = 0;
                clGetKernelInfo(kernels[i], CL_KERNEL_FUNCTION_NAME, 0, nullptr, &name_size);
                std::string kernel_name(name_size, '\0');
                clGetKernelInfo(kernels[i], CL_KERNEL_FUNCTION_NAME, name_size, kernel_name.data(), nullptr);
                std::cout << "[GPU]   " << i << ": " << kernel_name.c_str() << std::endl;
                clReleaseKernel(kernels[i]);
            }
        }
    }

    gctx.kernel = clCreateKernel(gctx.program, "randomq_mining_full", &err);
    if (err != CL_SUCCESS) {
        std::cout << "[GPU] Failed to create main kernel (error: " << err << ")" << std::endl;
        throw std::runtime_error("Failed to create kernel");
    }
    
    gctx.debug_kernel = clCreateKernel(gctx.program, "randomq_debug_nonce", &err);
    if (err != CL_SUCCESS) {
        std::cout << "[GPU] Warning: Failed to create debug kernel (error: " << err << ")" << std::endl;
        std::cout << "[GPU] Debug kernel error details: ";
        switch(err) {
            case CL_INVALID_PROGRAM: std::cout << "CL_INVALID_PROGRAM"; break;
            case CL_INVALID_PROGRAM_EXECUTABLE: std::cout << "CL_INVALID_PROGRAM_EXECUTABLE"; break;
            case CL_INVALID_KERNEL_NAME: std::cout << "CL_INVALID_KERNEL_NAME"; break;
            case CL_INVALID_KERNEL_DEFINITION: std::cout << "CL_INVALID_KERNEL_DEFINITION"; break;
            case CL_INVALID_VALUE: std::cout << "CL_INVALID_VALUE"; break;
            case CL_OUT_OF_RESOURCES: std::cout << "CL_OUT_OF_RESOURCES"; break;
            case CL_OUT_OF_HOST_MEMORY: std::cout << "CL_OUT_OF_HOST_MEMORY"; break;
            default: std::cout << "Unknown error code: " << err; break;
        }
        std::cout << std::endl;
        gctx.debug_kernel = nullptr;
    }
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

    // Initialize buffers with zero values
    uint32_t init_zero = 0;
    unsigned char init_hash[32] = {0};
    
    cl_mem found_flag_buf = clCreateBuffer(gctx.ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                           sizeof(uint32_t), &init_zero, &err);
    cl_mem found_nonce_buf = clCreateBuffer(gctx.ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                           sizeof(uint32_t), &init_zero, &err);
    cl_mem result_buf = clCreateBuffer(gctx.ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                       32, init_hash, &err);

    clSetKernelArg(gctx.kernel, 0, sizeof(cl_mem), &header_buf);
    clSetKernelArg(gctx.kernel, 1, sizeof(cl_mem), &base_nonce_buf);
    clSetKernelArg(gctx.kernel, 2, sizeof(cl_mem), &target_buf);
    clSetKernelArg(gctx.kernel, 3, sizeof(cl_mem), &found_flag_buf);
    clSetKernelArg(gctx.kernel, 4, sizeof(cl_mem), &found_nonce_buf);
    clSetKernelArg(gctx.kernel, 5, sizeof(cl_mem), &result_buf);

    size_t gws = global_work_size;
    std::cout << "[DEBUG] Mining parameters:" << std::endl;
    std::cout << "[DEBUG]   Start nonce: " << start_nonce << std::endl;
    std::cout << "[DEBUG]   Work size: " << gws << std::endl;
    std::cout << "[DEBUG]   Nonce range: " << start_nonce << " to " << (start_nonce + gws - 1) << std::endl;
    std::cout << "[DEBUG]   Expected nonce 1379716 in range: " << 
                 (1379716 >= start_nonce && 1379716 < start_nonce + gws ? "YES" : "NO") << std::endl;
    std::cout << "[DEBUG] Enqueueing main kernel..." << std::endl;
    err = clEnqueueNDRangeKernel(gctx.queue, gctx.kernel, 1, nullptr, &gws, nullptr, 0, nullptr, nullptr);
    if (err != CL_SUCCESS) {
        std::cout << "[DEBUG] Main kernel execution failed with error: " << err << std::endl;
        std::cout << "[DEBUG] Error details: ";
        switch(err) {
            case CL_INVALID_PROGRAM_EXECUTABLE: std::cout << "CL_INVALID_PROGRAM_EXECUTABLE"; break;
            case CL_INVALID_COMMAND_QUEUE: std::cout << "CL_INVALID_COMMAND_QUEUE"; break;
            case CL_INVALID_KERNEL: std::cout << "CL_INVALID_KERNEL"; break;
            case CL_INVALID_CONTEXT: std::cout << "CL_INVALID_CONTEXT"; break;
            case CL_INVALID_KERNEL_ARGS: std::cout << "CL_INVALID_KERNEL_ARGS"; break;
            case CL_INVALID_WORK_DIMENSION: std::cout << "CL_INVALID_WORK_DIMENSION"; break;
            case CL_INVALID_WORK_GROUP_SIZE: std::cout << "CL_INVALID_WORK_GROUP_SIZE"; break;
            case CL_INVALID_WORK_ITEM_SIZE: std::cout << "CL_INVALID_WORK_ITEM_SIZE"; break;
            case CL_INVALID_GLOBAL_OFFSET: std::cout << "CL_INVALID_GLOBAL_OFFSET"; break;
            case CL_OUT_OF_RESOURCES: std::cout << "CL_OUT_OF_RESOURCES"; break;
            case CL_MEM_OBJECT_ALLOCATION_FAILURE: std::cout << "CL_MEM_OBJECT_ALLOCATION_FAILURE"; break;
            case CL_INVALID_EVENT_WAIT_LIST: std::cout << "CL_INVALID_EVENT_WAIT_LIST"; break;
            case CL_OUT_OF_HOST_MEMORY: std::cout << "CL_OUT_OF_HOST_MEMORY"; break;
            default: std::cout << "Unknown error code: " << err; break;
        }
        std::cout << std::endl;
    }
    clFinish(gctx.queue);

    // Initialize output variables before reading
    out_hash.fill(0);
    out_found_nonce = 0;
    uint32_t found_flag = 0;
    
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
 * 使用调试内核测试特定 nonce
 */
static bool RunDebugKernel(GpuMinerContext& gctx,
                          const std::array<unsigned char,80>& header_le,
                          uint32_t test_nonce,
                          std::array<unsigned char,32>& out_hash)
{
    if (gctx.debug_kernel == nullptr) {
        return false;
    }
    
    cl_int err;
    cl_mem header_buf = clCreateBuffer(gctx.ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                       80, (void*)header_le.data(), &err);
    if (err != CL_SUCCESS) {
        std::cout << "[DEBUG] Header buffer creation failed with error: " << err << std::endl;
        return false;
    }
    
    cl_mem nonce_buf = clCreateBuffer(gctx.ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                      sizeof(uint32_t), &test_nonce, &err);
    if (err != CL_SUCCESS) {
        std::cout << "[DEBUG] Nonce buffer creation failed with error: " << err << std::endl;
        clReleaseMemObject(header_buf);
        return false;
    }
    
    // Try using READ_WRITE with initial data to test if buffer is accessible
    unsigned char init_pattern[32];
    for (int i = 0; i < 32; ++i) init_pattern[i] = 0x99; // Initial pattern to verify buffer state
    
    cl_mem result_buf = clCreateBuffer(gctx.ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                       32, init_pattern, &err);
    if (err != CL_SUCCESS) {
        std::cout << "[DEBUG] Result buffer creation failed with error: " << err << std::endl;
        clReleaseMemObject(header_buf);
        clReleaseMemObject(nonce_buf);
        return false;
    }
    
    std::cout << "[DEBUG] Result buffer created with initial 0x99 pattern" << std::endl;

    std::cout << "[DEBUG] Setting kernel arguments..." << std::endl;
    err = clSetKernelArg(gctx.debug_kernel, 0, sizeof(cl_mem), &header_buf);
    if (err != CL_SUCCESS) {
        std::cout << "[DEBUG] Failed to set kernel arg 0 (header): " << err << std::endl;
        return false;
    } else {
        std::cout << "[DEBUG] Kernel arg 0 (header) set successfully" << std::endl;
    }
    
    err = clSetKernelArg(gctx.debug_kernel, 1, sizeof(cl_mem), &nonce_buf);
    if (err != CL_SUCCESS) {
        std::cout << "[DEBUG] Failed to set kernel arg 1 (nonce): " << err << std::endl;
        return false;
    } else {
        std::cout << "[DEBUG] Kernel arg 1 (nonce) set successfully" << std::endl;
    }
    
    err = clSetKernelArg(gctx.debug_kernel, 2, sizeof(cl_mem), &result_buf);
    if (err != CL_SUCCESS) {
        std::cout << "[DEBUG] Failed to set kernel arg 2 (result): " << err << std::endl;
        return false;
    } else {
        std::cout << "[DEBUG] Kernel arg 2 (result) set successfully" << std::endl;
    }

    // Check work group size limits
    size_t max_work_group_size = 0;
    err = clGetKernelWorkGroupInfo(gctx.debug_kernel, gctx.device, CL_KERNEL_WORK_GROUP_SIZE, 
                                   sizeof(size_t), &max_work_group_size, nullptr);
    if (err == CL_SUCCESS) {
        std::cout << "[DEBUG] Max work group size for debug kernel: " << max_work_group_size << std::endl;
    } else {
        std::cout << "[DEBUG] Failed to get work group info: " << err << std::endl;
    }
    
    size_t gws = 1;
    std::cout << "[DEBUG] About to enqueue debug kernel with work size: " << gws << std::endl;
    
    cl_event kernel_event;
    err = clEnqueueNDRangeKernel(gctx.queue, gctx.debug_kernel, 1, nullptr, &gws, nullptr, 0, nullptr, &kernel_event);
    if (err != CL_SUCCESS) {
        std::cout << "[DEBUG] Kernel execution failed with error: " << err << std::endl;
        std::cout << "[DEBUG] Error details: ";
        switch(err) {
            case CL_INVALID_PROGRAM_EXECUTABLE: std::cout << "CL_INVALID_PROGRAM_EXECUTABLE"; break;
            case CL_INVALID_COMMAND_QUEUE: std::cout << "CL_INVALID_COMMAND_QUEUE"; break;
            case CL_INVALID_KERNEL: std::cout << "CL_INVALID_KERNEL"; break;
            case CL_INVALID_CONTEXT: std::cout << "CL_INVALID_CONTEXT"; break;
            case CL_INVALID_KERNEL_ARGS: std::cout << "CL_INVALID_KERNEL_ARGS"; break;
            case CL_INVALID_WORK_DIMENSION: std::cout << "CL_INVALID_WORK_DIMENSION"; break;
            case CL_INVALID_WORK_GROUP_SIZE: std::cout << "CL_INVALID_WORK_GROUP_SIZE"; break;
            case CL_INVALID_WORK_ITEM_SIZE: std::cout << "CL_INVALID_WORK_ITEM_SIZE"; break;
            case CL_INVALID_GLOBAL_OFFSET: std::cout << "CL_INVALID_GLOBAL_OFFSET"; break;
            case CL_OUT_OF_RESOURCES: std::cout << "CL_OUT_OF_RESOURCES"; break;
            case CL_MEM_OBJECT_ALLOCATION_FAILURE: std::cout << "CL_MEM_OBJECT_ALLOCATION_FAILURE"; break;
            case CL_INVALID_EVENT_WAIT_LIST: std::cout << "CL_INVALID_EVENT_WAIT_LIST"; break;
            case CL_OUT_OF_HOST_MEMORY: std::cout << "CL_OUT_OF_HOST_MEMORY"; break;
            default: std::cout << "Unknown error code: " << err; break;
        }
        std::cout << std::endl;
        clReleaseMemObject(header_buf);
        clReleaseMemObject(nonce_buf);
        clReleaseMemObject(result_buf);
        return false;
    }
    
    std::cout << "[DEBUG] Kernel enqueued successfully, waiting for completion..." << std::endl;
    
    // Check status before waiting
    cl_int exec_status;
    err = clGetEventInfo(kernel_event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &exec_status, nullptr);
    if (err == CL_SUCCESS) {
        std::cout << "[DEBUG] Initial kernel status: ";
        switch(exec_status) {
            case CL_QUEUED: std::cout << "CL_QUEUED"; break;
            case CL_SUBMITTED: std::cout << "CL_SUBMITTED"; break;
            case CL_RUNNING: std::cout << "CL_RUNNING"; break;
            case CL_COMPLETE: std::cout << "CL_COMPLETE"; break;
            default: std::cout << "Error status: " << exec_status; break;
        }
        std::cout << std::endl;
    }
    
    std::cout << "[DEBUG] Calling clFinish..." << std::endl;
    
    // Intel GPU workaround: flush before finish
    clFlush(gctx.queue);
    
    // Wait for the event specifically instead of just clFinish
    err = clWaitForEvents(1, &kernel_event);
    if (err != CL_SUCCESS) {
        std::cout << "[DEBUG] clWaitForEvents failed: " << err << std::endl;
    }
    
    clFinish(gctx.queue);
    std::cout << "[DEBUG] clFinish completed" << std::endl;
    
    // Check kernel execution status after clFinish
    err = clGetEventInfo(kernel_event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &exec_status, nullptr);
    if (err == CL_SUCCESS) {
        std::cout << "[DEBUG] Final kernel execution status: ";
        switch(exec_status) {
            case CL_QUEUED: std::cout << "CL_QUEUED"; break;
            case CL_SUBMITTED: std::cout << "CL_SUBMITTED"; break;
            case CL_RUNNING: std::cout << "CL_RUNNING"; break;
            case CL_COMPLETE: std::cout << "CL_COMPLETE"; break;
            default: std::cout << "Error status: " << exec_status; break;
        }
        std::cout << std::endl;
    } else {
        std::cout << "[DEBUG] Failed to get kernel execution status: " << err << std::endl;
    }
    
    // Get timing information if available
    cl_ulong start_time = 0, end_time = 0;
    err = clGetEventProfilingInfo(kernel_event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start_time, nullptr);
    if (err == CL_SUCCESS) {
        err = clGetEventProfilingInfo(kernel_event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end_time, nullptr);
        if (err == CL_SUCCESS && end_time > start_time) {
            double duration_ms = (end_time - start_time) / 1000000.0; // Convert nanoseconds to milliseconds
            std::cout << "[DEBUG] Kernel execution time: " << duration_ms << " ms" << std::endl;
        }
    }
    
    clReleaseEvent(kernel_event);

    // Initialize output before reading
    out_hash.fill(0);
    
    err = clEnqueueReadBuffer(gctx.queue, result_buf, CL_TRUE, 0, 32, out_hash.data(), 0, nullptr, nullptr);
    if (err != CL_SUCCESS) {
        std::cout << "[DEBUG] Buffer read failed with error: " << err << std::endl;
    }

    clReleaseMemObject(header_buf);
    clReleaseMemObject(nonce_buf);
    clReleaseMemObject(result_buf);

    return (err == CL_SUCCESS);
}

/**
 * Test ultra-simple kernel to verify basic GPU functionality
 */
static bool TestSimpleKernel(GpuMinerContext& gctx)
{
    std::cout << "[DEBUG] Testing ultra-simple kernel..." << std::endl;
    
    cl_int err;
    cl_kernel simple_kernel = clCreateKernel(gctx.program, "simple_test", &err);
    if (err != CL_SUCCESS) {
        std::cout << "[DEBUG] Failed to create simple test kernel: " << err << std::endl;
        return false;
    }
    
    // Create output buffer
    unsigned char init_data[32];
    for (int i = 0; i < 32; ++i) init_data[i] = 0x00;
    
    cl_mem output_buf = clCreateBuffer(gctx.ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                       32, init_data, &err);
    if (err != CL_SUCCESS) {
        std::cout << "[DEBUG] Failed to create output buffer: " << err << std::endl;
        clReleaseKernel(simple_kernel);
        return false;
    }
    
    // Set kernel argument
    err = clSetKernelArg(simple_kernel, 0, sizeof(cl_mem), &output_buf);
    if (err != CL_SUCCESS) {
        std::cout << "[DEBUG] Failed to set simple kernel arg: " << err << std::endl;
        clReleaseMemObject(output_buf);
        clReleaseKernel(simple_kernel);
        return false;
    }
    
    // Execute kernel
    size_t gws = 1;
    err = clEnqueueNDRangeKernel(gctx.queue, simple_kernel, 1, nullptr, &gws, nullptr, 0, nullptr, nullptr);
    if (err != CL_SUCCESS) {
        std::cout << "[DEBUG] Failed to execute simple kernel: " << err << std::endl;
        clReleaseMemObject(output_buf);
        clReleaseKernel(simple_kernel);
        return false;
    }
    
    clFinish(gctx.queue);
    
    // Read result
    unsigned char result[32];
    err = clEnqueueReadBuffer(gctx.queue, output_buf, CL_TRUE, 0, 32, result, 0, nullptr, nullptr);
    if (err != CL_SUCCESS) {
        std::cout << "[DEBUG] Failed to read simple kernel result: " << err << std::endl;
        clReleaseMemObject(output_buf);
        clReleaseKernel(simple_kernel);
        return false;
    }
    
    // Check result
    bool success = (result[0] == 0xDE && result[1] == 0xAD && result[2] == 0xBE && result[3] == 0xEF);
    std::cout << "[DEBUG] Simple kernel result: ";
    for (int i = 0; i < 8; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)result[i] << " ";
    }
    std::cout << std::dec << std::endl;
    std::cout << "[DEBUG] Simple kernel test: " << (success ? "PASSED" : "FAILED") << std::endl;
    
    clReleaseMemObject(output_buf);
    clReleaseKernel(simple_kernel);
    return success;
}

/**
 * Test minimal kernel for Intel GPU compatibility
 */
static bool TestMinimalKernel(GpuMinerContext& gctx)
{
    std::cout << "[DEBUG] Testing minimal kernel (Intel GPU compatible)..." << std::endl;
    
    cl_int err;
    cl_kernel minimal_kernel = clCreateKernel(gctx.program, "minimal_test", &err);
    if (err != CL_SUCCESS) {
        std::cout << "[DEBUG] Failed to create minimal test kernel: " << err << std::endl;
        return false;
    }
    
    // Create output buffer for 4 uint32 values
    uint32_t init_data[4] = {0, 0, 0, 0};
    
    cl_mem output_buf = clCreateBuffer(gctx.ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                       4 * sizeof(uint32_t), init_data, &err);
    if (err != CL_SUCCESS) {
        std::cout << "[DEBUG] Failed to create minimal output buffer: " << err << std::endl;
        clReleaseKernel(minimal_kernel);
        return false;
    }
    
    // Set kernel argument
    err = clSetKernelArg(minimal_kernel, 0, sizeof(cl_mem), &output_buf);
    if (err != CL_SUCCESS) {
        std::cout << "[DEBUG] Failed to set minimal kernel arg: " << err << std::endl;
        clReleaseMemObject(output_buf);
        clReleaseKernel(minimal_kernel);
        return false;
    }
    
    // Execute kernel with 4 work items
    size_t gws = 4;
    cl_event event;
    err = clEnqueueNDRangeKernel(gctx.queue, minimal_kernel, 1, nullptr, &gws, nullptr, 0, nullptr, &event);
    if (err != CL_SUCCESS) {
        std::cout << "[DEBUG] Failed to execute minimal kernel: " << err << std::endl;
        clReleaseMemObject(output_buf);
        clReleaseKernel(minimal_kernel);
        return false;
    }
    
    // Intel GPU workaround
    clFlush(gctx.queue);
    err = clWaitForEvents(1, &event);
    if (err != CL_SUCCESS) {
        std::cout << "[DEBUG] clWaitForEvents failed for minimal kernel: " << err << std::endl;
    }
    clFinish(gctx.queue);
    
    // Check execution status
    cl_int exec_status;
    err = clGetEventInfo(event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &exec_status, nullptr);
    if (err == CL_SUCCESS) {
        std::cout << "[DEBUG] Minimal kernel execution status: ";
        switch(exec_status) {
            case CL_QUEUED: std::cout << "CL_QUEUED"; break;
            case CL_SUBMITTED: std::cout << "CL_SUBMITTED"; break;
            case CL_RUNNING: std::cout << "CL_RUNNING"; break;
            case CL_COMPLETE: std::cout << "CL_COMPLETE"; break;
            default: std::cout << "Error status: " << exec_status; break;
        }
        std::cout << std::endl;
    }
    
    // Read result
    uint32_t result[4];
    err = clEnqueueReadBuffer(gctx.queue, output_buf, CL_TRUE, 0, 4 * sizeof(uint32_t), result, 0, nullptr, nullptr);
    if (err != CL_SUCCESS) {
        std::cout << "[DEBUG] Failed to read minimal kernel result: " << err << std::endl;
        clReleaseEvent(event);
        clReleaseMemObject(output_buf);
        clReleaseKernel(minimal_kernel);
        return false;
    }
    
    // Check result (should be gid + 0x12345678)
    bool success = true;
    std::cout << "[DEBUG] Minimal kernel results: ";
    for (int i = 0; i < 4; ++i) {
        std::cout << std::hex << result[i] << " ";
        uint32_t expected = i + 0x12345678;
        if (result[i] != expected) {
            success = false;
        }
    }
    std::cout << std::dec << std::endl;
    std::cout << "[DEBUG] Expected: 12345678 12345679 1234567a 1234567b" << std::endl;
    std::cout << "[DEBUG] Minimal kernel test: " << (success ? "PASSED" : "FAILED") << std::endl;
    
    clReleaseEvent(event);
    clReleaseMemObject(output_buf);
    clReleaseKernel(minimal_kernel);
    return success;
}

/**
 * Hybrid CPU-GPU mining: CPU handles SHA256, GPU handles RandomQ
 */
static bool RunHybridMining(GpuMinerContext& gctx,
                           const std::array<unsigned char,80>& header_le,
                           uint32_t start_nonce,
                           const std::array<unsigned char,32>& target_be,
                           uint32_t global_work_size,
                           std::array<unsigned char,32>& out_hash,
                           uint32_t& out_found_nonce)
{
    std::cout << "[HYBRID] Starting CPU-GPU hybrid mining..." << std::endl;
    std::cout << "[HYBRID] CPU: SHA256 processing" << std::endl;
    std::cout << "[HYBRID] GPU: RandomQ processing" << std::endl;
    
    cl_int err;
    
    // Step 1: CPU calculates SHA256 of header
    std::cout << "[CPU] Computing SHA256 of block header..." << std::endl;
    
    // Create header with nonce (we'll use start_nonce for the SHA256)
    std::array<unsigned char,80> header_with_nonce = header_le;
    header_with_nonce[76] = (unsigned char)((start_nonce) & 0xFF);
    header_with_nonce[77] = (unsigned char)((start_nonce >> 8) & 0xFF);
    header_with_nonce[78] = (unsigned char)((start_nonce >> 16) & 0xFF);
    header_with_nonce[79] = (unsigned char)((start_nonce >> 24) & 0xFF);
    
    // CPU computes SHA256 of header (using existing crypto functions)
    std::array<unsigned char,32> first_sha256;
    
    // Use the existing crypto functions to compute SHA256
    CSHA256 sha256_hasher;
    sha256_hasher.Write(header_with_nonce.data(), 80);
    sha256_hasher.Finalize(first_sha256.data());
    
    std::cout << "[CPU] SHA256 result: ";
    for (int i = 0; i < 8; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)first_sha256[i];
    }
    std::cout << "..." << std::dec << std::endl;
    
    // Step 2: Create OpenCL buffers
    cl_mem sha256_buf = clCreateBuffer(gctx.ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                       32, (void*)first_sha256.data(), &err);
    if (err != CL_SUCCESS) {
        std::cout << "[HYBRID] Failed to create SHA256 buffer: " << err << std::endl;
        return false;
    }
    
    cl_mem nonce_base_buf = clCreateBuffer(gctx.ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                          sizeof(uint32_t), &start_nonce, &err);
    cl_mem target_buf = clCreateBuffer(gctx.ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                      32, (void*)target_be.data(), &err);
    
    uint32_t init_zero = 0;
    cl_mem found_flag_buf = clCreateBuffer(gctx.ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                          sizeof(uint32_t), &init_zero, &err);
    cl_mem found_nonce_buf = clCreateBuffer(gctx.ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                           sizeof(uint32_t), &init_zero, &err);
    
    // Result buffer for RandomQ outputs
    unsigned char result_buffer[32] = {0};
    cl_mem result_buf = clCreateBuffer(gctx.ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                      32, result_buffer, &err);
    
    // Step 3: Create and set up hybrid kernel
    cl_kernel hybrid_kernel = clCreateKernel(gctx.program, "randomq_only", &err);
    if (err != CL_SUCCESS) {
        std::cout << "[HYBRID] Failed to create hybrid kernel: " << err << std::endl;
        clReleaseMemObject(sha256_buf);
        clReleaseMemObject(nonce_base_buf);
        clReleaseMemObject(target_buf);
        clReleaseMemObject(found_flag_buf);
        clReleaseMemObject(found_nonce_buf);
        clReleaseMemObject(result_buf);
        return false;
    }
    
    clSetKernelArg(hybrid_kernel, 0, sizeof(cl_mem), &sha256_buf);
    clSetKernelArg(hybrid_kernel, 1, sizeof(cl_mem), &nonce_base_buf);
    clSetKernelArg(hybrid_kernel, 2, sizeof(cl_mem), &target_buf);
    clSetKernelArg(hybrid_kernel, 3, sizeof(cl_mem), &found_flag_buf);
    clSetKernelArg(hybrid_kernel, 4, sizeof(cl_mem), &found_nonce_buf);
    clSetKernelArg(hybrid_kernel, 5, sizeof(cl_mem), &result_buf);
    
    // Step 4: Execute GPU RandomQ processing
    std::cout << "[GPU] Processing RandomQ for " << global_work_size << " nonces..." << std::endl;
    size_t gws = global_work_size;
    
    cl_event kernel_event;
    err = clEnqueueNDRangeKernel(gctx.queue, hybrid_kernel, 1, nullptr, &gws, nullptr, 0, nullptr, &kernel_event);
    if (err != CL_SUCCESS) {
        std::cout << "[HYBRID] Failed to execute hybrid kernel: " << err << std::endl;
        clReleaseKernel(hybrid_kernel);
        clReleaseMemObject(sha256_buf);
        clReleaseMemObject(nonce_base_buf);
        clReleaseMemObject(target_buf);
        clReleaseMemObject(found_flag_buf);
        clReleaseMemObject(found_nonce_buf);
        clReleaseMemObject(result_buf);
        return false;
    }
    
    // Wait for GPU completion
    clFlush(gctx.queue);
    clWaitForEvents(1, &kernel_event);
    clFinish(gctx.queue);
    
    // Step 5: CPU reads results
    std::cout << "[CPU] Reading GPU RandomQ results..." << std::endl;
    
    uint32_t found_flag = 0;
    clEnqueueReadBuffer(gctx.queue, found_flag_buf, CL_TRUE, 0, sizeof(uint32_t), &found_flag, 0, nullptr, nullptr);
    clEnqueueReadBuffer(gctx.queue, found_nonce_buf, CL_TRUE, 0, sizeof(uint32_t), &out_found_nonce, 0, nullptr, nullptr);
    clEnqueueReadBuffer(gctx.queue, result_buf, CL_TRUE, 0, 32, out_hash.data(), 0, nullptr, nullptr);
    
    bool found = (found_flag != 0);
    
    if (found) {
        std::cout << "[HYBRID] GPU completed RandomQ processing" << std::endl;
        std::cout << "[CPU] RandomQ result: ";
        for (int i = 0; i < 8; ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)out_hash[i];
        }
        std::cout << "..." << std::dec << std::endl;
        
        // Step 6: CPU performs final SHA256 on RandomQ result
        std::cout << "[CPU] Computing final SHA256..." << std::endl;
        
        // Compute final SHA256 of the RandomQ result
        CSHA256 final_sha256_hasher;
        final_sha256_hasher.Write(out_hash.data(), 32);
        
        std::array<unsigned char,32> final_hash;
        final_sha256_hasher.Finalize(final_hash.data());
        
        // Copy final hash back to out_hash (little-endian format for comparison)
        for (int i = 0; i < 32; ++i) {
            out_hash[i] = final_hash[31 - i]; // Convert to little-endian
        }
        
        std::cout << "[CPU] Final hash (LE): ";
        for (int i = 0; i < 8; ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)out_hash[i];
        }
        std::cout << "..." << std::dec << std::endl;
    }
    
    // Cleanup
    clReleaseEvent(kernel_event);
    clReleaseKernel(hybrid_kernel);
    clReleaseMemObject(sha256_buf);
    clReleaseMemObject(nonce_base_buf);
    clReleaseMemObject(target_buf);
    clReleaseMemObject(found_flag_buf);
    clReleaseMemObject(found_nonce_buf);
    clReleaseMemObject(result_buf);
    
    std::cout << "[HYBRID] Hybrid mining completed. Found: " << (found ? "YES" : "NO") << std::endl;
    return found;
}

/**
 * 将 nBits 转换为 target（little-endian 字节序，与哈希比较一致）
 */
static std::array<unsigned char,32> TargetFromBits(unsigned int nBits)
{
    arith_uint256 target;
    bool fNeg, fOverflow;
    target.SetCompact(nBits, &fNeg, &fOverflow);
    uint256 t256 = ArithToUint256(target);
    std::array<unsigned char,32> out{};
    std::memcpy(out.data(), t256.begin(), 32);
    // 不转换字节序，保持 little-endian
    return out;
}

/**
 * 创建实际的创世区块用于验证
 */
static CBlock CreateActualGenesisBlock()
{
    CBlock genesis;
    genesis.nVersion = 1;
    genesis.nTime = 1756857263;
    genesis.nBits = 0x1e0ffff0;
    genesis.nNonce = 1379716;
    
    // 设置创世区块的哈希值
    genesis.hashPrevBlock = uint256::FromHex("0000000000000000000000000000000000000000000000000000000000000000").value_or(uint256{});
    genesis.hashMerkleRoot = uint256::FromHex("b0e14069031ce67080e53fe3d2cdbc23d0949fd85efac43e67ffdcf07d66d541").value_or(uint256{});
    
    return genesis;
}

/**
 * Test if GPU/device is functional by running a simple test
 */
static bool TestDeviceFunctionality(GpuMinerContext& gctx)
{
    std::cout << "[GPU] Testing device functionality..." << std::endl;
    
    // Try the minimal kernel first
    bool minimal_works = TestMinimalKernel(gctx);
    if (minimal_works) {
        std::cout << "[GPU] Device is functional (minimal kernel works)" << std::endl;
        return true;
    }
    
    std::cout << "[GPU] Device appears non-functional (minimal kernel failed)" << std::endl;
    return false;
}

/**
 * 验证 GPU 算法是否正确
 */
static bool VerifyGenesisBlock()
{
        std::cout << "GPU RandomQ Algorithm Test: Testing GPU with actual genesis block header..." << std::endl;
    
    try {
        GpuMinerContext gctx;
        bool use_cpu = false;
        
        // First try GPU
        try {
            InitOpenCL(gctx, "src/tools/randomq_kernel.cl", false);
            
            // Test if the device actually works
            if (!TestDeviceFunctionality(gctx)) {
                std::cout << "[GPU] GPU device non-functional, switching to CPU..." << std::endl;
                use_cpu = true;
                
                // Clean up failed GPU context
                if (gctx.debug_kernel) clReleaseKernel(gctx.debug_kernel);
                if (gctx.kernel) clReleaseKernel(gctx.kernel);
                if (gctx.program) clReleaseProgram(gctx.program);
                if (gctx.queue) clReleaseCommandQueue(gctx.queue);
                if (gctx.ctx) clReleaseContext(gctx.ctx);
                
                // Reset context
                gctx = GpuMinerContext{};
            }
        } catch (const std::exception& e) {
            std::cout << "[GPU] GPU initialization failed: " << e.what() << std::endl;
            std::cout << "[GPU] Switching to CPU..." << std::endl;
            use_cpu = true;
        }
        
        // Initialize CPU if needed
        if (use_cpu) {
            InitOpenCL(gctx, "src/tools/randomq_kernel.cl", true);
        }
        
        std::cout << "\n=== Using " << (use_cpu ? "CPU" : "GPU") << " device for mining tests ===" << std::endl;
        
        // 创建实际的创世区块
        CBlock genesis = CreateActualGenesisBlock();
        
        std::cout << "Genesis Block Info:" << std::endl;
        std::cout << "  Version: " << genesis.nVersion << std::endl;
        std::cout << "  Time: " << genesis.nTime << std::endl;
        std::cout << "  Bits: 0x" << std::hex << genesis.nBits << std::dec << std::endl;
        std::cout << "  Nonce: " << genesis.nNonce << std::endl;
        std::cout << "  PrevBlock: " << genesis.hashPrevBlock.GetHex() << std::endl;
        std::cout << "  MerkleRoot: " << genesis.hashMerkleRoot.GetHex() << std::endl;
        
        // 序列化区块头 (little-endian)
        std::vector<unsigned char> ser;
        VectorWriter(ser, 0, genesis.nVersion, genesis.hashPrevBlock, genesis.hashMerkleRoot, genesis.nTime, genesis.nBits, genesis.nNonce);
        std::array<unsigned char,80> header_le{};
        std::memcpy(header_le.data(), ser.data(), std::min<size_t>(80, ser.size()));
        
        std::cout << "Serialized Header (80 bytes): ";
        for (int i = 0; i < 80; ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)header_le[i];
        }
        std::cout << std::endl;
        
        auto target_be = TargetFromBits(genesis.nBits);
        std::cout << "Target: ";
        for (unsigned char b : target_be) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)b;
        }
        std::cout << std::dec;
        std::cout << std::endl;
        
        std::array<unsigned char,32> gpu_hash{};
        uint32_t found_nonce = 0;
        
        // 测试1：小范围搜索（包含预期的 nonce）
        std::cout << "\nTest 1: Small range search around expected nonce..." << std::endl;
        uint32_t test_start_nonce = 1379710; // 从接近预期 nonce 开始
        // 清零 gpu_hash 以确保没有垃圾数据
        gpu_hash.fill(0);
        found_nonce = 0;
        
        std::cout << "[DEBUG] About to run main mining kernel with work size 1024..." << std::endl;
        bool found = RunKernelBatch(gctx, header_le, test_start_nonce,
                                    target_be, 1024,
                                    gpu_hash, found_nonce);
        std::cout << "[DEBUG] RunKernelBatch returned: " << (found ? "FOUND" : "NOT_FOUND") << std::endl;
        
        std::cout << "GPU Found Expected Nonce: " << (found && found_nonce == 1379716 ? "YES" : "NO") << std::endl;
        std::cout << "Found Nonce: " << found_nonce << " (expected: 1379716)" << std::endl;
        std::cout.flush(); // 强制刷新输出缓冲区
        
        if (found) {
            std::cout << "GPU Hash (LE): ";
            for (unsigned char b : gpu_hash) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)b;
            }
            std::cout << std::dec << std::endl;
            
            // 转换为大端序
            std::string gpu_hash_be;
            for (int i = 31; i >= 0; --i) {
                char buf[3];
                sprintf(buf, "%02x", gpu_hash[i]);
                gpu_hash_be += buf;
            }
            std::cout << "GPU Hash (BE): " << gpu_hash_be << std::endl;
            std::cout << "Expected (BE): 00000c62fac2d483d65c37331a3a73c6f315de2541e7384e94e36d3b1491604f" << std::endl;
            
            bool hash_matches = (gpu_hash_be == "00000c62fac2d483d65c37331a3a73c6f315de2541e7384e94e36d3b1491604f");
            std::cout << "Hash Match: " << (hash_matches ? "YES" : "NO") << std::endl;
            
            if (found_nonce == 1379716 && hash_matches) {
                std::cout << "GPU Verification Result: SUCCESS" << std::endl;
                std::cout << "GPU RandomQ algorithm is working correctly!" << std::endl;
                return true;
            }
        }
        
        // 测试2：更大的范围搜索
        std::cout << "\nTest 2: Larger range search..." << std::endl;
        test_start_nonce = 0;
        found = RunKernelBatch(gctx, header_le, test_start_nonce,
                               target_be, 10000, // 更大的工作大小
                               gpu_hash, found_nonce);
        
        std::cout << "GPU Found Any Solution: " << (found ? "YES" : "NO") << std::endl;
        if (found) {
            std::cout << "Found Nonce: " << found_nonce << std::endl;
            std::cout << "GPU Hash (LE): ";
            for (unsigned char b : gpu_hash) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)b;
            }
            std::cout << std::dec;
            std::cout << std::endl;
        }
        
        // 测试3：直接测试预期的 nonce
        std::cout << "\nTest 3: Direct test of expected nonce..." << std::endl;
        test_start_nonce = 1379716; // 直接测试预期的 nonce
        found = RunKernelBatch(gctx, header_le, test_start_nonce,
                               target_be, 1, // 只测试一个 nonce
                               gpu_hash, found_nonce);
        
        std::cout << "Direct Test Result: " << (found ? "FOUND" : "NOT FOUND") << std::endl;
        if (found) {
            std::cout << "Found Nonce: " << found_nonce << std::endl;
            std::cout << "GPU Hash (LE): ";
            for (unsigned char b : gpu_hash) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)b;
            }
            std::cout << std::dec;
            std::cout << std::endl;
        }
        
        // 测试4：CPU 验证
        std::cout << "\nTest 4: CPU verification of expected nonce..." << std::endl;
        try {
            // 使用 CPU 实现计算哈希
            CBlockHeader test_header = genesis;
            test_header.nNonce = 1379716;
            
            std::vector<unsigned char> test_ser;
            VectorWriter(test_ser, 0, test_header.nVersion, test_header.hashPrevBlock, test_header.hashMerkleRoot, test_header.nTime, test_header.nBits, test_header.nNonce);
            
            // 使用 CPU RandomQ 实现
            CRandomQHash cpu_hasher;
            cpu_hasher.Write(std::span<const unsigned char>(test_ser.data(), test_ser.size()));
            cpu_hasher.SetRandomQNonce(1379716);
            cpu_hasher.SetRandomQRounds(8192);
            
            unsigned char cpu_hash[32];
            cpu_hasher.Finalize(std::span<unsigned char>(cpu_hash, 32));
            
            std::cout << "CPU Hash (LE): ";
            for (unsigned char b : cpu_hash) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)b;
            }
            std::cout << std::dec;
            std::cout << std::endl;
            
            // 转换为大端序
            std::string cpu_hash_be;
            for (int i = 31; i >= 0; --i) {
                char buf[3];
                sprintf(buf, "%02x", cpu_hash[i]);
                cpu_hash_be += buf;
            }
            std::cout << "CPU Hash (BE): " << cpu_hash_be << std::endl;
            std::cout << "Expected (BE): 00000c62fac2d483d65c37331a3a73c6f315de2541e7384e94e36d3b1491604f" << std::endl;
            
            bool cpu_matches = (cpu_hash_be == "00000c62fac2d483d65c37331a3a73c6f315de2541e7384e94e36d3b1491604f");
            std::cout << "CPU Hash Match: " << (cpu_matches ? "YES" : "NO") << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "CPU Test Error: " << e.what() << std::endl;
        }
        
        // 测试5：最小内核测试
        std::cout << "\nTest 5: Minimal kernel test..." << std::endl;
        bool minimal_test_passed = TestMinimalKernel(gctx);
        std::cout << "Minimal kernel test result: " << (minimal_test_passed ? "PASSED" : "FAILED") << std::endl;
        
        // 测试5b：简单内核测试
        std::cout << "\nTest 5b: Simple kernel test..." << std::endl;
        bool simple_test_passed = TestSimpleKernel(gctx);
        std::cout << "Simple kernel test result: " << (simple_test_passed ? "PASSED" : "FAILED") << std::endl;
        
        // 测试6：混合CPU-GPU挖矿测试
        std::cout << "\nTest 6: Hybrid CPU-GPU mining test..." << std::endl;
        std::array<unsigned char,32> hybrid_hash{};
        uint32_t hybrid_nonce = 0;
        
        bool hybrid_success = RunHybridMining(gctx, header_le, 1379710,
                                             target_be, 10,
                                             hybrid_hash, hybrid_nonce);
        std::cout << "Hybrid mining test result: " << (hybrid_success ? "SUCCESS" : "FAILED") << std::endl;
        
        // 测试7：GPU 调试内核直接测试
        std::cout << "\nTest 7: GPU debug kernel direct test..." << std::endl;
        if (gctx.debug_kernel != nullptr) {
            std::cout << "[DEBUG] Debug kernel is available, attempting to run..." << std::endl;
            std::array<unsigned char,32> gpu_debug_hash{};
            bool debug_success = RunDebugKernel(gctx, header_le, 1379716, gpu_debug_hash);
            std::cout << "[DEBUG] RunDebugKernel returned: " << (debug_success ? "SUCCESS" : "FAILURE") << std::endl;
            
            if (debug_success) {
                std::cout << "GPU Debug Hash (LE): ";
                for (unsigned char b : gpu_debug_hash) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)b;
                }
                std::cout << std::dec;
                std::cout << std::endl;
                
                // Check if we still have the initial pattern (0x99) or if kernel wrote something
                bool has_initial_pattern = true;
                for (int i = 0; i < 32; ++i) {
                    if (gpu_debug_hash[i] != 0x99) {
                        has_initial_pattern = false;
                        break;
                    }
                }
                
                if (has_initial_pattern) {
                    std::cout << "DEBUG: Buffer still contains initial 0x99 pattern - kernel did not modify buffer at all" << std::endl;
                } else if (gpu_debug_hash[0] == 0xAA && gpu_debug_hash[1] == 0xBB && 
                          gpu_debug_hash[2] == 0xCC && gpu_debug_hash[3] == 0xDD && 
                          gpu_debug_hash[4] == 0xEE && gpu_debug_hash[5] == 0xFF) {
                    std::cout << "DEBUG: IMMEDIATE WRITE TEST PASSED - Kernel can write to buffer!" << std::endl;
                    
                    // Check global ID at positions 8-11
                    uint32_t gid = (uint32_t)gpu_debug_hash[8] | 
                                  ((uint32_t)gpu_debug_hash[9] << 8) |
                                  ((uint32_t)gpu_debug_hash[10] << 16) |
                                  ((uint32_t)gpu_debug_hash[11] << 24);
                    std::cout << "DEBUG: Global ID written at positions 8-11: " << gid << std::endl;
                    
                    // Check nonce reading test (positions 12-15)
                    if (gpu_debug_hash[12] == 0x17 && gpu_debug_hash[13] == 0x39 && 
                        gpu_debug_hash[14] == 0x71 && gpu_debug_hash[15] == 0x16) {
                        std::cout << "DEBUG: Nonce reading test PASSED - kernel can read test_nonce parameter" << std::endl;
                    } else {
                        std::cout << "DEBUG: Nonce reading test FAILED" << std::endl;
                    }
                    
                    // Check header reading test (positions 16-17)
                    if (gpu_debug_hash[16] == 0x01 && gpu_debug_hash[17] == 0x00) {
                        std::cout << "DEBUG: Header reading test PASSED - kernel can read header80 parameter" << std::endl;
                    } else {
                        std::cout << "DEBUG: Header reading test FAILED" << std::endl;
                        std::cout << "DEBUG: Expected header[16]=0x01, got 0x" << std::hex << (int)gpu_debug_hash[16] << std::endl;
                        std::cout << "DEBUG: Expected header[17]=0x00, got 0x" << std::hex << (int)gpu_debug_hash[17] << std::dec << std::endl;
                    }
                    
                } else {
                    std::cout << "DEBUG: Buffer was modified but not as expected" << std::endl;
                    std::cout << "DEBUG: Expected pattern: AA BB CC DD EE FF" << std::endl;
                    std::cout << "DEBUG: Got pattern: ";
                    for (int i = 0; i < 18; ++i) {
                        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)gpu_debug_hash[i] << " ";
                    }
                    std::cout << std::dec << std::endl;
                }
                
                // 转换为大端序
                std::string gpu_debug_be;
                for (int i = 31; i >= 0; --i) {
                    char buf[3];
                    sprintf(buf, "%02x", gpu_debug_hash[i]);
                    gpu_debug_be += buf;
                }
                std::cout << "GPU Debug Hash (BE): " << gpu_debug_be << std::endl;
                std::cout << "Expected (BE): 00000c62fac2d483d65c37331a3a73c6f315de2541e7384e94e36d3b1491604f" << std::endl;
                
                bool gpu_debug_matches = (gpu_debug_be == "00000c62fac2d483d65c37331a3a73c6f315de2541e7384e94e36d3b1491604f");
                std::cout << "GPU Debug Hash Match: " << (gpu_debug_matches ? "YES" : "NO") << std::endl;
            } else {
                std::cout << "GPU debug kernel execution failed." << std::endl;
            }
        } else {
            std::cout << "GPU debug kernel not available." << std::endl;
        }
        
        std::cout << "\nGPU Verification Result: PARTIAL" << std::endl;
        std::cout << "Genesis block creation is correct, but GPU needs debugging" << std::endl;
        std::cout << "GPU mining function may have issues finding the solution" << std::endl;
        std::cout << "Check GPU kernel implementation and work distribution" << std::endl;
        
        return false;
        
    } catch (const std::exception& e) {
        std::cerr << "FAILED" << std::endl;
        std::cerr << "Failed to initialize OpenCL for GPU test" << std::endl;
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }
}

int main(int argc, char* argv[])
{
    // Parse arguments
    std::string kernel_path;
    bool verify_mode = false;
    bool gpu_mode = false;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--kernel=", 0) == 0) {
            kernel_path = arg.substr(std::string("--kernel=").size());
        } else if (arg == "-k" && i + 1 < argc) {
            kernel_path = argv[++i];
        } else if (arg == "-verify") {
            verify_mode = true;
        } else if (arg == "-gpu") {
            gpu_mode = true;
        }
    }
    
    // Default kernel path if not provided
    if (kernel_path.empty()) {
        kernel_path = "src/tools/randomq_kernel.cl";
    }
    
    // Handle verification mode
    if (verify_mode && gpu_mode) {
        return VerifyGenesisBlock() ? 0 : 1;
    }
    
    // Regular mining mode
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
        VectorWriter(ser, 0, block.nVersion, block.hashPrevBlock, block.hashMerkleRoot, block.nTime, block.nBits, block.nNonce);
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
            for (unsigned char b : gpu_hash) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)b;
            }
            std::cout << std::dec;
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
