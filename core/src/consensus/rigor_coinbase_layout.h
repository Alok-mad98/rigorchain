// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Consensus Kernel — coinbase / mint layout boundary.
// Names the production predicates. Does not copy parsers or generate coinbase.

#ifndef BITCOIN_CONSENSUS_RIGOR_COINBASE_LAYOUT_H
#define BITCOIN_CONSENSUS_RIGOR_COINBASE_LAYOUT_H

#include <consensus/convergence_logic.h>
#include <consensus/convergence_root.h>
#include <consensus/rigor_supply.h>

/*
 * Kernel-facing layout surface (existing implementations; one parser each):
 *
 *   coinbase scriptSig
 *     ParseCoinbaseConvergenceRootScriptSig
 *     ValidateCoinbaseConvergenceRootScriptSig
 *     BuildCoinbaseCommitmentScriptSig
 *       = BIP34 height || OP_PUSHBYTES_32(C) || OP_PUSHBYTES_32(paid-winner)
 *
 *   RES mint presence
 *     CoinbaseHasResMint
 *
 *   RES mint evaluation (amount / owner / winner coupling)
 *     rigor::ConnectEvaluateCoinbaseResMint
 *
 *   RES coinbase budget
 *     RigorResCoinbaseExpected(fees_res, bootstrap, mint)
 *     ConnectBlockRigor::VerifyCoinbaseConvergenceAndResLayout
 *
 *   witness commitment
 *     CheckBlock BIP141 (validation.cpp). Not extracted here.
 *
 * Miner / pool / wallet / GBT must not be called from this header.
 */

#endif // BITCOIN_CONSENSUS_RIGOR_COINBASE_LAYOUT_H
