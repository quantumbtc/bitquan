// Copyright (c) 2024-present The Bitquantum Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CPUMINER_RPC_CLIENT_H
#define CPUMINER_RPC_CLIENT_H

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <nlohmann/json.hpp>

// Forward declarations
struct WorkData;
class RandomQMiner;

class RPCClient {
public:
    RPCClient();
    ~RPCClient();
    
    // Initialize RPC connection
    bool initialize(const std::string& host, uint16_t port, 
                   const std::string& user, const std::string& password);
    
    // Start RPC client
    void start();
    
    // Stop RPC client
    void stop();
    
    // Check if connected
    bool isConnected() const { return m_connected; }
    
    // Set miner reference
    void setMiner(RandomQMiner* miner);
    
    // Get current work
    WorkData getCurrentWork() const;
    
    // Submit block
    bool submitBlock(const std::string& block_hex);
    
    // Get blockchain info
    nlohmann::json getBlockchainInfo();
    
    // Get network info
    nlohmann::json getNetworkInfo();
    
    // Get mining info
    nlohmann::json getMiningInfo();

private:
    // RPC communication
    nlohmann::json makeRPCCall(const std::string& method, const nlohmann::json& params = nlohmann::json::array());
    
    // HTTP request
    std::string makeHTTPRequest(const std::string& url, const std::string& data);
    
    // Parse work from block template
    WorkData parseBlockTemplate(const nlohmann::json& template_data);
    
    // Work update thread
    void workUpdateThread();
    
    // Long polling
    void longPollingThread();
    
    // Error handling
    void handleRPCError(const std::string& method, const nlohmann::json& response);
    
    // Logging
    void log(int level, const std::string& message) const;

private:
    // Connection settings
    std::string m_rpc_host;
    uint16_t m_rpc_port;
    std::string m_rpc_user;
    std::string m_rpc_password;
    std::string m_rpc_url;
    
    // Connection state
    std::atomic<bool> m_connected;
    std::atomic<bool> m_running;
    std::atomic<bool> m_should_stop;
    
    // Threading
    std::thread m_work_thread;
    std::thread m_longpoll_thread;
    
    // Work data
    mutable std::mutex m_work_mutex;
    WorkData m_current_work;
    bool m_has_work;
    std::string m_longpoll_id;
    
    // Miner reference
    RandomQMiner* m_miner;
    
    // Configuration
    bool m_long_polling_enabled;
    uint32_t m_work_timeout;
    uint32_t m_retry_interval;
    int m_log_level;
    
    // Statistics
    mutable std::mutex m_stats_mutex;
    uint64_t m_rpc_calls;
    uint64_t m_rpc_errors;
    uint64_t m_work_updates;
    uint64_t m_block_submissions;
    uint64_t m_successful_submissions;
};

// RPC response structure
struct RPCResponse {
    bool success;
    nlohmann::json result;
    nlohmann::json error;
    std::string raw_response;
    
    RPCResponse() : success(false) {}
};

// Block template structure
struct BlockTemplate {
    std::string version;
    std::string previousblockhash;
    std::string target;
    std::string bits;
    uint32_t height;
    uint32_t curtime;
    std::string coinbasevalue;
    std::string longpollid;
    std::vector<nlohmann::json> transactions;
    nlohmann::json coinbaseaux;
    std::vector<std::string> mutable_fields;
    std::string noncerange;
    uint32_t sigoplimit;
    uint32_t sizelimit;
    uint32_t weightlimit;
    
    bool isValid() const;
    WorkData toWorkData() const;
};

#endif // CPUMINER_RPC_CLIENT_H
