// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TEST_UTIL_SCRIPT_H
#define BITCOIN_TEST_UTIL_SCRIPT_H

#include <script/predicate_verify.h>
#include <script/script.h>

#include <vector>

static const std::vector<uint8_t> WITNESS_STACK_ELEM_OP_TRUE{uint8_t{RIGOR_PUSH_1}};
/** Dummy witness-v0 P2WPKH (20-byte program). P2WSH is not a consensus output. */
static const CScript P2WSH_OP_TRUE{
    CScript{} << RIGOR_PUSH_0 << std::vector<uint8_t>(WITNESS_V0_KEYHASH_SIZE, uint8_t{0x11})};

/** RFC 8032 Ed25519 test vector 1 public key (canonical, prime-order subgroup). */
inline std::vector<unsigned char> TestEd25519PubKey()
{
    return {0xd7, 0x5a, 0x98, 0x01, 0x82, 0xb1, 0x0a, 0xb7, 0xd5, 0x4b, 0xfe, 0xd3, 0xc9, 0x64, 0x07, 0x3a,
            0x0e, 0xe1, 0x72, 0xf3, 0xda, 0xa6, 0x23, 0x25, 0xaf, 0x02, 0x1a, 0x68, 0xf7, 0x07, 0x51, 0x1a};
}

/** RFC 8032 Ed25519 test vector 2 public key. */
inline std::vector<unsigned char> TestEd25519PubKey2()
{
    return {0x3d, 0x40, 0x17, 0xc3, 0xe8, 0x43, 0x89, 0x5a, 0x92, 0xb7, 0x0a, 0xa7, 0x4d, 0x1b, 0x7e, 0xbc,
            0x9c, 0x98, 0x2c, 0xcf, 0x2e, 0xc4, 0x96, 0x8c, 0xc0, 0xcd, 0x55, 0xf1, 0x2a, 0xf4, 0x66, 0x0c};
}

inline CScript TestNativeResScript()
{
    return CScript() << RIGOR_PUSH_1 << TestEd25519PubKey();
}

inline CScript TestNativeResScript2()
{
    return CScript() << RIGOR_PUSH_1 << TestEd25519PubKey2();
}

/** Flags that are not forbidden by an assert in script validation */
bool IsValidFlagCombination(unsigned flags);

#endif // BITCOIN_TEST_UTIL_SCRIPT_H
