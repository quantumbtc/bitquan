// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitquantum Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <kernel/chainparams.h>

#include <chainparamsseeds.h>
#include <consensus/amount.h>
#include <consensus/merkle.h>
#include <consensus/params.h>
#include <hash.h>
#include <crypto/randomq_mining.h>
#include <kernel/messagestartchars.h>
#include <logging.h>
#include <pow.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <uint256.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <vector>
#include <iomanip>
#include <iostream>
#include <util/chaintype.h>
#include <util/strencodings.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <iostream>
#include <iomanip>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

using namespace util::hex_literals;

// Workaround MSVC bug triggering C7595 when calling consteval constructors in
// initializer lists.
// https://developercommunity.visualstudio.com/t/Bogus-C7595-error-on-valid-C20-code/10906093
#if defined(_MSC_VER)
auto consteval_ctor(auto&& input) { return input; }
#else
#define consteval_ctor(input) (input)
#endif

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.version = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
 *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: 4a5e1e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "Entangle value, not control";
    const CScript genesisOutputScript = CScript() << "04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f"_hex << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

/**
 * Main network on which people trade goods and services.
 */
class CMainParams : public CChainParams {
public:
    CMainParams() {
        m_chain_type = ChainType::MAIN;
        
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 210000;
        
        consensus.BIP34Height = 0;
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 0; // 000000000000000004c2b624ed5d7756c508d90fd0da2c7c679febfa6c4735f0
        consensus.BIP66Height = 0; // 00000000000000000379eaa19dce8c9b722d46ae6a57c2f1a988119488b50931
        consensus.CSVHeight = 0; // 000000000000000004a1b34462cb8aeebd5799177f7a29cf28f2d1961716b5b5
        consensus.SegwitHeight = 0; // 0000000000000000001c8018d9cb3b742ef25114f27563e3fc4a1902167f9893
        consensus.MinBIP9WarningHeight = 0; // segwit activation height + miner confirmation window
        consensus.powLimit = uint256{"00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.nPowTargetTimespan = 3 * 24 * 60 * 60; // three days
        consensus.nPowTargetSpacing = 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.enforce_BIP94 = false;
        consensus.fPowNoRetargeting = false;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 1815; // 90%
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period = 2016;

        // Deployment of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = 1619222400; // April 24th, 2021
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = 1628640000; // August 11th, 2021
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 709632; // Approximately November 12th, 2021
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].threshold = 1815; // 90%
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].period = 2016;


        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
         pchMessageStart[0] = 0xfa;
         pchMessageStart[1] = 0xbf;
         pchMessageStart[2] = 0xc1;
         pchMessageStart[3] = 0xdb;
         // => 0xfabfc1db
         
        nDefaultPort = 51997;
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 720;
        m_assumed_chain_state_size = 14;

        vSeeds.clear();
        vFixedSeeds.clear();

        // Create genesis with nonce 0, then find a valid nonce using RandomQ
        genesis = CreateGenesisBlock(1756857263, 1379716, 0x1e0ffff0, 1, 50 * COIN);

        consensus.hashGenesisBlock = genesis.GetHash();
        
