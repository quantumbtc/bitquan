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
#include <crypto/sha256.h>
#include <primitives/block.h>
#include <consensus/merkle.h>
#include <chainparamsbase.h>
#include <support/events.h>
#include <core_io.h>
#include <netbase.h>
#include <key_io.h>
#include <streams.h>
#include <serialize.h>

#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#endif

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/http.h>

#ifdef CUDA_FOUND
// CUDA headers
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#endif

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <memory>

using namespace std::chrono_literals;

// Provide translation function symbol expected by common/init.cpp link
const TranslateFn G_TRANSLATION_FUN{nullptr};

#ifdef CUDA_FOUND
// CUDA error checking macro
#define CUDA_CHECK(call) do { \
    cudaError_t err = call; \
    if (err != cudaSuccess) { \
        throw std::runtime_error(std::string("CUDA error: ") + cudaGetErrorString(err)); \
    } \
} while(0)
#endif

#ifdef CUDA_FOUND
// GPU device information
struct GPUDevice {
    int device_id;
    std::string name;
    size_t memory;
    int compute_capability_major;
    int compute_capability_minor;
    int multiprocessor_count;
    int max_threads_per_block;
    bool is_available;
};

// CUDA mining context
struct CudaMinerContext {
    int device_id;
    cudaStream_t stream;
    
    // Device memory pointers
    uint8_t* d_header;      // 80 bytes block header
    uint32_t* d_nonce_base; // base nonce
    uint8_t* d_target;      // 32 bytes target
    uint32_t* d_found_flag; // found flag
    uint32_t* d_found_nonce;// found nonce
    uint8_t* d_result_hash; // 32 bytes result hash
    
    // Host memory
    uint32_t h_found_flag;
    uint32_t h_found_nonce;
    uint8_t h_result_hash[32];
    
    CudaMinerContext() : device_id(-1), stream(nullptr), 
                        d_header(nullptr), d_nonce_base(nullptr), d_target(nullptr),
                        d_found_flag(nullptr), d_found_nonce(nullptr), d_result_hash(nullptr),
                        h_found_flag(0), h_found_nonce(0) {
        memset(h_result_hash, 0, 32);
    }
};
#endif

static const char DEFAULT_RPCCONNECT[] = "127.0.0.1";
static const int DEFAULT_HTTP_CLIENT_TIMEOUT = 900;

static std::atomic<bool> g_stop{false};

#ifdef CUDA_FOUND
// Forward declarations
extern "C" {
    // CUDA kernel wrapper functions (implemented in .cu file)
    void launch_randomq_kernel(
        uint8_t* d_header,
        uint32_t* d_nonce_base, 
        uint8_t* d_target,
        uint32_t* d_found_flag,
        uint32_t* d_found_nonce,
        uint8_t* d_result_hash,
        uint32_t grid_size,
        uint32_t block_size,
        cudaStream_t stream
    );
    
    void cuda_device_query();
}
#endif

