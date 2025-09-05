// Copyright (c) 2024-present The Bitquantum Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "randomq_miner.h"
#include "config.h"
#include "rpc_client.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cstring>

// Include RandomQ implementation
#include "randomq_hash.h"
#include "randomq_mining.h"

RandomQMiner::RandomQMiner()
    : m_num_threads(0)
    , m_randomq_rounds(8192)
    , m_enable_avx2(true)
    , m_enable_sse4(true)
    , m_enable_optimized(true)
    , m_submit_work(true)
    , m_running(false)
    , m_should_stop(false)
    , m_has_work(false)
    , m_log_level(2)
    , m_show_stats(true)
    , m_stats_interval(10)
{
    m_stats.reset();
}

RandomQMiner::~RandomQMiner() {
    stop();
}

bool RandomQMiner::initialize(const std::string& config_file) {
    // Load configuration
    MinerConfig config;
    if (!config_file.empty()) {
        if (!ConfigManager::loadFromFile(config_file, config)) {
            return false;
        }
    }
    
    // Validate configuration
    if (!config.validate()) {
        return false;
    }
    
    // Apply configuration
    m_num_threads = config.num_threads;
    m_randomq_rounds = config.randomq_rounds;
    m_enable_avx2 = config.enable_avx2;
    m_enable_sse4 = config.enable_sse4;
    m_enable_optimized = config.enable_optimized;
    m_submit_work = config.submit_work;
    m_log_level = config.log_level;
    m_show_stats = config.show_stats;
    m_stats_interval = config.stats_interval;
    
    log(2, "RandomQMiner initialized with " + std::to_string(m_num_threads) + " threads");
    log(2, "RandomQ rounds: " + std::to_string(m_randomq_rounds));
    log(2, "AVX2: " + std::string(m_enable_avx2 ? "enabled" : "disabled"));
    log(2, "SSE4: " + std::string(m_enable_sse4 ? "enabled" : "disabled"));
    log(2, "Optimized: " + std::string(m_enable_optimized ? "enabled" : "disabled"));
    
    return true;
}

void RandomQMiner::start() {
    if (m_running) {
        log(1, "Miner is already running");
        return;
    }
    
    m_running = true;
    m_should_stop = false;
    m_start_time = std::chrono::steady_clock::now();
    m_last_stats_time = m_start_time;
    
    // Start mining threads
    m_threads.clear();
    for (int i = 0; i < m_num_threads; i++) {
        m_threads.emplace_back(&RandomQMiner::miningThread, this, i);
    }
    
    log(2, "Started " + std::to_string(m_num_threads) + " mining threads");
}

void RandomQMiner::stop() {
    if (!m_running) {
        return;
    }
    
    log(2, "Stopping miner...");
    m_should_stop = true;
    
    // Wait for all threads to finish
    for (auto& thread : m_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    m_threads.clear();
    m_running = false;
    
    log(2, "Miner stopped");
}

MiningStats RandomQMiner::getStats() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    return m_stats;
}

void RandomQMiner::setWork(const WorkData& work) {
    if (!work.isValid()) {
        log(0, "Invalid work data received");
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_work_mutex);
    m_current_work = work;
    m_has_work = true;
    
    log(2, "New work received:");
    work.print();
}

void RandomQMiner::setThreadCount(int count) {
    if (m_running) {
        log(1, "Cannot change thread count while mining");
        return;
    }
    
    m_num_threads = count;
    log(2, "Thread count set to " + std::to_string(count));
}

void RandomQMiner::setRandomQRounds(uint64_t rounds) {
    m_randomq_rounds = rounds;
    log(2, "RandomQ rounds set to " + std::to_string(rounds));
}

void RandomQMiner::setOptimizations(bool avx2, bool sse4, bool optimized) {
    m_enable_avx2 = avx2;
    m_enable_sse4 = sse4;
    m_enable_optimized = optimized;
    
    log(2, "Optimizations updated - AVX2: " + std::string(avx2 ? "enabled" : "disabled") +
           ", SSE4: " + std::string(sse4 ? "enabled" : "disabled") +
           ", Optimized: " + std::string(optimized ? "enabled" : "disabled"));
}