        // Verify genesis block meets proof-of-work requirements
        if (!CheckProofOfWork(genesis, genesis.nBits, consensus)) {
            std::cout << "ERROR: Genesis block does not meet proof-of-work requirements!" << std::endl;
            std::cout << "Genesis block hash: " << consensus.hashGenesisBlock.GetHex() << std::endl;
            std::cout << "Genesis merkle root: " << genesis.hashMerkleRoot.GetHex() << std::endl;
            std::cout << "Genesis nBits: 0x" << std::hex << genesis.nBits << std::dec << std::endl;
            std::cout << "Genesis nNonce: " << genesis.nNonce << std::endl;
            std::cout << "Genesis nTime: " << genesis.nTime << std::endl;
            std::cout << "PowLimit: " << consensus.powLimit.GetHex() << std::endl;
            
            // Calculate target from nBits
            arith_uint256 target;
            bool fNegative, fOverflow;
            target.SetCompact(genesis.nBits, &fNegative, &fOverflow);
            std::cout << "Target: " << target.GetHex() << std::endl;
            std::cout << "Hash <= Target: " << ((UintToArith256(consensus.hashGenesisBlock) <= target) ? "true" : "false") << std::endl;
            
            // Try to find a valid nonce for the genesis block using multi-threading
            std::cout << "Attempting to find valid nonce for genesis block using multi-threading..." << std::endl;
            
            // Multi-threaded nonce search
            const int thread_count = std::min(4, (int)std::thread::hardware_concurrency());
            const uint64_t max_tries = 1000000; // Limit search to prevent infinite loop
            const uint64_t nonces_per_thread = max_tries / thread_count;
            
            std::atomic<bool> found{false};
            std::atomic<uint32_t> found_nonce{0};
            std::atomic<uint256> found_hash{};
            std::mutex result_mutex;
            
            std::cout << "Using " << thread_count << " threads for nonce search, max_tries=" << max_tries << std::endl;
            
            auto worker = [&](uint32_t start_nonce, uint64_t thread_max_tries) {
                CBlockHeader genesisHeader = genesis;
                uint32_t nonce = start_nonce;
                
                for (uint64_t i = 0; i < thread_max_tries && !found; ++i) {
                    genesisHeader.nNonce = nonce;
                    uint256 hash = genesisHeader.GetHash();
                    
                    if (UintToArith256(hash) <= target) {
                        std::lock_guard<std::mutex> lock(result_mutex);
                        if (!found.exchange(true)) {
                            found_nonce = nonce;
                            found_hash = hash;
                            std::cout << "Found valid nonce: " << nonce << " (thread " << std::this_thread::get_id() << ")" << std::endl;
                            std::cout << "New genesis block hash: " << hash.GetHex() << std::endl;
                        }
                        break;
                    }
                    
                    nonce++;
                    if (nonce == 0) break; // Prevent overflow
                }
            };
            
            // Start worker threads
            std::vector<std::thread> workers;
            for (int i = 0; i < thread_count; ++i) {
                uint32_t start_nonce = i * nonces_per_thread;
                uint64_t thread_tries = (i == thread_count - 1) ? 
                    (max_tries - i * nonces_per_thread) : nonces_per_thread;
                
                workers.emplace_back(worker, start_nonce, thread_tries);
            }
            
            // Wait for all threads to complete
            for (auto& worker : workers) {
                worker.join();
            }
            
            if (found) {
                // Update genesis block with valid nonce
                genesis.nNonce = found_nonce;
                consensus.hashGenesisBlock = found_hash;
                std::cout << "Genesis block updated with valid nonce " << found_nonce.load() << std::endl;
                
                // Verify the updated genesis block
                if (!CheckProofOfWork(genesis, genesis.nBits, consensus)) {
                    std::cout << "WARNING: Updated genesis block still does not meet proof-of-work requirements" << std::endl;
                    std::cout << "WARNING: Continuing with invalid genesis block for debugging purposes" << std::endl;
                } else {
                    std::cout << "Genesis block proof-of-work verification passed after nonce search" << std::endl;
                }
            } else {
                std::cout << "WARNING: Could not find valid nonce for genesis block" << std::endl;
                std::cout << "WARNING: Continuing with invalid genesis block for debugging purposes" << std::endl;
            }
        } else {
            std::cout << "Genesis block proof-of-work verification passed" << std::endl;
            std::cout << "Genesis block hash: " << consensus.hashGenesisBlock.GetHex() << std::endl;
            std::cout << "Genesis merkle root: " << genesis.hashMerkleRoot.GetHex() << std::endl;
        }
        
        // TODO: Recalculate genesis block hash after RandomQ algorithm changes
        // assert(consensus.hashGenesisBlock == uint256{"00000c62fac2d483d65c37331a3a73c6f315de2541e7384e94e36d3b1491604f"});
        // assert(genesis.hashMerkleRoot == uint256{"b0e14069031ce67080e53fe3d2cdbc23d0949fd85efac43e67ffdcf07d66d541"});

        // Note that of those which support the service bits prefix, most only support a subset of
        // possible options.
        // This is fine at runtime as we'll fall back to using them as an addrfetch if they don't support the
        // service bits we want, but we should get them updated to support all service bits wanted by any
        // release ASAP to avoid it where possible.
        //vSeeds.emplace_back("seed.bitquantum.sipa.be."); // Pieter Wuille, only supports x1, x5, x9, and xd

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,0x1B);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,0x55);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,0x9B);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        bech32_hrp = "btq";

        {
            const size_t seed_len = sizeof(chainparams_seed_main) / sizeof(chainparams_seed_main[0]);
            vFixedSeeds.assign(chainparams_seed_main, chainparams_seed_main + seed_len);
        }

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        m_assumeutxo_data = {
        };

        chainTxData = ChainTxData{
            // Data from RPC: getchaintxstats 4096 00000000000000000001b658dd1120e82e66d2790811f89ede9742ada3ed6d77
            .nTime    = 0,
            .tx_count = 0,
            .dTxRate  = 4.620728156243148,
        };
    }
};

