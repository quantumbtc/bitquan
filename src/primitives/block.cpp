// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/block.h>

#include <hash.h>
#include <crypto/randomq_hash.h>
#include <streams.h>
#include <tinyformat.h>

uint256 CBlockHeader::GetHash() const
{
    // Use RandomQ hash: SHA256 -> RandomQ -> SHA256
    CRandomQHash hasher;
    std::vector<unsigned char> serialized;
    VectorWriter(serialized, 0, *this);
    hasher.Write(std::span<const unsigned char>(serialized.data(), serialized.size()));
    hasher.SetRandomQNonce(nNonce);
    hasher.SetRandomQRounds(8192);

    uint256 result;
    hasher.Finalize(std::span<unsigned char>(result.begin(), result.size()));
    return result;
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        vtx.size());
    for (const auto& tx : vtx) {
        s << "  " << tx->ToString() << "\n";
    }
    return s.str();
}
