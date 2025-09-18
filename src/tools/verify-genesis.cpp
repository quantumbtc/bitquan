/**
 * Genesis Block Verification Tool for GPU Miner
 * 
 * This tool verifies that the GPU RandomQ mining algorithm produces
 * the correct hash for the genesis block with known parameters.
 * 
 * Genesis Block Parameters:
 * - Time: 1756857263
 * - Nonce: 1379716
 * - Bits: 0x1e0ffff0
 * - Version: 1
 * - Expected Hash: 00000c62fac2d483d65c37331a3a73c6f315de2541e7384e94e36d3b1491604f
 * - Merkle Root: b0e14069031ce67080e53fe3d2cdbc23d0949fd85efac43e67ffdcf07d66d541
 */

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>

#include <crypto/randomq.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <primitives/block.h>
#include <streams.h>

// Genesis block parameters from chainparams.cpp
const uint32_t GENESIS_TIME = 1756857263;
const uint32_t GENESIS_NONCE = 1379716;
const uint32_t GENESIS_BITS = 0x1e0ffff0;
const int32_t GENESIS_VERSION = 1;
const std::string EXPECTED_HASH = "00000c62fac2d483d65c37331a3a73c6f315de2541e7384e94e36d3b1491604f";
const std::string EXPECTED_MERKLE = "b0e14069031ce67080e53fe3d2cdbc23d0949fd85efac43e67ffdcf07d66d541";

// Convert bytes to hex string
std::string BytesToHex(const unsigned char* bytes, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i) {
        oss << std::hex << std::setfill('0') << std::setw(2) << (unsigned int)bytes[i];
    }
    return oss.str();
}

// Convert hex string to bytes
std::vector<unsigned char> HexToBytes(const std::string& hex) {
    std::vector<unsigned char> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        unsigned char byte = static_cast<unsigned char>(strtol(byteString.c_str(), nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

// Create the genesis block header (80 bytes)
void CreateGenesisHeader(unsigned char header[80]) {
    // Clear header
    memset(header, 0, 80);
    
    // Version (4 bytes, little-endian)
    uint32_t version = GENESIS_VERSION;
    memcpy(header + 0, &version, 4);
    
    // Previous block hash (32 bytes) - all zeros for genesis
    // Already zeroed by memset
    
    // Merkle root (32 bytes) - from chainparams.cpp
    auto merkle_bytes = HexToBytes(EXPECTED_MERKLE);
    std::reverse(merkle_bytes.begin(), merkle_bytes.end()); // Convert to little-endian
    memcpy(header + 36, merkle_bytes.data(), 32);
    
    // Time (4 bytes, little-endian)
    uint32_t time = GENESIS_TIME;
    memcpy(header + 68, &time, 4);
    
    // Bits (4 bytes, little-endian)
    uint32_t bits = GENESIS_BITS;
    memcpy(header + 72, &bits, 4);
    
    // Nonce (4 bytes, little-endian)
    uint32_t nonce = GENESIS_NONCE;
    memcpy(header + 76, &nonce, 4);
}

// Verify RandomQ hash matches expected result
bool VerifyRandomQHash(const unsigned char header[80]) {
    std::cout << "=== RandomQ Hash Verification ===" << std::endl;
    
    // Print header hex
    std::cout << "Header (80 bytes): " << BytesToHex(header, 80) << std::endl;
    
    // Compute RandomQ hash
    uint256 hash = RandomQHash(header);
    std::string computed_hash = hash.GetHex();
    
    std::cout << "Computed Hash: " << computed_hash << std::endl;
    std::cout << "Expected Hash: " << EXPECTED_HASH << std::endl;
    
    bool matches = (computed_hash == EXPECTED_HASH);
    std::cout << "Hash Match: " << (matches ? "✅ PASS" : "❌ FAIL") << std::endl;
    
    return matches;
}

// Test individual components
void TestComponents() {
    std::cout << "\n=== Component Tests ===" << std::endl;
    
    // Test with known test vectors if available
    std::cout << "Genesis Block Parameters:" << std::endl;
    std::cout << "  Version: " << GENESIS_VERSION << std::endl;
    std::cout << "  Time: " << GENESIS_TIME << std::endl;
    std::cout << "  Nonce: " << GENESIS_NONCE << std::endl;
    std::cout << "  Bits: 0x" << std::hex << GENESIS_BITS << std::dec << std::endl;
    std::cout << "  Expected Merkle: " << EXPECTED_MERKLE << std::endl;
    std::cout << "  Expected Hash: " << EXPECTED_HASH << std::endl;
}

int main() {
    std::cout << "🔍 Genesis Block Verification Tool" << std::endl;
    std::cout << "===================================" << std::endl;
    
    TestComponents();
    
    // Create genesis block header
    unsigned char header[80];
    CreateGenesisHeader(header);
    
    std::cout << "\n=== Header Analysis ===" << std::endl;
    std::cout << "Header breakdown:" << std::endl;
    std::cout << "  Version (0-3):    " << BytesToHex(header + 0, 4) << std::endl;
    std::cout << "  PrevHash (4-35):  " << BytesToHex(header + 4, 32) << std::endl;
    std::cout << "  MerkleRoot (36-67): " << BytesToHex(header + 36, 32) << std::endl;
    std::cout << "  Time (68-71):     " << BytesToHex(header + 68, 4) << std::endl;
    std::cout << "  Bits (72-75):     " << BytesToHex(header + 72, 4) << std::endl;
    std::cout << "  Nonce (76-79):    " << BytesToHex(header + 76, 4) << std::endl;
    
    // Verify the hash
    bool success = VerifyRandomQHash(header);
    
    std::cout << "\n=== Final Result ===" << std::endl;
    if (success) {
        std::cout << "🎉 SUCCESS: GPU RandomQ algorithm is CORRECT!" << std::endl;
        std::cout << "The algorithm produces the expected genesis block hash." << std::endl;
        return 0;
    } else {
        std::cout << "💥 FAILURE: GPU RandomQ algorithm has ISSUES!" << std::endl;
        std::cout << "The computed hash does not match the expected genesis hash." << std::endl;
        std::cout << "Please check the RandomQ implementation." << std::endl;
        return 1;
    }
}