/**
 * Testnet (v3): public test network which is reset from time to time.
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        m_chain_type = ChainType::TESTNET;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 210000;
       
        consensus.BIP34Height = 0;
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 0; // 00000000007f6655f22f98e72ed80d8b06dc761d5da09df0fa1dc4be4f861eb6
        consensus.BIP66Height = 0; // 000000002104c8c45e99a8853285a3b592602a3ccde2b832481da85e9e4ba182
        consensus.CSVHeight = 0; // 00000000025e930139bac5c6c31a403776da130831ab85be56578f3fa75369bb
        consensus.SegwitHeight = 0; // 00000000002b980fcd729daaa248fd9316a5200e9b367f4ff2c42453e84201ca
        consensus.MinBIP9WarningHeight = 0; // segwit activation height + miner confirmation window
        consensus.powLimit = uint256{"00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.enforce_BIP94 = false;
        consensus.fPowNoRetargeting = false;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 1512; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period = 2016;

        // Deployment of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = 1619222400; // April 24th, 2021
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = 1628640000; // August 11th, 2021
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].threshold = 1512; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].period = 2016;


        pchMessageStart[0] = 0xfb;
pchMessageStart[1] = 0xce;
pchMessageStart[2] = 0xb2;
pchMessageStart[3] = 0xda;
// => 0xfbceb2da

        nDefaultPort = 51998;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 200;
        m_assumed_chain_state_size = 19;

        genesis = CreateGenesisBlock(1756857263, 1379716, 0x1e0ffff0, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        
        // Verify genesis block meets proof-of-work requirements
        if (!CheckProofOfWork(genesis, genesis.nBits, consensus)) {
            std::cout << "ERROR: Genesis block does not meet proof-of-work requirements!" << std::endl;
            std::cout << "Genesis block hash: " << consensus.hashGenesisBlock.GetHex() << std::endl;
            std::cout << "Genesis merkle root: " << genesis.hashMerkleRoot.GetHex() << std::endl;
            std::cout << "Genesis nBits: 0x" << std::hex << genesis.nBits << std::dec << std::endl;
            std::cout << "Genesis nNonce: " << genesis.nNonce << std::endl;
            std::cout << "Genesis nTime: " << genesis.nTime << std::endl;
            std::cout << "PowLimit: " << consensus.powLimit.GetHex() << std::endl;
            
            // Calculate target from nBits
            arith_uint256 target;
            bool fNegative, fOverflow;
            target.SetCompact(genesis.nBits, &fNegative, &fOverflow);
            std::cout << "Target: " << target.GetHex() << std::endl;
            std::cout << "Hash <= Target: " << ((UintToArith256(consensus.hashGenesisBlock) <= target) ? "true" : "false") << std::endl;
            
            // Try to find a valid nonce for the genesis block using multi-threading
            std::cout << "Attempting to find valid nonce for genesis block using multi-threading..." << std::endl;
            
            // Multi-threaded nonce search
            const int thread_count = std::min(4, (int)std::thread::hardware_concurrency());
            const uint64_t max_tries = 1000000; // Limit search to prevent infinite loop
            const uint64_t nonces_per_thread = max_tries / thread_count;
            
            std::atomic<bool> found{false};
            std::atomic<uint32_t> found_nonce{0};
            std::atomic<uint256> found_hash{};
            std::mutex result_mutex;
            
            std::cout << "Using " << thread_count << " threads for nonce search, max_tries=" << max_tries << std::endl;
            
            auto worker = [&](uint32_t start_nonce, uint64_t thread_max_tries) {
                CBlockHeader genesisHeader = genesis;
                uint32_t nonce = start_nonce;
                
                for (uint64_t i = 0; i < thread_max_tries && !found; ++i) {
                    genesisHeader.nNonce = nonce;
                    uint256 hash = genesisHeader.GetHash();
                    
                    if (UintToArith256(hash) <= target) {
                        std::lock_guard<std::mutex> lock(result_mutex);
                        if (!found.exchange(true)) {
                            found_nonce = nonce;
                            found_hash = hash;
                            std::cout << "Found valid nonce: " << nonce << " (thread " << std::this_thread::get_id() << ")" << std::endl;
                            std::cout << "New genesis block hash: " << hash.GetHex() << std::endl;
                        }
                        break;
                    }
                    
                    nonce++;
                    if (nonce == 0) break; // Prevent overflow
                }
            };
            
            // Start worker threads
            std::vector<std::thread> workers;
            for (int i = 0; i < thread_count; ++i) {
                uint32_t start_nonce = i * nonces_per_thread;
                uint64_t thread_tries = (i == thread_count - 1) ? 
                    (max_tries - i * nonces_per_thread) : nonces_per_thread;
                
                workers.emplace_back(worker, start_nonce, thread_tries);
            }
            
            // Wait for all threads to complete
            for (auto& worker : workers) {
                worker.join();
            }
            
            if (found) {
                // Update genesis block with valid nonce
                genesis.nNonce = found_nonce;
                consensus.hashGenesisBlock = found_hash;
                std::cout << "Genesis block updated with valid nonce " << found_nonce.load() << std::endl;
                
                // Verify the updated genesis block
                if (!CheckProofOfWork(genesis, genesis.nBits, consensus)) {
                    std::cout << "WARNING: Updated genesis block still does not meet proof-of-work requirements" << std::endl;
                    std::cout << "WARNING: Continuing with invalid genesis block for debugging purposes" << std::endl;
                } else {
                    std::cout << "Genesis block proof-of-work verification passed after nonce search" << std::endl;
                }
            } else {
                std::cout << "WARNING: Could not find valid nonce for genesis block" << std::endl;
                std::cout << "WARNING: Continuing with invalid genesis block for debugging purposes" << std::endl;
            }
        } else {
            std::cout << "Genesis block proof-of-work verification passed" << std::endl;
            std::cout << "Genesis block hash: " << consensus.hashGenesisBlock.GetHex() << std::endl;
            std::cout << "Genesis merkle root: " << genesis.hashMerkleRoot.GetHex() << std::endl;
        }
        
        // TODO: Recalculate genesis block hash after RandomQ algorithm changes
        // assert(consensus.hashGenesisBlock == uint256{"00000c62fac2d483d65c37331a3a73c6f315de2541e7384e94e36d3b1491604f"});
        // assert(genesis.hashMerkleRoot == uint256{"b0e14069031ce67080e53fe3d2cdbc23d0949fd85efac43e67ffdcf07d66d541"});

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "tb";

        {
            const size_t seed_len = sizeof(chainparams_seed_test) / sizeof(chainparams_seed_test[0]);
            vFixedSeeds.assign(chainparams_seed_test, chainparams_seed_test + seed_len);
        }

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        m_assumeutxo_data = {
        };

        chainTxData = ChainTxData{
            // Data from RPC: getchaintxstats 4096 00000000000003fc7967410ba2d0a8a8d50daedc318d43e8baf1a9782c236a57
            .nTime    = 0,
            .tx_count = 0,
            .dTxRate  = 17.15933950357594,
        };
    }
};

/**
 * Testnet (v4): public test network which is reset from time to time.
 */
