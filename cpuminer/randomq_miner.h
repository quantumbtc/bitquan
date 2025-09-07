// Copyright (c) 2024-present The Bitquantum Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CPUMINER_RANDOMQ_MINER_H
#define CPUMINER_RANDOMQ_MINER_H

#include <cstdint>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include "../src/uint256.h"

// Forward declarations
struct WorkData;

class RandomQMiner {
public:
    RandomQMiner();
    ~RandomQMiner();
    
    // Initialize the miner
    bool initialize(const std::string& config_file = "");
    
    // Start mining
    void start();
    
    // Stop mining
    void stop();
    
    // Check if mining is running
    bool isRunning() const { return m_running; }
    
    // Get current statistics
    MiningStats getStats() const;
    
    // Set new work
    void setWork(const WorkData& work);
    
    // Set number of threads
    void setThreadCount(int count);
    
    // Set RandomQ rounds
    void setRandomQRounds(uint64_t rounds);
    
    // Enable/disable optimizations
    void setOptimizations(bool avx2, bool sse4, bool optimized);

private:
    // Mining thread function
    void miningThread(int thread_id);
    
    // Check if work is valid
    bool checkWork(const WorkData& work);
    
    // Calculate RandomQ hash
    uint256 calculateHash(const WorkData& work, uint32_t nonce);
    
    // Submit found solution
    void submitSolution(const WorkData& work, uint32_t nonce, const uint256& hash);
    
    // Update statistics
    void updateStats(uint32_t hashes_processed);
    
    // Logging
    void log(int level, const std::string& message) const;
    
private:
    // Configuration
    int m_num_threads;
    uint64_t m_randomq_rounds;
    bool m_enable_avx2;
    bool m_enable_sse4;
    bool m_enable_optimized;
    bool m_submit_work;
    
    // Threading
    std::vector<std::thread> m_threads;
    std::atomic<bool> m_running;
    std::atomic<bool> m_should_stop;
    
    // Work data
    std::mutex m_work_mutex;
    WorkData m_current_work;
    bool m_has_work;
    
    // Statistics
    mutable std::mutex m_stats_mutex;
    MiningStats m_stats;
    std::chrono::steady_clock::time_point m_start_time;
    std::chrono::steady_clock::time_point m_last_stats_time;
    
    // Logging
    int m_log_level;
    bool m_show_stats;
    uint32_t m_stats_interval;
};

// WorkData is defined in rpc_client.h

// MiningStats is defined in rpc_client.h

#endif // CPUMINER_RANDOMQ_MINER_H
