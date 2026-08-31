// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P058: Transaction Kernel spend-predicate boundary.
// Standalone. Does not switch production. Does not recode issuance / PoW.

#include <consensus/txkernel/transaction_kernel_input.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/standard.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int fail(const char* msg)
{
    std::fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static TransactionKernelInput BaseInput()
{
    TransactionKernelInput in;
    in.flags = TK_SCRIPT_VERIFY_WITNESS;
    in.scriptSig_push_only = true;
    in.stack_clean = true;
    in.signature.assign(64, 0x07);
    in.pubkey.assign(32, 0x09);
    in.sighash_tag = {'R', 'G', 'R', 'O', 'R', 'U', '0', '1'};
    in.chain_id.assign(32, 0);
    in.hashPrevouts.assign(32, 0);
    in.hashSequence.assign(32, 0);
    in.hashOutputs.assign(32, 0);
    in.prevout_hash.assign(32, 0);
    in.nVersion = 2;
    return in;
}

static std::vector<unsigned char> Bytes(const CScript& s)
{
    return std::vector<unsigned char>(s.begin(), s.end());
}

int main()
{
    const CScript oru = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(WITNESS_V0_KEYHASH_SIZE, 0x11);
    const CScript res = CScript() << RIGOR_PUSH_1 << std::vector<unsigned char>(WITNESS_V1_RES_SIZE, 0xaa);
    const CScript empty;
    const CScript p2pkh = CScript() << RIGOR_RESERVED_LEGACY_DUP << RIGOR_RESERVED_LEGACY_HASH160
                                    << std::vector<unsigned char>(20, 0x00)
                                    << RIGOR_RESERVED_LEGACY_EQUALVERIFY << RIGOR_RESERVED_LEGACY_CHECKSIG;
    const CScript p2wsh = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(32, 0x00);
    const CScript v2 = CScript() << RIGOR_PUSH_2 << std::vector<unsigned char>(20, 0x00);
    const CScript truncated = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(10, 0x11);

    if (ClassifyRigorAsset(oru) != ASSET_ORU) return fail("classify ORU");
    if (ClassifyRigorAsset(res) != ASSET_RES) return fail("classify RES");
    if (ClassifyRigorAsset(empty) != ASSET_INVALID) return fail("classify empty");
    if (ClassifyRigorAsset(p2pkh) != ASSET_INVALID) return fail("classify P2PKH");
    if (ClassifyRigorAsset(p2wsh) != ASSET_INVALID) return fail("classify P2WSH");
    if (ClassifyRigorAsset(v2) != ASSET_INVALID) return fail("classify unknown version");
    if (ClassifyRigorAsset(truncated) != ASSET_INVALID) return fail("classify bad length");

    // --- valid ORU P2WPKH-style ---
    {
        TransactionKernelInput in = BaseInput();
        in.scriptPubKey = Bytes(oru);
        in.witness = {in.signature, in.pubkey};
        in.keyhash_match = true;
        in.crypto_accept = true;
        if (!VerifyRigorPredicateBoundary(in)) return fail("valid ORU spend");
        if (!TransactionKernelWitnessTwo(in)) return fail("valid witness");
        if (!TransactionKernelEncodingOk(in)) return fail("valid signature encoding");
    }

    // --- valid RES key path ---
    {
        TransactionKernelInput in = BaseInput();
        in.scriptPubKey = Bytes(res);
        in.pubkey.assign(32, 0xaa);
        in.witness = {in.signature, in.pubkey};
        in.keyhash_match = true;
        in.crypto_accept = true;
        if (!VerifyRigorPredicateBoundary(in)) return fail("valid RES key path");
    }

    // --- invalid empty ---
    {
        TransactionKernelInput in = BaseInput();
        in.scriptPubKey = Bytes(empty);
        in.witness = {in.signature, in.pubkey};
        in.keyhash_match = true;
        in.crypto_accept = true;
        if (VerifyRigorPredicateBoundary(in)) return fail("empty must fail closed");
    }

    // --- wrong pubkey (RES program != pk) ---
    {
        TransactionKernelInput in = BaseInput();
        in.scriptPubKey = Bytes(res);
        in.pubkey.assign(32, 0x09);
        in.witness = {in.signature, in.pubkey};
        in.crypto_accept = true;
        if (VerifyRigorPredicateBoundary(in)) return fail("wrong pubkey");
    }

    // --- wrong signature (crypto reject) ---
    {
        TransactionKernelInput in = BaseInput();
        in.scriptPubKey = Bytes(oru);
        in.witness = {in.signature, in.pubkey};
        in.keyhash_match = true;
        in.crypto_accept = false;
        if (VerifyRigorPredicateBoundary(in)) return fail("wrong signature");
    }

    // --- wrong sighash: distinct fields, distinct preimage; spend still gated ---
    {
        TransactionKernelInput a = BaseInput();
        a.scriptPubKey = Bytes(oru);
        a.spent_script = a.scriptPubKey;
        a.amount = 1;
        TransactionKernelInput b = a;
        b.amount = 2;
        const auto pa = CppSighashPreimage(a);
        const auto pb = CppSighashPreimage(b);
        if (pa == pb) return fail("wrong sighash must change preimage");
        if (CppSighashHasHashtypeByte(pa)) return fail("SIGHASH_ALL byte present");
        a.witness = {a.signature, a.pubkey};
        a.keyhash_match = true;
        a.crypto_accept = false;
        if (VerifyRigorPredicateBoundary(a)) return fail("wrong sighash accept");
    }

    // --- malformed witness ---
    {
        TransactionKernelInput in = BaseInput();
        in.scriptPubKey = Bytes(oru);
        in.witness = {in.signature};
        in.keyhash_match = true;
        in.crypto_accept = true;
        if (VerifyRigorPredicateBoundary(in)) return fail("malformed witness");
    }

    // --- unknown script version ---
    {
        TransactionKernelInput in = BaseInput();
        in.scriptPubKey = Bytes(v2);
        in.witness = {in.signature, in.pubkey};
        in.keyhash_match = true;
        in.crypto_accept = true;
        if (VerifyRigorPredicateBoundary(in)) return fail("unknown script version");
    }

    // --- no WITNESS flag: fail closed, never anyone-can-spend ---
    {
        TransactionKernelInput in = BaseInput();
        in.flags = 0;
        in.scriptPubKey = Bytes(oru);
        in.witness = {in.signature, in.pubkey};
        in.keyhash_match = true;
        in.crypto_accept = true;
        if (VerifyRigorPredicateBoundary(in)) return fail("missing WITNESS flag");
    }

    // --- regression: P2PKH / P2WSH / truncated ---
    {
        TransactionKernelInput in = BaseInput();
        in.witness = {in.signature, in.pubkey};
        in.keyhash_match = true;
        in.crypto_accept = true;
        in.scriptPubKey = Bytes(p2pkh);
        if (VerifyRigorPredicateBoundary(in)) return fail("regression P2PKH");
        in.scriptPubKey = Bytes(p2wsh);
        if (VerifyRigorPredicateBoundary(in)) return fail("regression P2WSH");
        in.scriptPubKey = Bytes(truncated);
        if (VerifyRigorPredicateBoundary(in)) return fail("regression truncated");
    }

    std::printf("valid spend: PASS\n");
    std::printf("valid witness: PASS\n");
    std::printf("valid signature: PASS\n");
    std::printf("invalid: PASS\n");
    std::printf("wrong pubkey: PASS\n");
    std::printf("wrong signature: PASS\n");
    std::printf("wrong sighash: PASS\n");
    std::printf("malformed witness: PASS\n");
    std::printf("unknown script version: PASS\n");
    std::printf("regression: PASS\n");
    return 0;
}
