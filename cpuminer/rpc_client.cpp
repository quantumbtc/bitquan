// Copyright (c) 2024-present The Bitquantum Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc_client.h"
#include "randomq_miner.h"
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <cstring>

// HTTP response callback for libcurl
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t newLength = size * nmemb;
    try {
        s->append((char*)contents, newLength);
        return newLength;
    } catch (std::bad_alloc& e) {
        return 0;
    }
}

RPCClient::RPCClient()
    : m_rpc_port(18332)
    , m_connected(false)
    , m_running(false)
    , m_should_stop(false)
    , m_has_work(false)
    , m_miner(nullptr)
    , m_long_polling_enabled(true)
    , m_work_timeout(300)
    , m_retry_interval(5)
    , m_log_level(2)
    , m_rpc_calls(0)
    , m_rpc_errors(0)
    , m_work_updates(0)
    , m_block_submissions(0)
    , m_successful_submissions(0)
{
    // Initialize libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

RPCClient::~RPCClient() {
    stop();
    curl_global_cleanup();
}

bool RPCClient::initialize(const std::string& host, uint16_t port, 
                          const std::string& user, const std::string& password) {
    m_rpc_host = host;
    m_rpc_port = port;
    m_rpc_user = user;
    m_rpc_password = password;
    
    // Construct RPC URL
    std::ostringstream oss;
    oss << "http://" << m_rpc_user << ":" << m_rpc_password 
        << "@" << m_rpc_host << ":" << m_rpc_port;
    m_rpc_url = oss.str();
    
    log(2, "RPC client initialized for " + m_rpc_host + ":" + std::to_string(m_rpc_port));
    return true;
}

void RPCClient::start() {
    if (m_running) {
        log(1, "RPC client is already running");
        return;
    }
    
    m_running = true;
    m_should_stop = false;
    
    // Start work update thread
    m_work_thread = std::thread(&RPCClient::workUpdateThread, this);
    
    // Start long polling thread if enabled
    if (m_long_polling_enabled) {
        m_longpoll_thread = std::thread(&RPCClient::longPollingThread, this);
    }
    
    log(2, "RPC client started");
}

void RPCClient::stop() {
    if (!m_running) {
        return;
    }
    
    log(2, "Stopping RPC client...");
    m_should_stop = true;
    
    // Wait for threads to finish
    if (m_work_thread.joinable()) {
        m_work_thread.join();
    }
    
    if (m_longpoll_thread.joinable()) {
        m_longpoll_thread.join();
    }
    
    m_running = false;
    m_connected = false;
    
    log(2, "RPC client stopped");
}

void RPCClient::setMiner(RandomQMiner* miner) {
    m_miner = miner;
}

WorkData RPCClient::getCurrentWork() const {
    std::lock_guard<std::mutex> lock(m_work_mutex);
    return m_current_work;
}

bool RPCClient::submitBlock(const std::string& block_hex) {
    nlohmann::json params = nlohmann::json::array();
    params.push_back(block_hex);
    
    nlohmann::json response = makeRPCCall("submitblock", params);
    
    {
        std::lock_guard<std::mutex> lock(m_stats_mutex);
        m_block_submissions++;
    }
    
    if (response.is_null()) {
        log(2, "Block submitted successfully");
        {
            std::lock_guard<std::mutex> lock(m_stats_mutex);
            m_successful_submissions++;
        }
        return true;
    } else {
        log(0, "Block submission failed: " + response.dump());
        return false;
    }
}

nlohmann::json RPCClient::getBlockchainInfo() {
    return makeRPCCall("getblockchaininfo");
}

nlohmann::json RPCClient::getNetworkInfo() {
    return makeRPCCall("getnetworkinfo");
}

nlohmann::json RPCClient::getMiningInfo() {
    return makeRPCCall("getmininginfo");
}

nlohmann::json RPCClient::makeRPCCall(const std::string& method, const nlohmann::json& params) {
    nlohmann::json request;
    request["jsonrpc"] = "2.0";
    request["id"] = "1";
    request["method"] = method;
    request["params"] = params;
    
    std::string request_data = request.dump();
    
    // Make HTTP request
    std::string response_data = makeHTTPRequest(m_rpc_url, request_data);
    
    {
        std::lock_guard<std::mutex> lock(m_stats_mutex);
        m_rpc_calls++;
    }
    
    if (response_data.empty()) {
        log(0, "RPC call failed: Empty response");
        {
            std::lock_guard<std::mutex> lock(m_stats_mutex);
            m_rpc_errors++;
        }
        return nlohmann::json();
    }
    
    try {
        nlohmann::json response = nlohmann::json::parse(response_data);
        
        if (response.contains("error") && !response["error"].is_null()) {
            handleRPCError(method, response);
            return nlohmann::json();
        }
        
        return response.contains("result") ? response["result"] : nlohmann::json();
    } catch (const std::exception& e) {
        log(0, "RPC response parsing failed: " + std::string(e.what()));
        {
            std::lock_guard<std::mutex> lock(m_stats_mutex);
            m_rpc_errors++;
        }
        return nlohmann::json();
    }
}

std::string RPCClient::makeHTTPRequest(const std::string& url, const std::string& data) {
    CURL* curl;
    CURLcode res;
    std::string response_data;
    
    curl = curl_easy_init();
    if (!curl) {
        log(0, "Failed to initialize libcurl");
        return "";
    }
    
    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    
    // Set POST data
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.length());
    
    // Set headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Set response callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    
    // Set timeout
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    // Perform request
    res = curl_easy_perform(curl);
    
    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        log(0, "HTTP request failed: " + std::string(curl_easy_strerror(res)));
        return "";
    }
    
    return response_data;
}