static void SetupMinerArgs(ArgsManager& argsman)
{
    SetupHelpOptions(argsman);
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
    argsman.AddArg("-address=<bech32>", "Payout address for coinbase", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-maxtries=<n>", "Max nonce attempts before refreshing template (default: 1000000)", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-gpu=<n>", "GPU device ID to use (default: 0)", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-gputhreads=<n>", "GPU threads per block (default: 256)", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-gpublocks=<n>", "GPU blocks per grid (default: auto-detect)", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-list-gpus", "List available GPU devices and exit", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
}

#ifdef CUDA_FOUND
// GPU device detection and management
static std::vector<GPUDevice> DetectGPUDevices()
{
    std::vector<GPUDevice> devices;
    
    int device_count = 0;
    cudaError_t err = cudaGetDeviceCount(&device_count);
    if (err != cudaSuccess) {
        tfm::format(std::cout, "[GPU] No CUDA devices found: %s\n", cudaGetErrorString(err));
        return devices;
    }
    
    tfm::format(std::cout, "[GPU] Found %d CUDA device(s)\n", device_count);
    
    for (int i = 0; i < device_count; ++i) {
        GPUDevice device;
        device.device_id = i;
        device.is_available = false;
        
        cudaDeviceProp prop;
        err = cudaGetDeviceProperties(&prop, i);
        if (err != cudaSuccess) {
            tfm::format(std::cout, "[GPU] Failed to get properties for device %d: %s\n", i, cudaGetErrorString(err));
            continue;
        }
        
        device.name = prop.name;
        device.memory = prop.totalGlobalMem;
        device.compute_capability_major = prop.major;
        device.compute_capability_minor = prop.minor;
        device.multiprocessor_count = prop.multiProcessorCount;
        device.max_threads_per_block = prop.maxThreadsPerBlock;
        
        // Check if device is suitable for mining (compute capability >= 3.0)
        if (prop.major >= 3) {
            device.is_available = true;
        }
        
        devices.push_back(device);
        
        tfm::format(std::cout, "[GPU] Device %d: %s\n", i, prop.name);
        tfm::format(std::cout, "      Memory: %.1f MB\n", (double)prop.totalGlobalMem / (1024 * 1024));
        tfm::format(std::cout, "      Compute Capability: %d.%d\n", prop.major, prop.minor);
        tfm::format(std::cout, "      Multiprocessors: %d\n", prop.multiProcessorCount);
        tfm::format(std::cout, "      Max Threads/Block: %d\n", prop.maxThreadsPerBlock);
        tfm::format(std::cout, "      Available: %s\n", device.is_available ? "Yes" : "No (requires compute capability >= 3.0)");
    }
    
    return devices;
}

static void InitializeCudaContext(CudaMinerContext& ctx, int device_id)
{
    tfm::format(std::cout, "[GPU] Initializing CUDA context on device %d\n", device_id);
    
    CUDA_CHECK(cudaSetDevice(device_id));
    ctx.device_id = device_id;
    
    // Create CUDA stream
    CUDA_CHECK(cudaStreamCreate(&ctx.stream));
    
    // Allocate device memory
    CUDA_CHECK(cudaMalloc(&ctx.d_header, 80));         // block header
    CUDA_CHECK(cudaMalloc(&ctx.d_nonce_base, sizeof(uint32_t))); // base nonce
    CUDA_CHECK(cudaMalloc(&ctx.d_target, 32));         // target
    CUDA_CHECK(cudaMalloc(&ctx.d_found_flag, sizeof(uint32_t))); // found flag
    CUDA_CHECK(cudaMalloc(&ctx.d_found_nonce, sizeof(uint32_t))); // found nonce
    CUDA_CHECK(cudaMalloc(&ctx.d_result_hash, 32));    // result hash
    
    tfm::format(std::cout, "[GPU] CUDA context initialized successfully\n");
}

static void CleanupCudaContext(CudaMinerContext& ctx)
{
    if (ctx.device_id >= 0) {
        cudaSetDevice(ctx.device_id);
        
        if (ctx.d_header) cudaFree(ctx.d_header);
        if (ctx.d_nonce_base) cudaFree(ctx.d_nonce_base);
        if (ctx.d_target) cudaFree(ctx.d_target);
        if (ctx.d_found_flag) cudaFree(ctx.d_found_flag);
        if (ctx.d_found_nonce) cudaFree(ctx.d_found_nonce);
        if (ctx.d_result_hash) cudaFree(ctx.d_result_hash);
        
        if (ctx.stream) cudaStreamDestroy(ctx.stream);
        
        ctx = CudaMinerContext(); // Reset
    }
}

// RPC client implementation (same as cpuminer)
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

static UniValue DoRpcRequest(const std::string& method, const UniValue& params_arr)
{
    std::string host;
    uint16_t port{BaseParams().RPCPort()};
    uint16_t rpcconnect_port{0};
    const std::string rpcconnect_str = gArgs.GetArg("-rpcconnect", DEFAULT_RPCCONNECT);
    if (!SplitHostPort(rpcconnect_str, rpcconnect_port, host)) {
        throw std::runtime_error(strprintf("Invalid -rpcconnect: %s", rpcconnect_str));
    }
    if (std::optional<std::string> port_arg = gArgs.GetArg("-rpcport")) {
        const auto parsed = ToIntegral<uint16_t>(*port_arg);
        if (!parsed) throw std::runtime_error("Invalid -rpcport");
        port = *parsed;
    } else if (rpcconnect_port != 0) {
        port = rpcconnect_port;
    }

    raii_event_base base = obtain_event_base();
    raii_evhttp_connection evcon = obtain_evhttp_connection_base(base.get(), host, port);

    const int timeout = gArgs.GetIntArg("-rpcclienttimeout", DEFAULT_HTTP_CLIENT_TIMEOUT);
    if (timeout > 0) evhttp_connection_set_timeout(evcon.get(), timeout);

    HTTPReply response;
    raii_evhttp_request req = obtain_evhttp_request(http_request_done, (void*)&response);
    if (!req) throw std::runtime_error("create http request failed");

    std::string auth = GetAuth();
    struct evkeyvalq* headers = evhttp_request_get_output_headers(req.get());
    evhttp_add_header(headers, "Host", host.c_str());
    evhttp_add_header(headers, "Connection", "close");
    evhttp_add_header(headers, "Content-Type", "application/json");
    evhttp_add_header(headers, "Authorization", (std::string("Basic ") + EncodeBase64(auth)).c_str());

    UniValue body = JSONRPCRequestObj(method, params_arr, UniValue(1));
    std::string body_str = body.write() + "\n";
    struct evbuffer* out = evhttp_request_get_output_buffer(req.get());
    evbuffer_add(out, body_str.data(), body_str.size());

    int r = evhttp_make_request(evcon.get(), req.get(), EVHTTP_REQ_POST, "/");
    req.release();
    if (r != 0) throw std::runtime_error("send http request failed");
    event_base_dispatch(base.get());

    if (response.status == 0) throw std::runtime_error("RPC connection failed");
    UniValue reply;
    if (!reply.read(response.body)) throw std::runtime_error("Invalid RPC response");
    return reply;
}

} // namespace