void RandomQMiner::miningThread(int thread_id) {
    log(3, "Mining thread " + std::to_string(thread_id) + " started");
    
    uint32_t local_hashes = 0;
    const uint32_t stats_update_interval = 10000; // Update stats every 10k hashes
    
    while (!m_should_stop) {
        // Get current work
        WorkData work;
        bool has_work = false;
        
        {
            std::lock_guard<std::mutex> lock(m_work_mutex);
            if (m_has_work) {
                work = m_current_work;
                has_work = true;
            }
        }
        
        if (!has_work) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // Mining loop
        uint32_t nonce = work.nonce_start + thread_id;
        uint32_t nonce_increment = m_num_threads;
        
        while (!m_should_stop && nonce <= work.nonce_end) {
            // Calculate hash
            uint256 hash = calculateHash(work, nonce);
            local_hashes++;
            
            // Check if hash meets target
            if (checkWork(work) && hash < uint256(work.target)) {
                log(2, "Found valid block! Nonce: " + std::to_string(nonce));
                log(2, "Hash: " + hash.ToString());
                
                // Update statistics
                {
                    std::lock_guard<std::mutex> lock(m_stats_mutex);
                    m_stats.valid_blocks++;
                    m_stats.best_hash = hash.ToString();
                    m_stats.best_nonce = nonce;
                }
                
                // Submit solution
                if (m_submit_work) {
                    submitSolution(work, nonce, hash);
                }
                
                // Update work (get new block template)
                // This would typically be done by the RPC client
                break;
            }
            
            nonce += nonce_increment;
            
            // Update statistics periodically
            if (local_hashes % stats_update_interval == 0) {
                updateStats(local_hashes);
                local_hashes = 0;
            }
        }
        
        // If we've exhausted the nonce range, wait for new work
        if (nonce > work.nonce_end) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    log(3, "Mining thread " + std::to_string(thread_id) + " stopped");
}

bool RandomQMiner::checkWork(const WorkData& work) {
    // Basic validation
    if (work.block_template.empty() || work.target.empty()) {
        return false;
    }
    
    // Additional validation can be added here
    return true;
}

uint256 RandomQMiner::calculateHash(const WorkData& work, uint32_t nonce) {
    // Create block header
    CBlockHeader header;
    header.nVersion = work.version;
    header.hashPrevBlock = uint256(work.previous_block_hash);
    header.hashMerkleRoot = uint256(work.merkle_root);
    header.nTime = work.timestamp;
    header.nBits = work.bits;
    header.nNonce = nonce;
    
    // Calculate RandomQ hash
    return RandomQMining::CalculateRandomQHashOptimized(header, nonce);
}

void RandomQMiner::submitSolution(const WorkData& work, uint32_t nonce, const uint256& hash) {
    // This would typically submit the solution via RPC
    // For now, just log the solution
    log(2, "Submitting solution:");
    log(2, "  Nonce: " + std::to_string(nonce));
    log(2, "  Hash: " + hash.ToString());
    log(2, "  Target: " + work.target);
}

void RandomQMiner::updateStats(uint32_t hashes_processed) {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    
    m_stats.total_hashes += hashes_processed;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_start_time);
    m_stats.elapsed_time = elapsed.count();
    
    if (m_stats.elapsed_time > 0) {
        m_stats.hash_rate = static_cast<double>(m_stats.total_hashes) / m_stats.elapsed_time;
    }
    
    // Print statistics if enabled
    if (m_show_stats) {
        auto time_since_last_stats = std::chrono::duration_cast<std::chrono::seconds>(now - m_last_stats_time);
        if (time_since_last_stats.count() >= m_stats_interval) {
            m_stats.print();
            m_last_stats_time = now;
        }
    }
}

void RandomQMiner::log(int level, const std::string& message) const {
    if (level <= m_log_level) {
        const char* level_names[] = {"ERROR", "WARNING", "INFO", "DEBUG"};
        std::cout << "[" << level_names[level] << "] " << message << std::endl;
    }
}

// WorkData implementation
bool WorkData::isValid() const {
    return !block_template.empty() && 
           !previous_block_hash.empty() && 
           !target.empty() && 
           version != 0 && 
           timestamp != 0 && 
           bits != 0 && 
           height != 0;
}

void WorkData::print() const {
    std::cout << "Work Data:" << std::endl;
    std::cout << "  Previous Block: " << previous_block_hash << std::endl;
    std::cout << "  Target: " << target << std::endl;
    std::cout << "  Version: " << version << std::endl;
    std::cout << "  Timestamp: " << timestamp << std::endl;
    std::cout << "  Bits: " << bits << std::endl;
    std::cout << "  Height: " << height << std::endl;
    std::cout << "  Nonce Range: " << nonce_start << " - " << nonce_end << std::endl;
    std::cout << "  Transactions: " << transactions.size() << std::endl;
}

// MiningStats implementation
void MiningStats::reset() {
    total_hashes = 0;
    valid_blocks = 0;
    invalid_blocks = 0;
    hash_rate = 0.0;
    elapsed_time = 0.0;
    current_nonce = 0;
    current_target.clear();
    best_hash.clear();
    best_nonce = 0;
}

void MiningStats::print() const {
    std::cout << "\n=== Mining Statistics ===" << std::endl;
    std::cout << "Total Hashes: " << total_hashes << std::endl;
    std::cout << "Valid Blocks: " << valid_blocks << std::endl;
    std::cout << "Invalid Blocks: " << invalid_blocks << std::endl;
    std::cout << "Hash Rate: " << std::fixed << std::setprecision(2) << hash_rate << " H/s" << std::endl;
    std::cout << "Elapsed Time: " << elapsed_time << " seconds" << std::endl;
    if (!best_hash.empty()) {
        std::cout << "Best Hash: " << best_hash << std::endl;
        std::cout << "Best Nonce: " << best_nonce << std::endl;
    }
    std::cout << "=========================" << std::endl;
}