WorkData RPCClient::parseBlockTemplate(const nlohmann::json& template_data) {
    WorkData work;
    
    try {
        work.version = template_data["version"].get<uint32_t>();
        work.previous_block_hash = template_data["previousblockhash"].get<std::string>();
        work.target = template_data["target"].get<std::string>();
        work.bits = template_data["bits"].get<std::string>();
        work.height = template_data["height"].get<uint32_t>();
        work.timestamp = template_data["curtime"].get<uint32_t>();
        
        // Parse transactions
        if (template_data.contains("transactions") && template_data["transactions"].is_array()) {
            for (const auto& tx : template_data["transactions"]) {
                work.transactions.push_back(tx["data"].get<std::string>());
            }
        }
        
        // Set nonce range
        work.nonce_start = 0;
        work.nonce_end = UINT32_MAX;
        
        // Set coinbase transaction (simplified)
        work.coinbase_tx = "01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff";
        
        // Calculate merkle root (simplified)
        work.merkle_root = "0000000000000000000000000000000000000000000000000000000000000000";
        
        work.block_template = template_data.dump();
        
    } catch (const std::exception& e) {
        log(0, "Failed to parse block template: " + std::string(e.what()));
        work = WorkData(); // Return invalid work
    }
    
    return work;
}

void RPCClient::workUpdateThread() {
    log(2, "Work update thread started");
    
    while (!m_should_stop) {
        try {
            // Get block template
            nlohmann::json template_request;
            template_request["rules"] = nlohmann::json::array();
            template_request["rules"].push_back("segwit");
            
            nlohmann::json template_data = makeRPCCall("getblocktemplate", template_request);
            
            if (!template_data.is_null()) {
                WorkData work = parseBlockTemplate(template_data);
                
                if (work.isValid()) {
                    {
                        std::lock_guard<std::mutex> lock(m_work_mutex);
                        m_current_work = work;
                        m_has_work = true;
                    }
                    
                    if (m_miner) {
                        m_miner->setWork(work);
                    }
                    
                    {
                        std::lock_guard<std::mutex> lock(m_stats_mutex);
                        m_work_updates++;
                    }
                    
                    log(2, "Work updated - Height: " + std::to_string(work.height) + 
                         ", Target: " + work.target);
                    
                    m_connected = true;
                } else {
                    log(1, "Invalid work data received");
                }
            } else {
                log(1, "Failed to get block template");
                m_connected = false;
            }
            
        } catch (const std::exception& e) {
            log(0, "Work update error: " + std::string(e.what()));
            m_connected = false;
        }
        
        // Wait before next update
        std::this_thread::sleep_for(std::chrono::seconds(m_retry_interval));
    }
    
    log(2, "Work update thread stopped");
}

