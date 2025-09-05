// Copyright (c) 2024-present The Bitquantum Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CPUMINER_MINER_H
#define CPUMINER_MINER_H

#include "randomq_miner.h"
#include "rpc_client.h"
#include "config.h"
#include <memory>
#include <atomic>
#include <thread>

class Miner {
public:
    Miner();
    ~Miner();
    
    // Initialize miner with configuration
    bool initialize(int argc, char* argv[]);
    
    // Start mining
    void start();
    
    // Stop mining
    void stop();
    
    // Check if running
    bool isRunning() const { return m_running; }
    
    // Get statistics
    void printStats() const;
    
    // Signal handlers
    static void signalHandler(int signal);
    
private:
    // Initialize components
    bool initializeComponents();
    
    // Main mining loop
    void miningLoop();
    
    // Statistics thread
    void statsThread();
    
    // Cleanup
    void cleanup();
    
    // Logging
    void log(int level, const std::string& message) const;

private:
    // Configuration
    MinerConfig m_config;
    
    // Components
    std::unique_ptr<RandomQMiner> m_randomq_miner;
    std::unique_ptr<RPCClient> m_rpc_client;
    
    // State
    std::atomic<bool> m_running;
    std::atomic<bool> m_should_stop;
    
    // Threading
    std::thread m_main_thread;
    std::thread m_stats_thread;
    
    // Statistics
    mutable std::mutex m_stats_mutex;
    std::chrono::steady_clock::time_point m_start_time;
    uint64_t m_total_blocks_found;
    uint64_t m_total_blocks_submitted;
    uint64_t m_total_blocks_accepted;
    
    // Logging
    int m_log_level;
    bool m_show_stats;
    uint32_t m_stats_interval;
    
    // Signal handling
    static std::atomic<bool> s_should_stop;
};

#endif // CPUMINER_MINER_H
