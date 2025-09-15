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
#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#endif

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

using namespace std::chrono_literals;

// Provide translation function symbol expected by common/init.cpp link
const TranslateFn G_TRANSLATION_FUN{nullptr};

// CPU affinity binding
static void SetThreadAffinity(int thread_id, int max_cores)
{
	if (max_cores <= 0) return;
	int core = thread_id % max_cores;
#ifdef WIN32
	HANDLE hThread = GetCurrentThread();
	DWORD_PTR mask = 1ULL << core;
	SetThreadAffinityMask(hThread, mask);
#else
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(core, &cpuset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
}

static const char DEFAULT_RPCCONNECT[] = "127.0.0.1";
static const int DEFAULT_HTTP_CLIENT_TIMEOUT = 900;

static std::atomic<bool> g_stop{false};

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
	argsman.AddArg("-threads=<n>", "Mining threads (default: number of cores)", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
	argsman.AddArg("-cpucore=<n>", "Bind mining to first n CPU cores", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
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

static UniValue RpcCallWait(const std::string& method, const std::vector<std::string>& params)
{
	const bool fWait = gArgs.GetBoolArg("-rpcwait", false);
	const int timeout = gArgs.GetIntArg("-rpcwaittimeout", 0);
	auto deadline = std::chrono::steady_clock::now() + 1s * timeout;
	while (true) {
		try {
			return RpcCall(method, params);
		} catch (...) {
			if (!fWait) throw;
			if (timeout > 0 && std::chrono::steady_clock::now() >= deadline) {
				throw std::runtime_error("timeout waiting for RPC server");
			}
			UninterruptibleSleep(1s);
		}
	}
}

static UniValue RpcCallWaitParams(const std::string& method, const UniValue& params_arr)
{
	const bool fWait = gArgs.GetBoolArg("-rpcwait", false);
	const int timeout = gArgs.GetIntArg("-rpcwaittimeout", 0);
	auto deadline = std::chrono::steady_clock::now() + 1s * timeout;
	while (true) {
		try {
			return RpcCallParams(method, params_arr);
		} catch (...) {
			if (!fWait) throw;
			if (timeout > 0 && std::chrono::steady_clock::now() >= deadline) {
				throw std::runtime_error("timeout waiting for RPC server");
			}
			UninterruptibleSleep(1s);
		}
	}
}

} // namespace

static bool BuildBlockFromGBT(const UniValue& gbt_res, CBlock& block, std::string& tmpl_hex_out)
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
	
	// Coinbase: prefer server-provided coinbasetxn if available
	block.vtx.clear();
	bool built_local_coinbase = false;
	const UniValue coinbasetxn = gbt_res.find_value("coinbasetxn");
	if (coinbasetxn.isObject() && coinbasetxn.exists("data") && coinbasetxn.find_value("data").isStr()) {
		CMutableTransaction mtx;
		if (!DecodeHexTx(mtx, coinbasetxn.find_value("data").get_str())) {
			throw std::runtime_error("failed to decode coinbase txn from template");
		}
		block.vtx.push_back(MakeTransactionRef(std::move(mtx)));
	} else {
		// Build coinbase locally (ONLY BIP34 height; no coinbaseaux flags)
		CAmount cb_value = 0;
		if (!gbt_res["coinbasevalue"].isNull()) {
			cb_value = gbt_res["coinbasevalue"].getInt<int64_t>();
		}
		if (gbt_res["height"].isNull()) throw std::runtime_error("GBT missing height for local coinbase");
		int32_t height = gbt_res["height"].getInt<int>();
		CMutableTransaction coinbase;
		coinbase.version = 1;
		// scriptSig: BIP34 height (following bitquantum-cli implementation)
		CScript sig;
		sig << CScriptNum(height);
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
	const unsigned int hw_threads = std::thread::hardware_concurrency();
	const int max_cores = gArgs.GetIntArg("-cpucore", 0);
	unsigned int threads = gArgs.GetIntArg("-threads", hw_threads ? hw_threads : 1);
	// If -cpucore specified, limit threads to that number
	if (max_cores > 0 && threads > (unsigned int)max_cores) {
		threads = max_cores;
		tfm::format(std::cout, "[Info] Limited threads to %u (cpucore=%d)\n", threads, max_cores);
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
			throw std::runtime_error(err.write());
		}
		const UniValue res = gbt.find_value("result");
		if (res.isNull()) throw std::runtime_error("GBT returned null");

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
		std::mutex found_mu;
		uint32_t start_nonce = block.nNonce;

		auto worker = [&](uint32_t thread_id) {
			// Set CPU affinity if -cpucore specified
			const int max_cores = gArgs.GetIntArg("-cpucore", 0);
			if (max_cores > 0) {
				SetThreadAffinity(thread_id, max_cores);
			}
			CBlock local = block;
			uint32_t nonce = start_nonce + thread_id;
			for (int64_t i = 0; i < maxtries && !g_stop.load() && !found.load(); ++i) {
				local.nNonce = nonce;
				const uint256 h = RandomQMining::CalculateRandomQHashOptimized(local, local.nNonce);
				window_hashes.fetch_add(1, std::memory_order_relaxed);
				total_hashes.fetch_add(1, std::memory_order_relaxed);
				arith_uint256 target; bool neg=false, of=false; target.SetCompact(local.nBits, &neg, &of);
				if (!neg && !of && target != 0 && UintToArith256(h) <= target) {
					std::lock_guard<std::mutex> l(found_mu);
					if (!found.exchange(true)) {
						block.nNonce = local.nNonce;
					}
					break;
				}
				nonce += threads; // stride by thread count
			}
		};

		std::vector<std::thread> miners;
		miners.reserve(threads);
		for (unsigned int t = 0; t < threads; ++t) miners.emplace_back(worker, t);
		for (auto& th : miners) th.join();

		if (found.load()) {
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
}

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
		SelectBaseParams(gArgs.GetChainType());
		if (auto cfgerr = common::InitConfig(gArgs, nullptr)) {
			// ignore config failure for standalone mode
		}
		std::signal(SIGINT, [](int){ g_stop.store(true); });
#ifdef SIGTERM
		std::signal(SIGTERM, [](int){ g_stop.store(true); });
#endif
		MinerLoop();
	} catch (const std::exception& e) {
		tfm::format(std::cerr, "cpuminer-randomq error: %s\n", e.what());
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
