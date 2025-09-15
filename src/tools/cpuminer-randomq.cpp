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

static UniValue RpcCall(const std::string& method, const std::vector<std::string>& params)
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

	// Obtain event base and connection
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

	UniValue params_arr(UniValue::VARR);
	for (const auto& p : params) params_arr.push_back(p);
	UniValue id(1);
	UniValue body = JSONRPCRequestObj(method, params_arr, id);
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
	
	// Coinbase
	CAmount cb_value = 0;
	if (!gbt_res["coinbasevalue"].isNull()) {
		cb_value = gbt_res["coinbasevalue"].getInt<int64_t>();
	}
	int32_t height = 0;
	if (!gbt_res["height"].isNull()) height = gbt_res["height"].getInt<int>();
	CMutableTransaction coinbase;
	coinbase.version = 1;
	// scriptSig: BIP34 height + coinbaseaux flags if provided
	CScript sig;
	sig << CScriptNum(height);
	const UniValue aux = gbt_res.find_value("coinbaseaux");
	if (aux.isObject()) {
		const UniValue flags = aux.find_value("flags");
		if (flags.isStr()) {
			std::vector<unsigned char> flag_bytes = ParseHex(flags.get_str());
			sig << flag_bytes;
		}
	}
	coinbase.vin.emplace_back(CTxIn(COutPoint(), sig));
	// payout
	{
		const std::string addr_str = gArgs.GetArg("-address", "");
		CTxDestination dest = DecodeDestination(addr_str);
		if (!IsValidDestination(dest)) throw std::runtime_error("invalid mining address for coinbase");
		CScript payout = GetScriptForDestination(dest);
		coinbase.vout.emplace_back(CTxOut(cb_value, payout));
	}
	// Witness handling
	bool add_commitment = false;
	std::vector<unsigned char> default_commitment;
	{
		const UniValue commit = gbt_res.find_value("default_witness_commitment");
		if (!commit.isNull() && commit.isStr()) {
			default_commitment = ParseHex(commit.get_str());
			add_commitment = (default_commitment.size() == 32);
		}
	}
	if (add_commitment) {
		coinbase.vin[0].scriptWitness.stack.resize(1);
		coinbase.vin[0].scriptWitness.stack[0].assign(32, 0);
	}
	block.vtx.clear();
	block.vtx.push_back(MakeTransactionRef(std::move(coinbase)));
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
	// Add witness commitment output if required
	if (add_commitment) {
		// OP_RETURN 0xaa21a9ed <32-byte commitment>
		CScript opret;
		opret << OP_RETURN;
		std::vector<unsigned char> marker{0xaa,0x21,0xa9,0xed};
		std::vector<unsigned char> payload;
		payload.insert(payload.end(), marker.begin(), marker.end());
		payload.insert(payload.end(), default_commitment.begin(), default_commitment.end());
		opret << payload;
		CMutableTransaction cb2;
		cb2.version = block.vtx[0]->version;
		cb2.vin = block.vtx[0]->vin;
		cb2.vout = block.vtx[0]->vout;
		cb2.nLockTime = block.vtx[0]->nLockTime;
		cb2.vout.emplace_back(CTxOut(0, opret));
		// preserve witness reserved value
		cb2.vin[0].scriptWitness = block.vtx[0]->vin[0].scriptWitness;
		block.vtx[0] = MakeTransactionRef(std::move(cb2));
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

static void MinerLoop()
{
	const std::string payout = gArgs.GetArg("-address", "");
	if (payout.empty()) {
		throw std::runtime_error("-address is required");
	}

	const int maxtries = gArgs.GetIntArg("-maxtries", 1000000);
	const unsigned int hw_threads = std::thread::hardware_concurrency();
	const unsigned int threads = gArgs.GetIntArg("-threads", hw_threads ? hw_threads : 1);

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
		// getblocktemplate (no params; use node defaults)
		UniValue gbt = RpcCallWait("getblocktemplate", {});
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

		std::atomic<bool> found{false};
		std::mutex found_mu;
		uint32_t start_nonce = block.nNonce;

		auto worker = [&](uint32_t thread_id) {
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
			// Submit by patching nonce bytes in template hex
			const std::string sub_hex = UpdateNonceInBlockHex(tmpl_hex, block.nNonce);
			UniValue sub = RpcCallWait("submitblock", {sub_hex});
			( void )sub;
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