// RPC functions accessible outside the anonymous namespace
UniValue RpcCallWaitParams(const std::string& method, const UniValue& params_arr)
{
    const bool fWait = gArgs.GetBoolArg("-rpcwait", false);
    const int timeout = gArgs.GetIntArg("-rpcwaittimeout", 0);
    auto deadline = std::chrono::steady_clock::now() + 1s * timeout;
    while (true) {
        try {
            return DoRpcRequest(method, params_arr);
        } catch (...) {
            if (!fWait) throw;
            if (timeout > 0 && std::chrono::steady_clock::now() >= deadline) {
                throw std::runtime_error("timeout waiting for RPC server");
            }
            UninterruptibleSleep(1s);
        }
    }
}

UniValue RpcCallWait(const std::string& method, const std::vector<std::string>& params)
{
    UniValue arr(UniValue::VARR);
    for (const auto& p : params) arr.push_back(p);
    return RpcCallWaitParams(method, arr);
}

// Block template building (same as cpuminer)
bool BuildBlockFromGBT(const UniValue& gbt_res, CBlock& block, std::string& tmpl_hex_out)
{
    const UniValue hexv = gbt_res.find_value("hex");
    if (!hexv.isNull()) {
        tmpl_hex_out = hexv.get_str();
        return DecodeHexBlk(block, tmpl_hex_out);
    }
    
    // Header
    if (!gbt_res["version"].isNull()) block.nVersion = gbt_res["version"].getInt<int>();
    if (!gbt_res["previousblockhash"].isNull()) block.hashPrevBlock = uint256::FromHex(gbt_res["previousblockhash"].get_str()).value_or(uint256{});
    if (!gbt_res["curtime"].isNull()) block.nTime = gbt_res["curtime"].getInt<int>();
    if (!gbt_res["bits"].isNull()) block.nBits = (uint32_t)std::stoul(gbt_res["bits"].get_str(), nullptr, 16);
    block.nNonce = 0;
    
    // Build coinbase and transactions (simplified version)
    block.vtx.clear();
    
    // Build coinbase locally
    CAmount cb_value = 0;
    if (!gbt_res["coinbasevalue"].isNull()) {
        cb_value = gbt_res["coinbasevalue"].getInt<int64_t>();
    }
    if (gbt_res["height"].isNull()) throw std::runtime_error("GBT missing height for local coinbase");
    int32_t height = gbt_res["height"].getInt<int>();
    
    CMutableTransaction coinbase;
    coinbase.version = 1;
    
    // scriptSig: BIP34 height
    CScript sig;
    if (height <= 16) {
        sig << (height == 0 ? OP_0 : (opcodetype)(OP_1 + height - 1));
        sig << OP_0;
    } else {
        sig << CScriptNum(height);
    }
    coinbase.vin.emplace_back(CTxIn(COutPoint(), sig));
    
    // payout
    const std::string addr_str = gArgs.GetArg("-address", "");
    CTxDestination dest = DecodeDestination(addr_str);
    if (!IsValidDestination(dest)) throw std::runtime_error("invalid mining address for coinbase");
    CScript payout = GetScriptForDestination(dest);
    coinbase.vout.emplace_back(CTxOut(cb_value, payout));
    
    block.vtx.push_back(MakeTransactionRef(std::move(coinbase)));
    
    // Add other transactions
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
    
    // Finalize merkle root
    block.hashMerkleRoot = BlockMerkleRoot(block);
    return true;
}

