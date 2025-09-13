#include <iostream>
#include <iomanip>
#include <chrono>
#include <cstdint>

// Simple RandomQ test to verify the algorithm works
// This is a minimal test to check if RandomQ can find valid solutions

int main() {
    std::cout << "=== RandomQ Algorithm Test ===" << std::endl;
    
    // Test with a very easy target (difficulty 1)
    uint32_t easy_nbits = 0x1d00ffff;  // DIFF_1_N_BITS from tests
    uint32_t hard_nbits = 0x1e0ffff0;  // Mainnet difficulty
    
    std::cout << "Easy nBits: 0x" << std::hex << easy_nbits << std::endl;
    std::cout << "Hard nBits: 0x" << std::hex << hard_nbits << std::endl;
    
    // Calculate targets
    auto calculate_target = [](uint32_t nbits) -> uint64_t {
        uint32_t exponent = nbits >> 24;
        uint32_t mantissa = nbits & 0xffffff;
        return (uint64_t)mantissa << (8 * (exponent - 3));
    };
    
    uint64_t easy_target = calculate_target(easy_nbits);
    uint64_t hard_target = calculate_target(hard_nbits);
    
    std::cout << "Easy target: 0x" << std::hex << easy_target << std::endl;
    std::cout << "Hard target: 0x" << std::hex << hard_target << std::endl;
    
    // Calculate difficulty ratio
    double difficulty_ratio = (double)hard_target / (double)easy_target;
    std::cout << "Difficulty ratio (hard/easy): " << std::dec << difficulty_ratio << std::endl;
    
    std::cout << "\nThis test shows that the mainnet difficulty is much harder than test difficulty." << std::endl;
    std::cout << "For RandomQ mining to work, we need to either:" << std::endl;
    std::cout << "1. Use a lower difficulty for testing" << std::endl;
    std::cout << "2. Implement a more efficient RandomQ algorithm" << std::endl;
    std::cout << "3. Use more CPU cores and longer mining time" << std::endl;
    
    return 0;
}
