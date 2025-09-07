// Copyright (c) 2024-present The Bitquantum Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "config.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>

bool MinerConfig::validate() const {
    if (rpc_host.empty()) {
        std::cerr << "Error: RPC host cannot be empty" << std::endl;
        return false;
    }
    
    if (rpc_user.empty()) {
        std::cerr << "Error: RPC username cannot be empty" << std::endl;
        return false;
    }
    
    if (rpc_password.empty()) {
        std::cerr << "Error: RPC password cannot be empty" << std::endl;
        return false;
    }
    
    if (num_threads < 0) {
        std::cerr << "Error: Number of threads cannot be negative" << std::endl;
        return false;
    }
    
    if (nonce_start > nonce_end) {
        std::cerr << "Error: nonce_start cannot be greater than nonce_end" << std::endl;
        return false;
    }
    
    if (randomq_rounds == 0) {
        std::cerr << "Error: RandomQ rounds must be greater than 0" << std::endl;
        return false;
    }
    
    return true;
}

void MinerConfig::print() const {
    std::cout << "=== CPUMiner Configuration ===" << std::endl;
    std::cout << "RPC Host: " << rpc_host << ":" << rpc_port << std::endl;
    std::cout << "RPC User: " << rpc_user << std::endl;
    std::cout << "Threads: " << (num_threads == 0 ? "auto" : std::to_string(num_threads)) << std::endl;
    std::cout << "Nonce Range: " << nonce_start << " - " << nonce_end << std::endl;
    std::cout << "RandomQ Rounds: " << randomq_rounds << std::endl;
    std::cout << "AVX2: " << (enable_avx2 ? "enabled" : "disabled") << std::endl;
    std::cout << "SSE4: " << (enable_sse4 ? "enabled" : "disabled") << std::endl;
    std::cout << "Optimized: " << (enable_optimized ? "enabled" : "disabled") << std::endl;
    std::cout << "Long Polling: " << (long_polling ? "enabled" : "disabled") << std::endl;
    std::cout << "Submit Work: " << (submit_work ? "enabled" : "disabled") << std::endl;
    std::cout << "=============================" << std::endl;
}

bool ConfigManager::loadFromFile(const std::string& filename, MinerConfig& config) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open config file: " << filename << std::endl;
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Remove comments and whitespace
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if (line.empty()) continue;
        
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;
        
        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);
        
        // Trim key and value
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        // Parse configuration values
        if (key == "rpc_host") {
            config.rpc_host = value;
        } else if (key == "rpc_port") {
            config.rpc_port = static_cast<uint16_t>(std::stoul(value));
        } else if (key == "rpc_user") {
            config.rpc_user = value;
        } else if (key == "rpc_password") {
            config.rpc_password = value;
        } else if (key == "num_threads") {
            config.num_threads = std::stoi(value);
        } else if (key == "nonce_start") {
            config.nonce_start = std::stoul(value);
        } else if (key == "nonce_end") {
            config.nonce_end = std::stoul(value);
        } else if (key == "randomq_rounds") {
            config.randomq_rounds = std::stoull(value);
        } else if (key == "enable_avx2") {
            config.enable_avx2 = (value == "true" || value == "1");
        } else if (key == "enable_sse4") {
            config.enable_sse4 = (value == "true" || value == "1");
        } else if (key == "enable_optimized") {
            config.enable_optimized = (value == "true" || value == "1");
        } else if (key == "log_level") {
            config.log_level = std::stoi(value);
        } else if (key == "show_stats") {
            config.show_stats = (value == "true" || value == "1");
        } else if (key == "stats_interval") {
            config.stats_interval = std::stoul(value);
        } else if (key == "long_polling") {
            config.long_polling = (value == "true" || value == "1");
        } else if (key == "work_timeout") {
            config.work_timeout = std::stoul(value);
        } else if (key == "retry_interval") {
            config.retry_interval = std::stoul(value);
        } else if (key == "submit_work") {
            config.submit_work = (value == "true" || value == "1");
        }
    }
    
    return true;
}