// Encode block to hex string (similar to EncodeHexTx)
std::string EncodeHexBlk(const CBlock& block)
{
    DataStream ssBlk;
    ssBlk << TX_WITH_WITNESS(block);
    return HexStr(ssBlk);
}

// GPU mining function
static bool GPUMineBlock(CudaMinerContext& ctx, const CBlock& block_template, uint32_t start_nonce, 
                        uint32_t max_nonce, uint32_t& found_nonce, uint256& pow_hash)
{
    // Serialize block header
    std::vector<uint8_t> header_data(80);
    VectorWriter vw(header_data, 0, static_cast<const CBlockHeader&>(block_template));
    
    // Calculate target from nBits
    arith_uint256 target;
    bool fNegative, fOverflow;
    target.SetCompact(block_template.nBits, &fNegative, &fOverflow);
    if (fNegative || fOverflow || target == 0) {
        throw std::runtime_error("Invalid target difficulty");
    }
    
    // Convert target to little-endian bytes for GPU
    uint256 target_le = ArithToUint256(target);
    std::vector<uint8_t> target_data(target_le.begin(), target_le.end());
    
    // Copy data to GPU
    CUDA_CHECK(cudaMemcpyAsync(ctx.d_header, header_data.data(), 80, cudaMemcpyHostToDevice, ctx.stream));
    CUDA_CHECK(cudaMemcpyAsync(ctx.d_nonce_base, &start_nonce, sizeof(uint32_t), cudaMemcpyHostToDevice, ctx.stream));
    CUDA_CHECK(cudaMemcpyAsync(ctx.d_target, target_data.data(), 32, cudaMemcpyHostToDevice, ctx.stream));
    
    // Reset found flag
    ctx.h_found_flag = 0;
    CUDA_CHECK(cudaMemcpyAsync(ctx.d_found_flag, &ctx.h_found_flag, sizeof(uint32_t), cudaMemcpyHostToDevice, ctx.stream));
    
    // Calculate grid and block sizes
    uint32_t threads_per_block = gArgs.GetIntArg("-gputhreads", 256);
    uint32_t nonce_range = max_nonce - start_nonce;
    uint32_t grid_size = gArgs.GetIntArg("-gpublocks", (nonce_range + threads_per_block - 1) / threads_per_block);
    
    // Limit grid size to prevent kernel timeout
    if (grid_size > 65535) grid_size = 65535;
    
    // Launch kernel
    launch_randomq_kernel(ctx.d_header, ctx.d_nonce_base, ctx.d_target,
                         ctx.d_found_flag, ctx.d_found_nonce, ctx.d_result_hash,
                         grid_size, threads_per_block, ctx.stream);
    
    // Wait for completion
    CUDA_CHECK(cudaStreamSynchronize(ctx.stream));
    
    // Check for kernel errors
    cudaError_t kernel_err = cudaGetLastError();
    if (kernel_err != cudaSuccess) {
        throw std::runtime_error(std::string("CUDA kernel error: ") + cudaGetErrorString(kernel_err));
    }
    
    // Copy results back
    CUDA_CHECK(cudaMemcpyAsync(&ctx.h_found_flag, ctx.d_found_flag, sizeof(uint32_t), cudaMemcpyDeviceToHost, ctx.stream));
    CUDA_CHECK(cudaMemcpyAsync(&ctx.h_found_nonce, ctx.d_found_nonce, sizeof(uint32_t), cudaMemcpyDeviceToHost, ctx.stream));
    CUDA_CHECK(cudaMemcpyAsync(ctx.h_result_hash, ctx.d_result_hash, 32, cudaMemcpyDeviceToHost, ctx.stream));
    CUDA_CHECK(cudaStreamSynchronize(ctx.stream));
    
    if (ctx.h_found_flag != 0) {
        found_nonce = ctx.h_found_nonce;
        // Convert result hash back to uint256
        pow_hash = uint256(std::vector<uint8_t>(ctx.h_result_hash, ctx.h_result_hash + 32));
        return true;
    }
    
    return false;
}

