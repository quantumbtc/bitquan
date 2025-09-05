// Copyright (c) 2024-present The Bitquantum Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"
#include <iostream>
#include <exception>

int main(int argc, char* argv[]) {
    std::cout << "CPUMiner v1.0.0 - Bitquantum RandomQ CPU Miner" << std::endl;
    std::cout << "Copyright (c) 2024-present The Bitquantum Core developers" << std::endl;
    std::cout << std::endl;
    
    try {
        // Create miner instance
        Miner miner;
        
        // Initialize miner
        if (!miner.initialize(argc, argv)) {
            std::cerr << "Failed to initialize miner" << std::endl;
            return 1;
        }
        
        // Start mining
        std::cout << "Starting miner..." << std::endl;
        miner.start();
        
        // Wait for stop signal
        while (miner.isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Print final statistics
        std::cout << "\nFinal Statistics:" << std::endl;
        miner.printStats();
        
        std::cout << "Miner stopped successfully" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return 1;
    }
}
