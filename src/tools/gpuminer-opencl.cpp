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

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/http.h>

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
#include <cstring>
#include <span>

#ifdef OPENCL_FOUND
#include <CL/cl.h>
#endif

using namespace std::chrono_literals;

const TranslateFn G_TRANSLATION_FUN{nullptr};

static std::atomic<bool> g_stop{false};

static void SetupMinerArgs(ArgsManager& argsman)
{
	SetupHelpOptions(argsman);
	const auto defaultBaseParams = CreateBaseChainParams(ChainType::MAIN);
	const auto testnetBaseParams = CreateBaseChainParams(ChainType::TESTNET);
	const auto testnet4BaseParams = CreateBaseChainParams(ChainType::TESTNET4);
	const auto signetBaseParams = CreateBaseChainParams(ChainType::SIGNET);
	const auto regtestBaseParams = CreateBaseChainParams(ChainType::REGTEST);

	argsman.AddArg("-rpcconnect=<ip>", strprintf("Send RPCs to node at <ip> (default: %s)", "127.0.0.1"), ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
	argsman.AddArg("-rpcport=<port>", strprintf("RPC port (default: %u, testnet: %u, testnet4: %u, signet: %u, regtest: %u)", defaultBaseParams->RPCPort(), testnetBaseParams->RPCPort(), testnet4BaseParams->RPCPort(), signetBaseParams->RPCPort(), regtestBaseParams->RPCPort()), ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
	argsman.AddArg("-rpcuser=<user>", "RPC username", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
	argsman.AddArg("-rpcpassword=<pw>", "RPC password (omit to use cookie)", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
	argsman.AddArg("-rpccookiefile=<loc>", "RPC cookie file (defaults to datadir)", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
	argsman.AddArg("-rpcwait", "Wait for RPC server to be ready", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
	argsman.AddArg("-rpcwaittimeout=<n>", "Timeout in seconds to wait for RPC server (0 = forever)", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
	argsman.AddArg("-address=<bech32>", "Payout address for coinbase", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
	argsman.AddArg("-maxtries=<n>", "Max nonce attempts before refreshing template (default: 1000000)", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
	argsman.AddArg("-gpu=<n>", "Select OpenCL device index (default: 0)", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
	argsman.AddArg("-list-gpus", "List OpenCL platforms/devices and exit", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
	argsman.AddArg("-cpu-fallback", "Force CPU mining loop (bypass OpenCL)", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
}

namespace {

struct HTTPReply { int status = 0; std::string body; };

static void http_request_done(struct evhttp_request* req, void* ctx)
{
	HTTPReply* reply = static_cast<HTTPReply*>(ctx);
	if (!req) { reply->status = 0; reply->body = ""; return; }
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
	const std::string rpcconnect_str = gArgs.GetArg("-rpcconnect", "127.0.0.1");
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

	const int timeout = gArgs.GetIntArg("-rpcclienttimeout", 900);
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

static UniValue RpcCall(const std::string& method, const std::vector<std::string>& params)
{
	UniValue arr(UniValue::VARR);
	for (const auto& p : params) arr.push_back(p);
	return DoRpcRequest(method, arr);
}

static UniValue RpcCallParams(const std::string& method, const UniValue& params_arr)
{
	return DoRpcRequest(method, params_arr);
}

static UniValue RpcCallWaitParams(const std::string& method, const UniValue& params_arr)
{
	const bool fWait = gArgs.GetBoolArg("-rpcwait", false);
	const int timeout = gArgs.GetIntArg("-rpcwaittimeout", 0);
	auto deadline = std::chrono::steady_clock::now() + 1s * timeout;
	while (true) {
		try { return RpcCallParams(method, params_arr); }
		catch (...) {
			if (!fWait) throw;
			if (timeout > 0 && std::chrono::steady_clock::now() >= deadline) {
				throw std::runtime_error("timeout waiting for RPC server");
			}
			UninterruptibleSleep(1s);
		}
	}
}

} // end anonymous namespace

#ifdef OPENCL_FOUND
static void ListOpenCLDevices()
{
	cl_uint num_platforms = 0;
	clGetPlatformIDs(0, nullptr, &num_platforms);
	std::vector<cl_platform_id> platforms(num_platforms);
	clGetPlatformIDs(num_platforms, platforms.data(), nullptr);
	unsigned device_index = 0;
	std::cout << "[OpenCL] Found " << (unsigned)num_platforms << " platform(s)\n";
	for (cl_uint pi = 0; pi < num_platforms; ++pi) {
		char pname[256] = {0};
		clGetPlatformInfo(platforms[pi], CL_PLATFORM_NAME, sizeof(pname), pname, nullptr);
		std::cout << "Platform " << pi << ": " << pname << "\n";
		cl_uint num_devices = 0;
		clGetDeviceIDs(platforms[pi], CL_DEVICE_TYPE_GPU, 0, nullptr, &num_devices);
		std::vector<cl_device_id> devices(num_devices);
		clGetDeviceIDs(platforms[pi], CL_DEVICE_TYPE_GPU, num_devices, devices.data(), nullptr);
		for (cl_uint di = 0; di < num_devices; ++di) {
			char dname[256] = {0};
			clGetDeviceInfo(devices[di], CL_DEVICE_NAME, sizeof(dname), dname, nullptr);
			size_t gmem = 0; clGetDeviceInfo(devices[di], CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(gmem), &gmem, nullptr);
			std::cout << "  [GPU] Device " << device_index++ << ": " << dname << ", GlobalMem=" << (unsigned long long)gmem/(1024*1024) << " MB\n";
		}
	}
}
#else
static void ListOpenCLDevices() { std::cout << "OpenCL not available in this build\n"; }
#endif

static bool BuildBlockFromGBT(const UniValue& gbt_res, CBlock& block, std::string& tmpl_hex_out)
{
	const UniValue hexv = gbt_res.find_value("hex");
	if (!hexv.isNull()) {
		tmpl_hex_out = hexv.get_str();
		return DecodeHexBlk(block, tmpl_hex_out);
	}
	if (!gbt_res["version"].isNull()) block.nVersion = gbt_res["version"].getInt<int>();
	if (!gbt_res["previousblockhash"].isNull()) block.hashPrevBlock = uint256::FromHex(gbt_res["previousblockhash"].get_str()).value_or(uint256{});
	if (!gbt_res["curtime"].isNull()) block.nTime = gbt_res["curtime"].getInt<int>();
	if (!gbt_res["bits"].isNull()) block.nBits = (uint32_t)std::stoul(gbt_res["bits"].get_str(), nullptr, 16);
	block.nNonce = 0;
	block.vtx.clear();
	const UniValue coinbasetxn = gbt_res.find_value("coinbasetxn");
	if (coinbasetxn.isObject() && coinbasetxn.exists("data") && coinbasetxn.find_value("data").isStr()) {
		CMutableTransaction mtx; if (!DecodeHexTx(mtx, coinbasetxn.find_value("data").get_str())) return false; block.vtx.push_back(MakeTransactionRef(std::move(mtx)));
	} else {
		CAmount cb_value = 0; if (!gbt_res["coinbasevalue"].isNull()) cb_value = gbt_res["coinbasevalue"].getInt<int64_t>();
		if (gbt_res["height"].isNull()) throw std::runtime_error("GBT missing height for local coinbase");
		int32_t height = gbt_res["height"].getInt<int>();
		CMutableTransaction coinbase; coinbase.version = 1; CScript sig; if (height <= 16) { sig << (height == 0 ? OP_0 : (opcodetype)(OP_1 + height - 1)); sig << OP_0; } else { sig << CScriptNum(height); }
		coinbase.vin.emplace_back(CTxIn(COutPoint(), sig));
		{
			const std::string addr_str = gArgs.GetArg("-address", "");
			CTxDestination dest = DecodeDestination(addr_str);
			if (!IsValidDestination(dest)) throw std::runtime_error("invalid mining address for coinbase");
			CScript payout = GetScriptForDestination(dest);
			coinbase.vout.emplace_back(CTxOut(cb_value, payout));
		}
		block.vtx.push_back(MakeTransactionRef(std::move(coinbase)));
	}
	const UniValue txs = gbt_res.find_value("transactions");
	if (txs.isArray()) {
		for (size_t i = 0; i < txs.size(); ++i) {
			const UniValue& txo = txs[i]; if (!txo.isObject()) continue; const UniValue data = txo.find_value("data"); if (!data.isStr()) continue; CMutableTransaction mtx; if (!DecodeHexTx(mtx, data.get_str())) throw std::runtime_error("failed to decode tx from template"); block.vtx.push_back(MakeTransactionRef(std::move(mtx)));
		}
	}
	block.hashMerkleRoot = BlockMerkleRoot(block);
	return true;
}

static std::string UpdateNonceInBlockHex(const std::string& tmpl_hex, uint32_t nonce)
{
	if (tmpl_hex.size() < 160) throw std::runtime_error("template hex too short");
	std::string out = tmpl_hex; const size_t off = 76 * 2; unsigned char b0 = (unsigned char)(nonce & 0xFF); unsigned char b1 = (unsigned char)((nonce >> 8) & 0xFF); unsigned char b2 = (unsigned char)((nonce >> 16) & 0xFF); unsigned char b3 = (unsigned char)((nonce >> 24) & 0xFF);
	auto write_byte = [&](size_t pos, unsigned char b){ static const char* hexd = "0123456789abcdef"; out[pos+0] = hexd[(b >> 4) & 0xF]; out[pos+1] = hexd[b & 0xF]; };
	write_byte(off + 0, b0); write_byte(off + 2, b1); write_byte(off + 4, b2); write_byte(off + 6, b3); return out;
}

static std::string BuildFullBlockHex(const CBlock& block)
{
	std::vector<unsigned char> bytes; VectorWriter vw(bytes, 0, static_cast<const CBlockHeader&>(block)); WriteCompactSize(vw, block.vtx.size()); for (const auto& txref : block.vtx) { const std::string tx_hex = EncodeHexTx(*txref); std::vector<unsigned char> tx_bytes = ParseHex(tx_hex); vw.write(std::as_bytes(std::span<const unsigned char>(tx_bytes.data(), tx_bytes.size()))); } return HexStr(bytes);
}

#ifdef OPENCL_FOUND
struct OpenCLContext {
	cl_platform_id platform = nullptr;
	cl_device_id device = nullptr;
	cl_context context = nullptr;
	cl_command_queue queue = nullptr;
	cl_program program = nullptr;
	cl_kernel kernel = nullptr;
};

static const char* kOpenCLKernel = R"CLC(
__kernel void randomq_kernel(
    __global const uchar* header80,
    uint nonce_base,
    __global const uchar* target,
    __global volatile int* found_flag,
    __global uint* found_nonce
) {
    uint gid = get_global_id(0);
    uint nonce = nonce_base + gid;
    // NOTE: Placeholder: compute RandomQ(header, nonce) and compare to target
    // For now, do nothing here; real implementation should port RandomQ to OpenCL
    if (0) {
        if (atom_cmpxchg(found_flag, 0, 1) == 0) {
            *found_nonce = nonce;
        }
    }
}
)CLC";

static void ReleaseOpenCL(OpenCLContext& ctx)
{
	if (ctx.kernel) clReleaseKernel(ctx.kernel);
	if (ctx.program) clReleaseProgram(ctx.program);
	if (ctx.queue) clReleaseCommandQueue(ctx.queue);
	if (ctx.context) clReleaseContext(ctx.context);
}

static OpenCLContext CreateOpenCL(unsigned wanted_index)
{
	cl_uint num_platforms = 0; clGetPlatformIDs(0, nullptr, &num_platforms);
	std::vector<cl_platform_id> platforms(num_platforms); clGetPlatformIDs(num_platforms, platforms.data(), nullptr);
	std::vector<cl_device_id> all_devices;
	for (cl_uint pi = 0; pi < num_platforms; ++pi) {
		cl_uint num_devices = 0; clGetDeviceIDs(platforms[pi], CL_DEVICE_TYPE_GPU, 0, nullptr, &num_devices);
		std::vector<cl_device_id> devices(num_devices); if (num_devices) clGetDeviceIDs(platforms[pi], CL_DEVICE_TYPE_GPU, num_devices, devices.data(), nullptr);
		all_devices.insert(all_devices.end(), devices.begin(), devices.end());
	}
	if (all_devices.empty()) throw std::runtime_error("No OpenCL GPU devices found");
	if (wanted_index >= all_devices.size()) throw std::runtime_error("-gpu index out of range");
	OpenCLContext ctx; ctx.device = all_devices[wanted_index];
	cl_int err = 0; ctx.context = clCreateContext(nullptr, 1, &ctx.device, nullptr, nullptr, &err); if (err != CL_SUCCESS) throw std::runtime_error("clCreateContext failed");
#ifdef CL_VERSION_2_0
	ctx.queue = clCreateCommandQueueWithProperties(ctx.context, ctx.device, nullptr, &err);
#else
	ctx.queue = clCreateCommandQueue(ctx.context, ctx.device, 0, &err);
#endif
	if (err != CL_SUCCESS) { ReleaseOpenCL(ctx); throw std::runtime_error("clCreateCommandQueue failed"); }
	const char* src = kOpenCLKernel; size_t len = std::strlen(kOpenCLKernel);
	ctx.program = clCreateProgramWithSource(ctx.context, 1, &src, &len, &err); if (err != CL_SUCCESS) { ReleaseOpenCL(ctx); throw std::runtime_error("clCreateProgramWithSource failed"); }
	err = clBuildProgram(ctx.program, 1, &ctx.device, "", nullptr, nullptr);
	if (err != CL_SUCCESS) {
		size_t log_size = 0; clGetProgramBuildInfo(ctx.program, ctx.device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &log_size);
		std::string log; log.resize(log_size);
		clGetProgramBuildInfo(ctx.program, ctx.device, CL_PROGRAM_BUILD_LOG, log_size, log.data(), nullptr);
		ReleaseOpenCL(ctx);
		throw std::runtime_error(std::string("OpenCL build error: ") + log);
	}
	ctx.kernel = clCreateKernel(ctx.program, "randomq_kernel", &err); if (err != CL_SUCCESS) { ReleaseOpenCL(ctx); throw std::runtime_error("clCreateKernel failed"); }
	return ctx;
}
#endif

static void MinerLoop()
{
	const std::string payout = gArgs.GetArg("-address", ""); if (payout.empty()) throw std::runtime_error("-address is required");
	const bool list_only = gArgs.GetBoolArg("-list-gpus", false);
	const unsigned gpu_index = (unsigned)gArgs.GetIntArg("-gpu", 0);
	const bool force_cpu = gArgs.GetBoolArg("-cpu-fallback", false);

	if (list_only) { ListOpenCLDevices(); return; }

	std::atomic<uint64_t> total_hashes{0}; std::atomic<uint64_t> window_hashes{0}; uint64_t start_time = (uint64_t)GetTime();
	std::thread reporter([&](){ while (!g_stop.load()) { uint64_t now = (uint64_t)GetTime(); uint64_t elapsed = now - start_time; double avg = elapsed ? (double)total_hashes.load() / (double)elapsed : 0.0; double cur = window_hashes.exchange(0) / 5.0; tfm::format(std::cout, "[HashRate] Current: %.2f H/s | Average: %.2f H/s | Total: %llu\n", cur, avg, (unsigned long long)total_hashes.load()); std::cout.flush(); for (int i = 0; i < 5 && !g_stop.load(); ++i) std::this_thread::sleep_for(1s); } });

	try {
	while (!g_stop.load()) {
		UniValue rules(UniValue::VARR); rules.push_back("segwit");
		UniValue caps(UniValue::VARR); caps.push_back("coinbasetxn");
		UniValue req(UniValue::VOBJ); req.pushKV("rules", rules); req.pushKV("capabilities", caps);
		UniValue params_arr(UniValue::VARR); params_arr.push_back(req);
		tfm::format(std::cout, "[Info] Fetching block template...\n"); std::cout.flush();
		UniValue gbt = RpcCallWaitParams("getblocktemplate", params_arr);
		const UniValue err = gbt.find_value("error"); if (!err.isNull()) throw std::runtime_error(err.write());
		const UniValue res = gbt.find_value("result"); if (res.isNull()) { for (int i = 0; i < 5 && !g_stop.load(); ++i) std::this_thread::sleep_for(1s); continue; }
		CBlock block; std::string tmpl_hex; if (!BuildBlockFromGBT(res, block, tmpl_hex)) { tfm::format(std::cout, "[Warn] Failed to build block from template, retrying...\n"); std::cout.flush(); continue; }
		block.hashMerkleRoot = BlockMerkleRoot(block);

#ifdef OPENCL_FOUND
		if (!force_cpu) {
			OpenCLContext clctx = CreateOpenCL(gpu_index);
			cl_int errc = 0;
			unsigned char header[80];
			std::vector<unsigned char> header_vec;
			{
				VectorWriter vw(header_vec, 0, static_cast<const CBlockHeader&>(block));
			}
			if (header_vec.size() < sizeof(header)) {
				throw std::runtime_error("serialized header too small");
			}
			std::memcpy(header, header_vec.data(), sizeof(header));
			cl_mem d_header = clCreateBuffer(clctx.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(header), header, &errc);
			uint32_t nonce_base = block.nNonce;
			cl_mem d_target = clCreateBuffer(clctx.context, CL_MEM_READ_ONLY, 32, nullptr, &errc);
			cl_mem d_found_flag = clCreateBuffer(clctx.context, CL_MEM_READ_WRITE, sizeof(cl_int), nullptr, &errc);
			cl_mem d_found_nonce = clCreateBuffer(clctx.context, CL_MEM_READ_WRITE, sizeof(cl_uint), nullptr, &errc);
			cl_int zero = 0; clEnqueueWriteBuffer(clctx.queue, d_found_flag, CL_TRUE, 0, sizeof(zero), &zero, 0, nullptr, nullptr);
			clSetKernelArg(clctx.kernel, 0, sizeof(d_header), &d_header);
			clSetKernelArg(clctx.kernel, 1, sizeof(nonce_base), &nonce_base);
			clSetKernelArg(clctx.kernel, 2, sizeof(d_target), &d_target);
			clSetKernelArg(clctx.kernel, 3, sizeof(d_found_flag), &d_found_flag);
			clSetKernelArg(clctx.kernel, 4, sizeof(d_found_nonce), &d_found_nonce);
			size_t global = 262144; // placeholder work size
			cl_int qerr = clEnqueueNDRangeKernel(clctx.queue, clctx.kernel, 1, nullptr, &global, nullptr, 0, nullptr, nullptr);
			if (qerr != CL_SUCCESS) { tfm::format(std::cout, "[Error] clEnqueueNDRangeKernel failed: %d\n", (int)qerr); std::cout.flush(); }
			clFinish(clctx.queue);
			int found = 0; clEnqueueReadBuffer(clctx.queue, d_found_flag, CL_TRUE, 0, sizeof(found), &found, 0, nullptr, nullptr);
			uint32_t found_nonce = 0; if (found) clEnqueueReadBuffer(clctx.queue, d_found_nonce, CL_TRUE, 0, sizeof(found_nonce), &found_nonce, 0, nullptr, nullptr);
			clReleaseMemObject(d_header); clReleaseMemObject(d_target); clReleaseMemObject(d_found_flag); clReleaseMemObject(d_found_nonce);
			ReleaseOpenCL(clctx);
			if (found) { block.nNonce = found_nonce; }
		} else
#endif
		{
			// Fallback: CPU check of a single batch (placeholder)
			const int maxtries = gArgs.GetIntArg("-maxtries", 1000000);
			for (int i = 0; i < maxtries && !g_stop.load(); ++i) {
				const uint256 h = RandomQMining::CalculateRandomQHashOptimized(block, block.nNonce);
				window_hashes.fetch_add(1, std::memory_order_relaxed);
				total_hashes.fetch_add(1, std::memory_order_relaxed);
				arith_uint256 target; bool neg=false, of=false; target.SetCompact(block.nBits, &neg, &of);
				if (!neg && !of && target != 0 && UintToArith256(h) <= target) break; ++block.nNonce;
			}
		}

		std::string sub_hex; if (!tmpl_hex.empty()) sub_hex = UpdateNonceInBlockHex(tmpl_hex, block.nNonce); else sub_hex = BuildFullBlockHex(block);
		UniValue sub = RpcCall("submitblock", {sub_hex}); (void)sub;
	}
	} catch (const std::exception& e) { g_stop.store(true); tfm::format(std::cerr, "gpuminer-opencl error: %s\n", e.what()); }

	reporter.join();
}

int main(int argc, char* argv[])
{
#ifdef WIN32
	common::WinCmdLineArgs winArgs; std::tie(argc, argv) = winArgs.get();
#endif
	SetupEnvironment(); if (!SetupNetworking()) { tfm::format(std::cerr, "Error: networking init failed\n"); return EXIT_FAILURE; }
	try {
		SetupMinerArgs(gArgs);
		std::string error; if (!gArgs.ParseParameters(argc, argv, error)) { if (error != "") tfm::format(std::cerr, "Error parsing command line: %s\n", error); return EXIT_FAILURE; }
		SelectBaseParams(gArgs.GetChainType()); if (auto cfgerr = common::InitConfig(gArgs, nullptr)) { }
		std::signal(SIGINT, [](int){ g_stop.store(true); });
#ifdef SIGTERM
		std::signal(SIGTERM, [](int){ g_stop.store(true); });
#endif
		if (gArgs.GetBoolArg("-list-gpus", false)) { ListOpenCLDevices(); return EXIT_SUCCESS; }
		MinerLoop();
	} catch (const std::exception& e) { tfm::format(std::cerr, "gpuminer-opencl error: %s\n", e.what()); return EXIT_FAILURE; }
	return EXIT_SUCCESS;
}


