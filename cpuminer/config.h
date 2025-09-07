// Copyright (c) 2024-present The Bitquantum Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CPUMINER_CONFIG_H
#define CPUMINER_CONFIG_H

#include <string>
#include <vector>
#include <cstdint>

struct MinerConfig {
    // RPC connection settings
    std::string rpc_host = "127.0.0.1";
    uint16_t rpc_port = 18332;
    std::string rpc_user;
    std::string rpc_password;
    
    // Mining settings
    int num_threads = 0; // 0 = auto-detect
    uint32_t nonce_start = 0;
    uint32_t nonce_end = UINT32_MAX;
    uint64_t randomq_rounds = 8192;
    
    // Performance settings
    bool enable_avx2 = true;
    bool enable_sse4 = true;
    bool enable_optimized = true;
    
    // Logging settings
    int log_level = 2; // 0=error, 1=warning, 2=info, 3=debug
    bool show_stats = true;
    uint32_t stats_interval = 10; // seconds
    
    // Advanced settings
    bool long_polling = true;
    uint32_t work_timeout = 300; // seconds
    uint32_t retry_interval = 5; // seconds
    bool submit_work = true;
    
    // Validation
    bool validate() const;
    void print() const;
};

class ConfigManager {
public:
    static bool loadFromFile(const std::string& filename, MinerConfig& config);
    static bool loadFromArgs(int argc, char* argv[], MinerConfig& config);
    static void printHelp();
    static void printVersion();
};

#endif // CPUMINER_CONFIG_H