// Main mining loop
static void GPUMinerLoop()
{
    const std::string payout = gArgs.GetArg("-address", "");
    if (payout.empty()) {
        throw std::runtime_error("-address is required");
    }
    
    const int maxtries = gArgs.GetIntArg("-maxtries", 1000000);
    const int gpu_device = gArgs.GetIntArg("-gpu", 0);
    
    // Detect and initialize GPU
    std::vector<GPUDevice> devices = DetectGPUDevices();
    if (devices.empty()) {
        throw std::runtime_error("No CUDA devices found");
    }
    
    if (gpu_device >= (int)devices.size() || !devices[gpu_device].is_available) {
        throw std::runtime_error(strprintf("GPU device %d not available", gpu_device));
    }
    
    tfm::format(std::cout, "[GPU] Using device %d: %s\n", gpu_device, devices[gpu_device].name);
    
    CudaMinerContext cuda_ctx;
    InitializeCudaContext(cuda_ctx, gpu_device);
    
    std::atomic<uint64_t> total_hashes{0};
    std::atomic<uint64_t> window_hashes{0};
    uint64_t start_time = (uint64_t)GetTime();
    
    // Hash rate reporter thread
    std::thread reporter([&](){
        while (!g_stop.load()) {
            uint64_t now = (uint64_t)GetTime();
            uint64_t elapsed = now - start_time;
            double avg = elapsed ? (double)total_hashes.load() / (double)elapsed : 0.0;
            double cur = window_hashes.exchange(0) / 5.0;
            tfm::format(std::cout, "[HashRate] Current: %.2f H/s | Average: %.2f H/s | Total: %llu\n", 
                       cur, avg, (unsigned long long)total_hashes.load());
            std::cout.flush();
            for (int i = 0; i < 5 && !g_stop.load(); ++i) std::this_thread::sleep_for(1s);
        }
    });
    
    try {
        while (!g_stop.load()) {
            // Get block template
            UniValue rules(UniValue::VARR); rules.push_back("segwit");
            UniValue caps(UniValue::VARR); caps.push_back("coinbasetxn");
            UniValue req(UniValue::VOBJ);
            req.pushKV("rules", rules);
            req.pushKV("capabilities", caps);
            UniValue params_arr(UniValue::VARR); params_arr.push_back(req);
            UniValue gbt = RpcCallWaitParams("getblocktemplate", params_arr);
            
            const UniValue err = gbt.find_value("error");
            if (!err.isNull()) {
                std::string error_msg = err.write();
                if (error_msg.find("not connected") != std::string::npos || 
                    error_msg.find("connection") != std::string::npos ||
                    error_msg.find("timeout") != std::string::npos) {
                    tfm::format(std::cout, "[Info] Node connection lost, retrying in 5 seconds...\n");
                    std::cout.flush();
                    for (int i = 0; i < 5 && !g_stop.load(); ++i) {
                        std::this_thread::sleep_for(1s);
                    }
                    continue;
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
                continue;
            }
            
            CBlock block;
            std::string tmpl_hex;
            if (!BuildBlockFromGBT(res, block, tmpl_hex)) {
                UniValue reply = RpcCallWait("generatetoaddress", {"1", payout, std::to_string(maxtries)});
                continue;
            }
            
            // Print template info
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
            
            // GPU mining
            uint32_t start_nonce = 0;
            uint32_t nonce_step = 1000000; // Process 1M nonces at a time
            bool found = false;
            uint32_t found_nonce = 0;
            uint256 pow_hash;
            
            for (uint32_t nonce_base = start_nonce; nonce_base < (uint32_t)maxtries && !g_stop.load() && !found; nonce_base += nonce_step) {
                uint32_t max_nonce = std::min(nonce_base + nonce_step, (uint32_t)maxtries);
                
                try {
                    found = GPUMineBlock(cuda_ctx, block, nonce_base, max_nonce, found_nonce, pow_hash);
                    uint32_t hashes_done = max_nonce - nonce_base;
                    window_hashes.fetch_add(hashes_done, std::memory_order_relaxed);
                    total_hashes.fetch_add(hashes_done, std::memory_order_relaxed);
                } catch (const std::exception& e) {
                    tfm::format(std::cout, "[GPU] Mining error: %s\n", e.what());
                    std::cout.flush();
                    break;
                }
            }
            
            if (found) {
                block.nNonce = found_nonce;
                
                // Verify the solution
                uint256 verify_hash = RandomQMining::CalculateRandomQHashOptimized(block, block.nNonce);
                arith_uint256 target_check; bool neg_check=false, of_check=false; 
                target_check.SetCompact(block.nBits, &neg_check, &of_check);
                
                if (!neg_check && !of_check && target_check != 0 && UintToArith256(verify_hash) <= target_check) {
                    tfm::format(std::cout,
                        "[Found] height=%d nonce=%u time=%u bits=%08x target=%s powhash=%s merkle=%s\n",
                        height,
                        (unsigned)block.nNonce,
                        (unsigned)block.nTime,
                        (unsigned)block.nBits,
                        target_check.GetHex().c_str(),
                        verify_hash.GetHex().c_str(),
                        block.hashMerkleRoot.GetHex().c_str());
                    std::cout.flush();
                    
                    // Submit block
                    std::string block_hex = EncodeHexBlk(block);
                    UniValue sub = RpcCallWait("submitblock", {block_hex});
                    
                    const UniValue err_sub = sub.find_value("error");
                    const UniValue resv = sub.find_value("result");
                    if (!err_sub.isNull()) {
                        std::string emsg = err_sub.isObject() && !err_sub.find_value("message").isNull() ? 
                                          err_sub.find_value("message").get_str() : err_sub.write();
                        tfm::format(std::cout, "[Submit] result=%s error=%s\n", 
                                   resv.isNull() ? "null" : resv.write().c_str(), emsg.c_str());
                    } else {
                        tfm::format(std::cout, "[Submit] result=%s error=null\n", 
                                   resv.isNull() ? "null" : resv.write().c_str());
                    }
                    std::cout.flush();
                } else {
                    tfm::format(std::cout, "[GPU] Invalid solution found, hash does not meet target\n");
                    std::cout.flush();
                }
            }
        }
    } catch (const std::exception& e) {
        g_stop.store(true);
        tfm::format(std::cerr, "[GPU] Mining loop error: %s\n", e.what());
    }
    
    CleanupCudaContext(cuda_ctx);
    reporter.join();
}
#else
// CPU-only mining loop when CUDA is not available
static void GPUMinerLoop()
{
    tfm::format(std::cout, "[INFO] CUDA not available, using CPU-only mining\n");
    tfm::format(std::cout, "[INFO] This version provides basic mining functionality without GPU acceleration\n");
    
    const std::string payout = gArgs.GetArg("-address", "");
    if (payout.empty()) {
        throw std::runtime_error("-address is required");
    }
    
    const int maxtries = gArgs.GetIntArg("-maxtries", 1000000);
    
    std::atomic<uint64_t> total_hashes{0};
    std::atomic<uint64_t> window_hashes{0};
    uint64_t start_time = (uint64_t)GetTime();
    
    // Hash rate reporter thread
    std::thread reporter([&](){
        while (!g_stop.load()) {
            uint64_t now = (uint64_t)GetTime();
            uint64_t elapsed = now - start_time;
            double avg = elapsed ? (double)total_hashes.load() / (double)elapsed : 0.0;
            double cur = window_hashes.exchange(0) / 5.0;
            tfm::format(std::cout, "[HashRate] Current: %.2f H/s | Average: %.2f H/s | Total: %llu\n", 
                       cur, avg, (unsigned long long)total_hashes.load());
            std::cout.flush();
            for (int i = 0; i < 5 && !g_stop.load(); ++i) std::this_thread::sleep_for(1s);
        }
    });
    
    try {
        while (!g_stop.load()) {
            // Get block template
            UniValue rules(UniValue::VARR); rules.push_back("segwit");
            UniValue caps(UniValue::VARR); caps.push_back("coinbasetxn");
            UniValue req(UniValue::VOBJ);
            req.pushKV("rules", rules);
            req.pushKV("capabilities", caps);
            UniValue params_arr(UniValue::VARR); params_arr.push_back(req);
            UniValue gbt = RpcCallWaitParams("getblocktemplate", params_arr);
            
            const UniValue err = gbt.find_value("error");
            if (!err.isNull()) {
                std::string error_msg = err.write();
                if (error_msg.find("not connected") != std::string::npos || 
                    error_msg.find("connection") != std::string::npos ||
                    error_msg.find("timeout") != std::string::npos) {
                    tfm::format(std::cout, "[Info] Node connection lost, retrying in 5 seconds...\n");
                    std::cout.flush();
                    for (int i = 0; i < 5 && !g_stop.load(); ++i) {
                        std::this_thread::sleep_for(1s);
                    }
                    continue;
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
                continue;
            }
            
            CBlock block;
            std::string tmpl_hex;
            if (!BuildBlockFromGBT(res, block, tmpl_hex)) {
                UniValue reply = RpcCallWait("generatetoaddress", {"1", payout, std::to_string(maxtries)});
                continue;
            }
            
            // Print template info
            int32_t height = res.find_value("height").isNull() ? -1 : res.find_value("height").getInt<int>();
            arith_uint256 target; bool neg=false, of=false; target.SetCompact(block.nBits, &neg, &of);
            tfm::format(std::cout,
                "[Template] height=%d version=%d prev=%s time=%u bits=%08x target=%s txs=%u merkle=%s\n",
                height, block.nVersion, block.hashPrevBlock.GetHex().c_str(),
                (unsigned)block.nTime, (unsigned)block.nBits, target.GetHex().c_str(),
                (unsigned)block.vtx.size(), block.hashMerkleRoot.GetHex().c_str());
            std::cout.flush();
            
            // CPU mining (simplified version)
            bool found = false;
            for (uint32_t nonce = 0; nonce < (uint32_t)maxtries && !g_stop.load() && !found; ++nonce) {
                block.nNonce = nonce;
                uint256 hash = RandomQMining::CalculateRandomQHashOptimized(block, block.nNonce);
                window_hashes.fetch_add(1, std::memory_order_relaxed);
                total_hashes.fetch_add(1, std::memory_order_relaxed);
                
                if (!neg && !of && target != 0 && UintToArith256(hash) <= target) {
                    found = true;
                    
                    tfm::format(std::cout,
                        "[Found] height=%d nonce=%u time=%u bits=%08x target=%s powhash=%s merkle=%s\n",
                        height, (unsigned)block.nNonce, (unsigned)block.nTime,
                        (unsigned)block.nBits, target.GetHex().c_str(),
                        hash.GetHex().c_str(), block.hashMerkleRoot.GetHex().c_str());
                    std::cout.flush();
                    
                    // Submit block
                    std::string block_hex = EncodeHexBlk(block);
                    UniValue sub = RpcCallWait("submitblock", {block_hex});
                    
                    const UniValue err_sub = sub.find_value("error");
                    const UniValue resv = sub.find_value("result");
                    if (!err_sub.isNull()) {
                        std::string emsg = err_sub.isObject() && !err_sub.find_value("message").isNull() ? 
                                          err_sub.find_value("message").get_str() : err_sub.write();
                        tfm::format(std::cout, "[Submit] result=%s error=%s\n", 
                                   resv.isNull() ? "null" : resv.write().c_str(), emsg.c_str());
                    } else {
                        tfm::format(std::cout, "[Submit] result=%s error=null\n", 
                                   resv.isNull() ? "null" : resv.write().c_str());
                    }
                    std::cout.flush();
                }
            }
        }
    } catch (const std::exception& e) {
        g_stop.store(true);
        tfm::format(std::cerr, "[CPU] Mining loop error: %s\n", e.what());
    }
    
    reporter.join();
}
#endif

int main(int argc, char* argv[])
{
#ifdef WIN32
    common::WinCmdLineArgs winArgs;
    std::tie(argc, argv) = winArgs.get();
#endif
    SetupEnvironment();
    if (!SetupNetworking()) {
        tfm::format(std::cerr, "Error: networking init failed\n");
        return EXIT_FAILURE;
    }
    
    try {
        SetupMinerArgs(gArgs);
        std::string error;
        if (!gArgs.ParseParameters(argc, argv, error)) {
            if (error != "") tfm::format(std::cerr, "Error parsing command line: %s\n", error);
            return EXIT_FAILURE;
        }
        
        // Handle list-gpus option
        if (gArgs.GetBoolArg("-list-gpus", false)) {
#ifdef CUDA_FOUND
            DetectGPUDevices();
#else
            tfm::format(std::cout, "[INFO] CUDA not available in this build\n");
            tfm::format(std::cout, "[INFO] This is a CPU-only version\n");
            tfm::format(std::cout, "[INFO] To use NVIDIA GPUs, please:\n");
            tfm::format(std::cout, "       1. Install CUDA Toolkit\n");
            tfm::format(std::cout, "       2. Recompile with CUDA support\n");
#endif
            return EXIT_SUCCESS;
        }
        
        SelectBaseParams(gArgs.GetChainType());
        if (auto cfgerr = common::InitConfig(gArgs, nullptr)) {
            // ignore config failure for standalone mode
        }
        
        std::signal(SIGINT, [](int){ g_stop.store(true); });
#ifdef SIGTERM
        std::signal(SIGTERM, [](int){ g_stop.store(true); });
#endif
        
        GPUMinerLoop();
    } catch (const std::exception& e) {
        tfm::format(std::cerr, "gpuminer-randomq error: %s\n", e.what());
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
