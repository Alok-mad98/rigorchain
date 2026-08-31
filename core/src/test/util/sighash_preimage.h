// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TEST_UTIL_SIGHASH_PREIMAGE_H
#define BITCOIN_TEST_UTIL_SIGHASH_PREIMAGE_H

#include <hash.h>
#include <primitives/transaction.h>
#include <script/predicate_verify.h>
#include <script/script.h>
#include <span.h>
#include <streams.h>

#include <boost/test/unit_test.hpp>

#include <vector>

/**
 * Independent HashWriter concat matching Coq SighashPreimage.sighash_preimage
 * and C++ SignatureHash's SER_GETHASH layout. One definition for both
 * sighash_tests and rigor_sighash_vector_tests.
 */
inline std::vector<unsigned char> ReconstructSighashPreimage(const CScript& spent_script,
                                                            const CMutableTransaction& txTo,
                                                            unsigned int nIn, const CAmount amount,
                                                            SigVersion sigversion,
                                                            const uint256& chain_id)
{
    BOOST_REQUIRE(nIn < txTo.vin.size());
    HashWriter prevs{};
    HashWriter seqs{};
    HashWriter outs{};
    for (const auto& txin : txTo.vin) {
        prevs << txin.prevout;
        seqs << txin.nSequence;
    }
    for (const auto& txout : txTo.vout) {
        outs << txout;
    }
    const uint256 hashPrevouts = SHA256Uint256(prevs.GetSHA256());
    const uint256 hashSequence = SHA256Uint256(seqs.GetSHA256());
    const uint256 hashOutputs = SHA256Uint256(outs.GetSHA256());

    std::vector<unsigned char> buf;
    CVectorWriter ss{SER_GETHASH, 0, buf, 0};
    ss.write(AsBytes(GetSighashDomainTag(sigversion)));
    ss << chain_id;
    ss << txTo.nVersion;
    ss << hashPrevouts;
    ss << hashSequence;
    ss << hashOutputs;
    ss << txTo.vin[nIn].prevout;
    ss << spent_script;
    ss << amount;
    ss << txTo.vin[nIn].nSequence;
    ss << txTo.nLockTime;
    return buf;
}

#endif // BITCOIN_TEST_UTIL_SIGHASH_PREIMAGE_H
