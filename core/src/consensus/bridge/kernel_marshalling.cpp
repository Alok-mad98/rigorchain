// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RIGOR_KERNEL_ABI_VERIFY_ONLY
#error "consensus/bridge is verify-only; compile with -DRIGOR_KERNEL_ABI_VERIFY_ONLY"
#endif

#include <consensus/bridge/kernel_marshalling.h>

#include <consensus/consensus.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>

#include <cstring>

Consensus::Params KernelFrozenParams()
{
    Consensus::Params p{};
    p.nCoinbaseMaturity = COINBASE_MATURITY;
    return p;
}

uint256 RigorU256ToUint256(const rigor_u256& in)
{
    uint256 out;
    std::memcpy(out.begin(), in.bytes, 32);
    return out;
}

void Uint256ToRigorU256(const uint256& in, rigor_u256* out)
{
    if (!out) return;
    std::memcpy(out->bytes, in.begin(), 32);
}

CScript RigorScriptFromBytes(const uint8_t* script, size_t script_len)
{
    if (!script || script_len == 0) {
        return CScript();
    }
    return CScript(script, script + script_len);
}