class CTestNet4Params : public CChainParams {
public:
    CTestNet4Params() {
        m_chain_type = ChainType::TESTNET4;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 1;
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256{"00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.enforce_BIP94 = true;
        consensus.fPowNoRetargeting = false;

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 1512; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period = 2016;

        // Deployment of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].threshold = 1512; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].period = 2016;

    
        pchMessageStart[0] = 0xfc;
pchMessageStart[1] = 0xab;
pchMessageStart[2] = 0xd3;
pchMessageStart[3] = 0xca;
// => 0xfcabd3ca

        nDefaultPort = 51999;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 11;
        m_assumed_chain_state_size = 1;

        genesis = CreateGenesisBlock(1756857263, 1379716, 0x1e0ffff0, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        
        // Verify genesis block meets proof-of-work requirements
        if (!CheckProofOfWork(genesis, genesis.nBits, consensus)) {
            std::cout << "ERROR: Genesis block does not meet proof-of-work requirements!" << std::endl;
            std::cout << "Genesis block hash: " << consensus.hashGenesisBlock.GetHex() << std::endl;
            std::cout << "Genesis merkle root: " << genesis.hashMerkleRoot.GetHex() << std::endl;
            std::cout << "Genesis nBits: 0x" << std::hex << genesis.nBits << std::dec << std::endl;
            std::cout << "Genesis nNonce: " << genesis.nNonce << std::endl;
            std::cout << "Genesis nTime: " << genesis.nTime << std::endl;
            std::cout << "PowLimit: " << consensus.powLimit.GetHex() << std::endl;
            
            // Calculate target from nBits
            arith_uint256 target;
            bool fNegative, fOverflow;
            target.SetCompact(genesis.nBits, &fNegative, &fOverflow);
            std::cout << "Target: " << target.GetHex() << std::endl;
            std::cout << "Hash <= Target: " << ((UintToArith256(consensus.hashGenesisBlock) <= target) ? "true" : "false") << std::endl;
            
            // Try to find a valid nonce for the genesis block using multi-threading
            std::cout << "Attempting to find valid nonce for genesis block using multi-threading..." << std::endl;
            
            // Multi-threaded nonce search
            const int thread_count = std::min(4, (int)std::thread::hardware_concurrency());
            const uint64_t max_tries = 1000000; // Limit search to prevent infinite loop
            const uint64_t nonces_per_thread = max_tries / thread_count;
            
            std::atomic<bool> found{false};
            std::atomic<uint32_t> found_nonce{0};
            std::atomic<uint256> found_hash{};
            std::mutex result_mutex;
            
            std::cout << "Using " << thread_count << " threads for nonce search, max_tries=" << max_tries << std::endl;
            
            auto worker = [&](uint32_t start_nonce, uint64_t thread_max_tries) {
                CBlockHeader genesisHeader = genesis;
                uint32_t nonce = start_nonce;
                
                for (uint64_t i = 0; i < thread_max_tries && !found; ++i) {
                    genesisHeader.nNonce = nonce;
                    uint256 hash = genesisHeader.GetHash();
                    
                    if (UintToArith256(hash) <= target) {
                        std::lock_guard<std::mutex> lock(result_mutex);
                        if (!found.exchange(true)) {
                            found_nonce = nonce;
                            found_hash = hash;
                            std::cout << "Found valid nonce: " << nonce << " (thread " << std::this_thread::get_id() << ")" << std::endl;
                            std::cout << "New genesis block hash: " << hash.GetHex() << std::endl;
                        }
                        break;
                    }
                    
                    nonce++;
                    if (nonce == 0) break; // Prevent overflow
                }
            };
            
            // Start worker threads
            std::vector<std::thread> workers;
            for (int i = 0; i < thread_count; ++i) {
                uint32_t start_nonce = i * nonces_per_thread;
                uint64_t thread_tries = (i == thread_count - 1) ? 
                    (max_tries - i * nonces_per_thread) : nonces_per_thread;
                
                workers.emplace_back(worker, start_nonce, thread_tries);
            }
            
            // Wait for all threads to complete
            for (auto& worker : workers) {
                worker.join();
            }
            
            if (found) {
                // Update genesis block with valid nonce
                genesis.nNonce = found_nonce;
                consensus.hashGenesisBlock = found_hash;
                std::cout << "Genesis block updated with valid nonce " << found_nonce.load() << std::endl;
                
                // Verify the updated genesis block
                if (!CheckProofOfWork(genesis, genesis.nBits, consensus)) {
                    std::cout << "WARNING: Updated genesis block still does not meet proof-of-work requirements" << std::endl;
                    std::cout << "WARNING: Continuing with invalid genesis block for debugging purposes" << std::endl;
                } else {
                    std::cout << "Genesis block proof-of-work verification passed after nonce search" << std::endl;
                }
            } else {
                std::cout << "WARNING: Could not find valid nonce for genesis block" << std::endl;
                std::cout << "WARNING: Continuing with invalid genesis block for debugging purposes" << std::endl;
            }
        } else {
            std::cout << "Genesis block proof-of-work verification passed" << std::endl;
            std::cout << "Genesis block hash: " << consensus.hashGenesisBlock.GetHex() << std::endl;
            std::cout << "Genesis merkle root: " << genesis.hashMerkleRoot.GetHex() << std::endl;
        }
        
        // TODO: Recalculate genesis block hash after RandomQ algorithm changes
        // assert(consensus.hashGenesisBlock == uint256{"00000c62fac2d483d65c37331a3a73c6f315de2541e7384e94e36d3b1491604f"});
        // assert(genesis.hashMerkleRoot == uint256{"b0e14069031ce67080e53fe3d2cdbc23d0949fd85efac43e67ffdcf07d66d541"});

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
      
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "tb";

        {
            const size_t seed_len = sizeof(chainparams_seed_testnet4) / sizeof(chainparams_seed_testnet4[0]);
            vFixedSeeds.assign(chainparams_seed_testnet4, chainparams_seed_testnet4 + seed_len);
        }

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        m_assumeutxo_data = {
            {}
        };

        chainTxData = ChainTxData{
            // Data from RPC: getchaintxstats 4096 0000000000003ed4f08dbdf6f7d6b271a6bcffce25675cb40aa9fa43179a89f3
            .nTime    = 1741070246,
            .tx_count = 7653966,
            .dTxRate  = 1.239174414591965,
        };
    }
};

/**
 * Signet: test network with an additional consensus parameter (see BIP325).
 */
class SigNetParams : public CChainParams {
public:
    explicit SigNetParams(const SigNetOptions& options)
    {
        std::vector<uint8_t> bin;
        vFixedSeeds.clear();
        vSeeds.clear();

        if (!options.challenge) {
            bin = "512103ad5e0edad18cb1f0fc0d28a3d4f1f3e445640337489abb10404f2d1e086be430210359ef5021964fe22d6f8e05b2463c9540ce96883fe3b278760f048f5189f2e6c452ae"_hex_v_u8;
            {
                const size_t seed_len = sizeof(chainparams_seed_signet) / sizeof(chainparams_seed_signet[0]);
                vFixedSeeds.assign(chainparams_seed_signet, chainparams_seed_signet + seed_len);
            }
            vSeeds.emplace_back("seed.signet.bitquantum.sprovoost.nl.");
            vSeeds.emplace_back("seed.signet.achownodes.xyz."); // Ava Chow, only supports x1, x5, x9, x49, x809, x849, xd, x400, x404, x408, x448, xc08, xc48, x40c

            consensus.nMinimumChainWork = uint256{"000000000000000000000000000000000000000000000000000002b517f3d1a1"};
            consensus.defaultAssumeValid = uint256{"000000895a110f46e59eb82bbc5bfb67fa314656009c295509c21b4999f5180a"}; // 237722
            m_assumed_blockchain_size = 9;
            m_assumed_chain_state_size = 1;
            chainTxData = ChainTxData{
                // Data from RPC: getchaintxstats 4096 000000895a110f46e59eb82bbc5bfb67fa314656009c295509c21b4999f5180a
                .nTime    = 1741019645,
                .tx_count = 16540736,
                .dTxRate  = 1.064918879911595,
            };
        } else {
            bin = *options.challenge;
            consensus.nMinimumChainWork = uint256{};
            consensus.defaultAssumeValid = uint256{};
            m_assumed_blockchain_size = 0;
            m_assumed_chain_state_size = 0;
            chainTxData = ChainTxData{
                0,
                0,
                0,
            };
            LogInfo("Signet with challenge %s", HexStr(bin));
        }

        if (options.seeds) {
            vSeeds = *options.seeds;
        }

        m_chain_type = ChainType::SIGNET;
        consensus.signet_blocks = true;
        consensus.signet_challenge.assign(bin.begin(), bin.end());
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 1;
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.enforce_BIP94 = false;
        consensus.fPowNoRetargeting = false;
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256{"00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 1815; // 90%
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period = 2016;

        // Activation of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].threshold = 1815; // 90%
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].period = 2016;

        // message start is defined as the first 4 bytes of the sha256d of the block script
        HashWriter h{};
        h << consensus.signet_challenge;
        uint256 hash = h.GetHash();
        std::copy_n(hash.begin(), 4, pchMessageStart.begin());

        nDefaultPort = 52000;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1756857263, 1379716, 0x1e0ffff0, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        
        // Verify genesis block meets proof-of-work requirements
        if (!CheckProofOfWork(genesis, genesis.nBits, consensus)) {
            std::cout << "ERROR: Genesis block does not meet proof-of-work requirements!" << std::endl;
            std::cout << "Genesis block hash: " << consensus.hashGenesisBlock.GetHex() << std::endl;
            std::cout << "Genesis merkle root: " << genesis.hashMerkleRoot.GetHex() << std::endl;
            std::cout << "Genesis nBits: 0x" << std::hex << genesis.nBits << std::dec << std::endl;
            std::cout << "Genesis nNonce: " << genesis.nNonce << std::endl;
            std::cout << "Genesis nTime: " << genesis.nTime << std::endl;
            std::cout << "PowLimit: " << consensus.powLimit.GetHex() << std::endl;
            
            // Calculate target from nBits
            arith_uint256 target;
            bool fNegative, fOverflow;
            target.SetCompact(genesis.nBits, &fNegative, &fOverflow);
            std::cout << "Target: " << target.GetHex() << std::endl;
            std::cout << "Hash <= Target: " << ((UintToArith256(consensus.hashGenesisBlock) <= target) ? "true" : "false") << std::endl;
            
            // Try to find a valid nonce for the genesis block using multi-threading
            std::cout << "Attempting to find valid nonce for genesis block using multi-threading..." << std::endl;
            
            // Multi-threaded nonce search
            const int thread_count = std::min(4, (int)std::thread::hardware_concurrency());
            const uint64_t max_tries = 1000000; // Limit search to prevent infinite loop
            const uint64_t nonces_per_thread = max_tries / thread_count;
            
            std::atomic<bool> found{false};
            std::atomic<uint32_t> found_nonce{0};
            std::atomic<uint256> found_hash{};
            std::mutex result_mutex;
            
            std::cout << "Using " << thread_count << " threads for nonce search, max_tries=" << max_tries << std::endl;
            
            auto worker = [&](uint32_t start_nonce, uint64_t thread_max_tries) {
                CBlockHeader genesisHeader = genesis;
                uint32_t nonce = start_nonce;
                
                for (uint64_t i = 0; i < thread_max_tries && !found; ++i) {
                    genesisHeader.nNonce = nonce;
                    uint256 hash = genesisHeader.GetHash();
                    
                    if (UintToArith256(hash) <= target) {
                        std::lock_guard<std::mutex> lock(result_mutex);
                        if (!found.exchange(true)) {
                            found_nonce = nonce;
                            found_hash = hash;
                            std::cout << "Found valid nonce: " << nonce << " (thread " << std::this_thread::get_id() << ")" << std::endl;
                            std::cout << "New genesis block hash: " << hash.GetHex() << std::endl;
                        }
                        break;
                    }
                    
                    nonce++;
                    if (nonce == 0) break; // Prevent overflow
                }
            };
            
            // Start worker threads
            std::vector<std::thread> workers;
            for (int i = 0; i < thread_count; ++i) {
                uint32_t start_nonce = i * nonces_per_thread;
                uint64_t thread_tries = (i == thread_count - 1) ? 
                    (max_tries - i * nonces_per_thread) : nonces_per_thread;
                
                workers.emplace_back(worker, start_nonce, thread_tries);
            }
            
            // Wait for all threads to complete
            for (auto& worker : workers) {
                worker.join();
            }
            
            if (found) {
                // Update genesis block with valid nonce
                genesis.nNonce = found_nonce;
                consensus.hashGenesisBlock = found_hash;
                std::cout << "Genesis block updated with valid nonce " << found_nonce.load() << std::endl;
                
                // Verify the updated genesis block
                if (!CheckProofOfWork(genesis, genesis.nBits, consensus)) {
                    std::cout << "WARNING: Updated genesis block still does not meet proof-of-work requirements" << std::endl;
                    std::cout << "WARNING: Continuing with invalid genesis block for debugging purposes" << std::endl;
                } else {
                    std::cout << "Genesis block proof-of-work verification passed after nonce search" << std::endl;
                }
            } else {
                std::cout << "WARNING: Could not find valid nonce for genesis block" << std::endl;
                std::cout << "WARNING: Continuing with invalid genesis block for debugging purposes" << std::endl;
            }
        } else {
            std::cout << "Genesis block proof-of-work verification passed" << std::endl;
            std::cout << "Genesis block hash: " << consensus.hashGenesisBlock.GetHex() << std::endl;
            std::cout << "Genesis merkle root: " << genesis.hashMerkleRoot.GetHex() << std::endl;
        }
        
        // TODO: Recalculate genesis block hash after RandomQ algorithm changes
        // assert(consensus.hashGenesisBlock == uint256{"00000c62fac2d483d65c37331a3a73c6f315de2541e7384e94e36d3b1491604f"});
        // assert(genesis.hashMerkleRoot == uint256{"b0e14069031ce67080e53fe3d2cdbc23d0949fd85efac43e67ffdcf07d66d541"});

        m_assumeutxo_data = {
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "tb";

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;
    }
};

/**
 * Regression test: intended for private networks only. Has minimal difficulty to ensure that
 * blocks can be found instantly.
 */
class CRegTestParams : public CChainParams
{
public:
    explicit CRegTestParams(const RegTestOptions& opts)
    {
        m_chain_type = ChainType::REGTEST;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 150;
        consensus.BIP34Height = 1; // Always active unless overridden
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1;  // Always active unless overridden
        consensus.BIP66Height = 1;  // Always active unless overridden
        consensus.CSVHeight = 1;    // Always active unless overridden
        consensus.SegwitHeight = 0; // Always active unless overridden
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256{"7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.nPowTargetTimespan = 24 * 60 * 60; // one day
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.enforce_BIP94 = opts.enforce_bip94;
        consensus.fPowNoRetargeting = true;

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 108; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period = 144; // Faster than normal for regtest (144 instead of 2016)

        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].threshold = 108; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].period = 144;

        consensus.nMinimumChainWork = uint256{};
        consensus.defaultAssumeValid = uint256{};

        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;
        nDefaultPort = 52001;
        nPruneAfterHeight = opts.fastprune ? 100 : 1000;
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        for (const auto& [dep, height] : opts.activation_heights) {
            switch (dep) {
            case Consensus::BuriedDeployment::DEPLOYMENT_SEGWIT:
                consensus.SegwitHeight = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_HEIGHTINCB:
                consensus.BIP34Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_DERSIG:
                consensus.BIP66Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_CLTV:
                consensus.BIP65Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_CSV:
                consensus.CSVHeight = int{height};
                break;
            }
        }

        for (const auto& [deployment_pos, version_bits_params] : opts.version_bits_parameters) {
            consensus.vDeployments[deployment_pos].nStartTime = version_bits_params.start_time;
            consensus.vDeployments[deployment_pos].nTimeout = version_bits_params.timeout;
            consensus.vDeployments[deployment_pos].min_activation_height = version_bits_params.min_activation_height;
        }

        genesis = CreateGenesisBlock(1756857263, 1379716, 0x1e0ffff0, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        
        // Verify genesis block meets proof-of-work requirements
        if (!CheckProofOfWork(genesis, genesis.nBits, consensus)) {
            std::cout << "ERROR: Genesis block does not meet proof-of-work requirements!" << std::endl;
            std::cout << "Genesis block hash: " << consensus.hashGenesisBlock.GetHex() << std::endl;
            std::cout << "Genesis merkle root: " << genesis.hashMerkleRoot.GetHex() << std::endl;
            std::cout << "Genesis nBits: 0x" << std::hex << genesis.nBits << std::dec << std::endl;
            std::cout << "Genesis nNonce: " << genesis.nNonce << std::endl;
            std::cout << "Genesis nTime: " << genesis.nTime << std::endl;
            std::cout << "PowLimit: " << consensus.powLimit.GetHex() << std::endl;
            
            // Calculate target from nBits
            arith_uint256 target;
            bool fNegative, fOverflow;
            target.SetCompact(genesis.nBits, &fNegative, &fOverflow);
            std::cout << "Target: " << target.GetHex() << std::endl;
            std::cout << "Hash <= Target: " << ((UintToArith256(consensus.hashGenesisBlock) <= target) ? "true" : "false") << std::endl;
            
            // Try to find a valid nonce for the genesis block using multi-threading
            std::cout << "Attempting to find valid nonce for genesis block using multi-threading..." << std::endl;
            
            // Multi-threaded nonce search
            const int thread_count = std::min(4, (int)std::thread::hardware_concurrency());
            const uint64_t max_tries = 1000000; // Limit search to prevent infinite loop
            const uint64_t nonces_per_thread = max_tries / thread_count;
            
            std::atomic<bool> found{false};
            std::atomic<uint32_t> found_nonce{0};
            std::atomic<uint256> found_hash{};
            std::mutex result_mutex;
            
            std::cout << "Using " << thread_count << " threads for nonce search, max_tries=" << max_tries << std::endl;
            
            auto worker = [&](uint32_t start_nonce, uint64_t thread_max_tries) {
                CBlockHeader genesisHeader = genesis;
                uint32_t nonce = start_nonce;
                
                for (uint64_t i = 0; i < thread_max_tries && !found; ++i) {
                    genesisHeader.nNonce = nonce;
                    uint256 hash = genesisHeader.GetHash();
                    
                    if (UintToArith256(hash) <= target) {
                        std::lock_guard<std::mutex> lock(result_mutex);
                        if (!found.exchange(true)) {
                            found_nonce = nonce;
                            found_hash = hash;
                            std::cout << "Found valid nonce: " << nonce << " (thread " << std::this_thread::get_id() << ")" << std::endl;
                            std::cout << "New genesis block hash: " << hash.GetHex() << std::endl;
                        }
                        break;
                    }
                    
                    nonce++;
                    if (nonce == 0) break; // Prevent overflow
                }
            };
            
            // Start worker threads
            std::vector<std::thread> workers;
            for (int i = 0; i < thread_count; ++i) {
                uint32_t start_nonce = i * nonces_per_thread;
                uint64_t thread_tries = (i == thread_count - 1) ? 
                    (max_tries - i * nonces_per_thread) : nonces_per_thread;
                
                workers.emplace_back(worker, start_nonce, thread_tries);
            }
            
            // Wait for all threads to complete
            for (auto& worker : workers) {
                worker.join();
            }
            
            if (found) {
                // Update genesis block with valid nonce
                genesis.nNonce = found_nonce;
                consensus.hashGenesisBlock = found_hash;
                std::cout << "Genesis block updated with valid nonce " << found_nonce.load() << std::endl;
                
                // Verify the updated genesis block
                if (!CheckProofOfWork(genesis, genesis.nBits, consensus)) {
                    std::cout << "WARNING: Updated genesis block still does not meet proof-of-work requirements" << std::endl;
                    std::cout << "WARNING: Continuing with invalid genesis block for debugging purposes" << std::endl;
                } else {
                    std::cout << "Genesis block proof-of-work verification passed after nonce search" << std::endl;
                }
            } else {
                std::cout << "WARNING: Could not find valid nonce for genesis block" << std::endl;
                std::cout << "WARNING: Continuing with invalid genesis block for debugging purposes" << std::endl;
            }
        } else {
            std::cout << "Genesis block proof-of-work verification passed" << std::endl;
            std::cout << "Genesis block hash: " << consensus.hashGenesisBlock.GetHex() << std::endl;
            std::cout << "Genesis merkle root: " << genesis.hashMerkleRoot.GetHex() << std::endl;
        }
        
        // TODO: Recalculate genesis block hash after RandomQ algorithm changes
        // assert(consensus.hashGenesisBlock == uint256{"00000c62fac2d483d65c37331a3a73c6f315de2541e7384e94e36d3b1491604f"});
        // assert(genesis.hashMerkleRoot == uint256{"b0e14069031ce67080e53fe3d2cdbc23d0949fd85efac43e67ffdcf07d66d541"});

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();
        vSeeds.emplace_back("dummySeed.invalid.");

        fDefaultConsistencyChecks = true;
        m_is_mockable_chain = true;

        m_assumeutxo_data = {
        };

        chainTxData = ChainTxData{
            .nTime = 0,
            .tx_count = 0,
            .dTxRate = 0.001, // Set a non-zero rate to make it testable
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "bcrt";
    }
};

std::unique_ptr<const CChainParams> CChainParams::SigNet(const SigNetOptions& options)
{
    return std::make_unique<const SigNetParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::RegTest(const RegTestOptions& options)
{
    return std::make_unique<const CRegTestParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::Main()
{
    return std::make_unique<const CMainParams>();
}

std::unique_ptr<const CChainParams> CChainParams::TestNet()
{
    return std::make_unique<const CTestNetParams>();
}

std::unique_ptr<const CChainParams> CChainParams::TestNet4()
{
    return std::make_unique<const CTestNet4Params>();
}

std::vector<int> CChainParams::GetAvailableSnapshotHeights() const
{
    std::vector<int> heights;
    heights.reserve(m_assumeutxo_data.size());

    for (const auto& data : m_assumeutxo_data) {
        heights.emplace_back(data.height);
    }
    return heights;
}

std::optional<ChainType> GetNetworkForMagic(const MessageStartChars& message)
{
    const auto mainnet_msg = CChainParams::Main()->MessageStart();
    const auto testnet_msg = CChainParams::TestNet()->MessageStart();
    const auto testnet4_msg = CChainParams::TestNet4()->MessageStart();
    const auto regtest_msg = CChainParams::RegTest({})->MessageStart();
    const auto signet_msg = CChainParams::SigNet({})->MessageStart();

    if (std::ranges::equal(message, mainnet_msg)) {
        return ChainType::MAIN;
    } else if (std::ranges::equal(message, testnet_msg)) {
        return ChainType::TESTNET;
    } else if (std::ranges::equal(message, testnet4_msg)) {
        return ChainType::TESTNET4;
    } else if (std::ranges::equal(message, regtest_msg)) {
        return ChainType::REGTEST;
    } else if (std::ranges::equal(message, signet_msg)) {
        return ChainType::SIGNET;
    }
    return std::nullopt;
}
