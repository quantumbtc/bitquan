// Copyright (c) 2024-present The Bitquantum Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"
#include <iostream>
#include <csignal>
#include <chrono>
#include <iomanip>
#include <cstring>

// Static member initialization
std::atomic<bool> Miner::s_should_stop(false);

Miner::Miner()
    : m_running(false)
    , m_should_stop(false)
    , m_total_blocks_found(0)
    , m_total_blocks_submitted(0)
    , m_total_blocks_accepted(0)
    , m_log_level(2)
    , m_show_stats(true)
    , m_stats_interval(10)
{
    // Set up signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
}

Miner::~Miner() {
    stop();
}

bool Miner::initialize(int argc, char* argv[]) {
    // Load configuration from command line arguments
    if (!ConfigManager::loadFromArgs(argc, argv, m_config)) {
        return false;
    }
    
    // Validate configuration
    if (!m_config.validate()) {
        return false;
    }
    
    // Print configuration
    m_config.print();
    
    // Initialize components
    if (!initializeComponents()) {
        return false;
    }
    
    log(2, "Miner initialized successfully");
    return true;
}

bool Miner::initializeComponents() {
    // Create RandomQ miner
    m_randomq_miner = std::make_unique<RandomQMiner>();
    if (!m_randomq_miner->initialize()) {
        log(0, "Failed to initialize RandomQ miner");
        return false;
    }
    
    // Configure RandomQ miner
    m_randomq_miner->setThreadCount(m_config.num_threads);
    m_randomq_miner->setRandomQRounds(m_config.randomq_rounds);
    m_randomq_miner->setOptimizations(m_config.enable_avx2, m_config.enable_sse4, m_config.enable_optimized);
    
    // Create RPC client
    m_rpc_client = std::make_unique<RPCClient>();
    if (!m_rpc_client->initialize(m_config.rpc_host, m_config.rpc_port, 
                                 m_config.rpc_user, m_config.rpc_password)) {
        log(0, "Failed to initialize RPC client");
        return false;
    }
    
    // Set miner reference in RPC client
    m_rpc_client->setMiner(m_randomq_miner.get());
    
    // Apply logging configuration
    m_log_level = m_config.log_level;
    m_show_stats = m_config.show_stats;
    m_stats_interval = m_config.stats_interval;
    
    return true;
}

void Miner::start() {
    if (m_running) {
        log(1, "Miner is already running");
        return;
    }
    
    m_running = true;
    m_should_stop = false;
    m_start_time = std::chrono::steady_clock::now();
    
    // Start RPC client
    m_rpc_client->start();
    
    // Start RandomQ miner
    m_randomq_miner->start();
    
    // Start main mining loop
    m_main_thread = std::thread(&Miner::miningLoop, this);
    
    // Start statistics thread
    if (m_show_stats) {
        m_stats_thread = std::thread(&Miner::statsThread, this);
    }
    
    log(2, "Miner started");
}

void Miner::stop() {
    if (!m_running) {
        return;
    }
    
    log(2, "Stopping miner...");
    m_should_stop = true;
    s_should_stop = true;
    
    // Stop RandomQ miner
    if (m_randomq_miner) {
        m_randomq_miner->stop();
    }
    
    // Stop RPC client
    if (m_rpc_client) {
        m_rpc_client->stop();
    }
    
    // Wait for threads to finish
    if (m_main_thread.joinable()) {
        m_main_thread.join();
    }
    
    if (m_stats_thread.joinable()) {
        m_stats_thread.join();
    }
    
    m_running = false;
    
    log(2, "Miner stopped");
}

void Miner::printStats() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_start_time);
    
    std::cout << "\n=== Miner Statistics ===" << std::endl;
    std::cout << "Uptime: " << elapsed.count() << " seconds" << std::endl;
    std::cout << "Blocks Found: " << m_total_blocks_found << std::endl;
    std::cout << "Blocks Submitted: " << m_total_blocks_submitted << std::endl;
    std::cout << "Blocks Accepted: " << m_total_blocks_accepted << std::endl;
    
    if (m_total_blocks_submitted > 0) {
        double acceptance_rate = (double)m_total_blocks_accepted / m_total_blocks_submitted * 100.0;
        std::cout << "Acceptance Rate: " << std::fixed << std::setprecision(2) 
                  << acceptance_rate << "%" << std::endl;
    }
    
    // Print RandomQ miner stats
    if (m_randomq_miner) {
        MiningStats randomq_stats = m_randomq_miner->getStats();
        std::cout << "\nRandomQ Miner Stats:" << std::endl;
        std::cout << "Total Hashes: " << randomq_stats.total_hashes << std::endl;
        std::cout << "Hash Rate: " << std::fixed << std::setprecision(2) 
                  << randomq_stats.hash_rate << " H/s" << std::endl;
        std::cout << "Valid Blocks: " << randomq_stats.valid_blocks << std::endl;
    }
    
    std::cout << "=======================" << std::endl;
}

void Miner::signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", stopping miner..." << std::endl;
    s_should_stop = true;
}

void Miner::miningLoop() {
    log(2, "Main mining loop started");
    
    while (!m_should_stop && !s_should_stop) {
        try {
            // Check if we have work
            if (m_rpc_client && m_rpc_client->isConnected()) {
                WorkData work = m_rpc_client->getCurrentWork();
                if (work.isValid()) {
                    // Work is already set in the RandomQ miner by the RPC client
                    // Just wait and monitor
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                } else {
                    log(1, "No valid work available, waiting...");
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                }
            } else {
                log(1, "RPC client not connected, waiting...");
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
            
        } catch (const std::exception& e) {
            log(0, "Mining loop error: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    log(2, "Main mining loop stopped");
}

void Miner::statsThread() {
    log(2, "Statistics thread started");
    
    while (!m_should_stop && !s_should_stop) {
        try {
            // Print statistics
            printStats();
            
            // Wait for next update
            std::this_thread::sleep_for(std::chrono::seconds(m_stats_interval));
            
        } catch (const std::exception& e) {
            log(0, "Statistics thread error: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    log(2, "Statistics thread stopped");
}

void Miner::cleanup() {
    // Cleanup resources
    if (m_randomq_miner) {
        m_randomq_miner.reset();
    }
    
    if (m_rpc_client) {
        m_rpc_client.reset();
    }
}

void Miner::log(int level, const std::string& message) const {
    if (level <= m_log_level) {
        const char* level_names[] = {"ERROR", "WARNING", "INFO", "DEBUG"};
        std::cout << "[" << level_names[level] << "] " << message << std::endl;
    }
}