void RPCClient::longPollingThread() {
    log(2, "Long polling thread started");
    
    while (!m_should_stop) {
        try {
            // Get longpoll ID from current work
            std::string longpoll_id;
            {
                std::lock_guard<std::mutex> lock(m_work_mutex);
                if (m_has_work) {
                    // Parse longpoll ID from block template
                    try {
                        nlohmann::json work_json = nlohmann::json::parse(m_current_work.block_template);
                        if (work_json.contains("longpollid")) {
                            longpoll_id = work_json["longpollid"].get<std::string>();
                        }
                    } catch (...) {
                        // Ignore parsing errors
                    }
                }
            }
            
            if (!longpoll_id.empty()) {
                // Make longpoll request
                nlohmann::json longpoll_request;
                longpoll_request["rules"] = nlohmann::json::array();
                longpoll_request["rules"].push_back("segwit");
                longpoll_request["longpollid"] = longpoll_id;
                
                nlohmann::json longpoll_data = makeRPCCall("getblocktemplate", longpoll_request);
                
                if (!longpoll_data.is_null()) {
                    WorkData work = parseBlockTemplate(longpoll_data);
                    
                    if (work.isValid()) {
                        {
                            std::lock_guard<std::mutex> lock(m_work_mutex);
                            m_current_work = work;
                            m_has_work = true;
                        }
                        
                        if (m_miner) {
                            m_miner->setWork(work);
                        }
                        
                        log(2, "Long poll work updated - Height: " + std::to_string(work.height));
                    }
                }
            }
            
        } catch (const std::exception& e) {
            log(0, "Long polling error: " + std::string(e.what()));
        }
        
        // Wait before next longpoll
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    log(2, "Long polling thread stopped");
}

void RPCClient::handleRPCError(const std::string& method, const nlohmann::json& response) {
    std::string error_msg = "RPC error in " + method;
    
    if (response["error"].contains("message")) {
        error_msg += ": " + response["error"]["message"].get<std::string>();
    }
    
    if (response["error"].contains("code")) {
        error_msg += " (Code: " + std::to_string(response["error"]["code"].get<int>()) + ")";
    }
    
    log(0, error_msg);
    
    {
        std::lock_guard<std::mutex> lock(m_stats_mutex);
        m_rpc_errors++;
    }
}

void RPCClient::log(int level, const std::string& message) const {
    if (level <= m_log_level) {
        const char* level_names[] = {"ERROR", "WARNING", "INFO", "DEBUG"};
        std::cout << "[RPC-" << level_names[level] << "] " << message << std::endl;
    }
}

// BlockTemplate implementation
bool BlockTemplate::isValid() const {
    return !version.empty() && 
           !previousblockhash.empty() && 
           !target.empty() && 
           !bits.empty() && 
           height > 0;
}

WorkData BlockTemplate::toWorkData() const {
    WorkData work;
    
    work.version = std::stoul(version, nullptr, 16);
    work.previous_block_hash = previousblockhash;
    work.target = target;
    work.bits = std::stoul(bits, nullptr, 16);
    work.height = height;
    work.timestamp = curtime;
    work.nonce_start = 0;
    work.nonce_end = UINT32_MAX;
    
    // Convert transactions
    for (const auto& tx : transactions) {
        if (tx.contains("data")) {
            work.transactions.push_back(tx["data"].get<std::string>());
        }
    }
    
    return work;
}
