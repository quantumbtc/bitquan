// Copyright (c) 2024-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/util/setup_common.h>

#include <crypto/randomq.h>
#include <crypto/randomq_hash.h>
#include <crypto/randomq_mining.h>
#include <primitives/block.h>
#include <uint256.h>
#include <arith_uint256.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(randomq_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(randomq_basic_test)
{
    // Test basic RandomQ hashing
    CRandomQ hasher;
    hasher.Initialize(nullptr, 0);
    
    uint8_t input[] = "Hello, RandomQ!";
    hasher.Write(std::span<const uint8_t>(input, sizeof(input) - 1));
    
    uint8_t hash[32];
    hasher.Finalize(hash);
    
    // Verify hash is not all zeros
    bool all_zero = true;
    for (int i = 0; i < 32; i++) {
        if (hash[i] != 0) {
            all_zero = false;
            break;
        }
    }
    BOOST_CHECK(!all_zero);
}

BOOST_AUTO_TEST_CASE(randomq_hash_test)
{
    // Test RandomQ hash (SHA256->RandomQ->SHA256)
    CRandomQHash hasher;
    
    uint8_t input[] = "Test input for RandomQ hash";
    hasher.Write(std::span<const unsigned char>(input, sizeof(input) - 1));
    
    uint256 result;
    hasher.Finalize(result.begin());
    
    // Verify result is not all zeros
    BOOST_CHECK(!result.IsNull());
}

BOOST_AUTO_TEST_CASE(randomq_mining_test)
{
    // Test RandomQ mining functionality
    CBlockHeader header;
    header.nVersion = 1;
    header.hashPrevBlock.SetNull();
    header.hashMerkleRoot.SetNull();
    header.nTime = 1234567890;
    header.nBits = 0x1d00ffff; // Easy difficulty
    header.nNonce = 0;
    
    uint256 powLimit = uint256S("00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
    
    // Test POW check
    bool valid = RandomQMining::CheckRandomQProofOfWork(header, header.nBits, powLimit);
    BOOST_CHECK(!valid); // Should fail with nonce 0
    
    // Test nonce finding
    bool found = RandomQMining::FindRandomQNonce(header, header.nBits, powLimit, 1000);
    if (found) {
        BOOST_CHECK(header.nNonce > 0);
        
        // Verify the found nonce is valid
        bool verified = RandomQMining::CheckRandomQProofOfWork(header, header.nBits, powLimit);
        BOOST_CHECK(verified);
    }
}

BOOST_AUTO_TEST_CASE(randomq_consistency_test)
{
    // Test that RandomQ hash is consistent
    CBlockHeader header1, header2;
    header1.nVersion = 1;
    header1.hashPrevBlock.SetNull();
    header1.hashMerkleRoot.SetNull();
    header1.nTime = 1234567890;
    header1.nBits = 0x1d00ffff;
    header1.nNonce = 42;
    
    header2 = header1; // Copy header1
    
    uint256 hash1 = RandomQMining::CalculateRandomQHash(header1);
    uint256 hash2 = RandomQMining::CalculateRandomQHash(header2);
    
    BOOST_CHECK_EQUAL(hash1, hash2);
    
    // Change nonce and verify hash changes
    header2.nNonce = 43;
    uint256 hash3 = RandomQMining::CalculateRandomQHash(header2);
    BOOST_CHECK(hash1 != hash3);
}

BOOST_AUTO_TEST_CASE(randomq_optimized_test)
{
    // Test optimized RandomQ hash calculation
    CBlockHeader header;
    header.nVersion = 1;
    header.hashPrevBlock.SetNull();
    header.hashMerkleRoot.SetNull();
    header.nTime = 1234567890;
    header.nBits = 0x1d00ffff;
    header.nNonce = 100;
    
    uint256 hash1 = RandomQMining::CalculateRandomQHash(header);
    uint256 hash2 = RandomQMining::CalculateRandomQHashOptimized(header, 100);
    
    BOOST_CHECK_EQUAL(hash1, hash2);
}

BOOST_AUTO_TEST_SUITE_END()
