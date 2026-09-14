// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Consensus Kernel — coinbase layout boundary.
// Names the production predicates. Does not copy parsers or generate coinbase.

#ifndef BITCOIN_CONSENSUS_RIGOR_COINBASE_LAYOUT_H
#define BITCOIN_CONSENSUS_RIGOR_COINBASE_LAYOUT_H

#include <consensus/rigor_supply.h>

/*
 * Kernel-facing layout surface (existing implementations; one parser each):
 *
 *   coinbase scriptSig
 *     = BIP34 height push, and nothing else.
 *
 *     Under RFC-017 the height was the first of three pushes, followed by the convergence
 *     root C and the paid-winner txid, each parsed and validated by its own function. RFC-019
 *     removed the draw, so there is no root to commit to and no winner to name: the scriptSig
 *     carries no consensus payload beyond the height, and the coinbase mints no RES at all.
 *
 *   RES coinbase budget
 *     RigorResCoinbaseExpected(fees_res, bootstrap)
 *       The coinbase may still carry RES fees and the devtest bootstrap. It may not mint.
 *       New RES comes only from body transactions that paid destroy_cost(h) for it.
 *
 *   witness commitment
 *     CheckBlock BIP141 (validation.cpp). Not extracted here.
 *
 * Miner / pool / wallet / GBT must not be called from this header.
 */

#endif // BITCOIN_CONSENSUS_RIGOR_COINBASE_LAYOUT_H
