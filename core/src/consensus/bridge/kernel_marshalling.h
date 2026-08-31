// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Type conversion only. No issuance / draw / classify / supply formula.

#ifndef BITCOIN_CONSENSUS_BRIDGE_KERNEL_MARSHALLING_H
#define BITCOIN_CONSENSUS_BRIDGE_KERNEL_MARSHALLING_H

#ifndef RIGOR_KERNEL_ABI_VERIFY_ONLY
#error "consensus/bridge is verify-only; compile with -DRIGOR_KERNEL_ABI_VERIFY_ONLY"
#endif

#include <consensus/bridge/kernel_types.h>
#include <consensus/params.h>
#include <script/script.h>
#include <uint256.h>

Consensus::Params KernelFrozenParams();

uint256 RigorU256ToUint256(const rigor_u256& in);
void Uint256ToRigorU256(const uint256& in, rigor_u256* out);

CScript RigorScriptFromBytes(const uint8_t* script, size_t script_len);

#endif