bool ConfigManager::loadFromArgs(int argc, char* argv[], MinerConfig& config) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            printHelp();
            return false;
        } else if (arg == "--version" || arg == "-v") {
            printVersion();
            return false;
        } else if (arg == "--rpc-host" && i + 1 < argc) {
            config.rpc_host = argv[++i];
        } else if (arg == "--rpc-port" && i + 1 < argc) {
            config.rpc_port = static_cast<uint16_t>(std::stoul(argv[++i]));
        } else if (arg == "--rpc-user" && i + 1 < argc) {
            config.rpc_user = argv[++i];
        } else if (arg == "--rpc-password" && i + 1 < argc) {
            config.rpc_password = argv[++i];
        } else if (arg == "--threads" && i + 1 < argc) {
            try {
                config.num_threads = std::stoi(argv[++i]);
                if (config.num_threads < 0) {
                    std::cerr << "Error: Thread count cannot be negative" << std::endl;
                    return false;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error: Invalid thread count: " << argv[i] << std::endl;
                return false;
            }
        } else if (arg == "--nonce-start" && i + 1 < argc) {
            try {
                config.nonce_start = std::stoul(argv[++i]);
            } catch (const std::exception& e) {
                std::cerr << "Error: Invalid nonce start value: " << argv[i] << std::endl;
                return false;
            }
        } else if (arg == "--nonce-end" && i + 1 < argc) {
            try {
                config.nonce_end = std::stoul(argv[++i]);
            } catch (const std::exception& e) {
                std::cerr << "Error: Invalid nonce end value: " << argv[i] << std::endl;
                return false;
            }
        } else if (arg == "--randomq-rounds" && i + 1 < argc) {
            try {
                config.randomq_rounds = std::stoull(argv[++i]);
                if (config.randomq_rounds == 0) {
                    std::cerr << "Error: RandomQ rounds must be greater than 0" << std::endl;
                    return false;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error: Invalid RandomQ rounds value: " << argv[i] << std::endl;
                return false;
            }
        } else if (arg == "--no-avx2") {
            config.enable_avx2 = false;
        } else if (arg == "--no-sse4") {
            config.enable_sse4 = false;
        } else if (arg == "--no-optimized") {
            config.enable_optimized = false;
        } else if (arg == "--log-level" && i + 1 < argc) {
            config.log_level = std::stoi(argv[++i]);
        } else if (arg == "--no-stats") {
            config.show_stats = false;
        } else if (arg == "--stats-interval" && i + 1 < argc) {
            config.stats_interval = std::stoul(argv[++i]);
        } else if (arg == "--no-long-polling") {
            config.long_polling = false;
        } else if (arg == "--work-timeout" && i + 1 < argc) {
            config.work_timeout = std::stoul(argv[++i]);
        } else if (arg == "--retry-interval" && i + 1 < argc) {
            config.retry_interval = std::stoul(argv[++i]);
        } else if (arg == "--no-submit") {
            config.submit_work = false;
        } else if (arg == "--config" && i + 1 < argc) {
            std::string config_file = argv[++i];
            if (!loadFromFile(config_file, config)) {
                return false;
            }
        }
    }
    
    // Auto-detect number of threads if not specified
    if (config.num_threads == 0) {
        config.num_threads = std::thread::hardware_concurrency();
        if (config.num_threads == 0) {
            config.num_threads = 1;
        }
    }
    
    return true;
}

void ConfigManager::printHelp() {
    std::cout << "CPUMiner - Bitquantum RandomQ CPU Miner" << std::endl;
    std::cout << "Usage: cpuminer [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --help              Show this help message" << std::endl;
    std::cout << "  -v, --version           Show version information" << std::endl;
    std::cout << "  --config FILE           Load configuration from file" << std::endl;
    std::cout << std::endl;
    std::cout << "RPC Connection:" << std::endl;
    std::cout << "  --rpc-host HOST         RPC server host (default: 127.0.0.1)" << std::endl;
    std::cout << "  --rpc-port PORT         RPC server port (default: 18332)" << std::endl;
    std::cout << "  --rpc-user USER         RPC username" << std::endl;
    std::cout << "  --rpc-password PASS     RPC password" << std::endl;
    std::cout << std::endl;
    std::cout << "Mining:" << std::endl;
    std::cout << "  --threads N             Number of mining threads (0=auto)" << std::endl;
    std::cout << "  --nonce-start N         Starting nonce value" << std::endl;
    std::cout << "  --nonce-end N           Ending nonce value" << std::endl;
    std::cout << "  --randomq-rounds N      RandomQ algorithm rounds (default: 8192)" << std::endl;
    std::cout << std::endl;
    std::cout << "Performance:" << std::endl;
    std::cout << "  --no-avx2               Disable AVX2 optimizations" << std::endl;
    std::cout << "  --no-sse4               Disable SSE4 optimizations" << std::endl;
    std::cout << "  --no-optimized          Disable optimized algorithms" << std::endl;
    std::cout << std::endl;
    std::cout << "Logging:" << std::endl;
    std::cout << "  --log-level N           Log level (0=error, 1=warning, 2=info, 3=debug)" << std::endl;
    std::cout << "  --no-stats              Disable statistics display" << std::endl;
    std::cout << "  --stats-interval N      Statistics update interval in seconds" << std::endl;
    std::cout << std::endl;
    std::cout << "Advanced:" << std::endl;
    std::cout << "  --no-long-polling       Disable long polling" << std::endl;
    std::cout << "  --work-timeout N        Work timeout in seconds (default: 300)" << std::endl;
    std::cout << "  --retry-interval N      Retry interval in seconds (default: 5)" << std::endl;
    std::cout << "  --no-submit             Don't submit found blocks" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  cpuminer --rpc-user user --rpc-password pass --threads 4" << std::endl;
}

void ConfigManager::printVersion() {
    std::cout << "CPUMiner v1.0.0" << std::endl;
    std::cout << "Bitquantum RandomQ CPU Miner" << std::endl;
    std::cout << "Copyright (c) 2024-present The Bitquantum Core developers" << std::endl;
}
