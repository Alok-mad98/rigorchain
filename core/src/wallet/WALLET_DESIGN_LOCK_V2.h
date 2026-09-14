// Copyright (c) The RigorChain / RingCore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_WALLET_DESIGN_LOCK_V2_H
#define BITCOIN_WALLET_WALLET_DESIGN_LOCK_V2_H

/**
 * @file  WALLET_DESIGN_LOCK_V2.h
 *
 * TAG: WALLET_DESIGN_LOCK_V2
 *
 * RigorChain wallet (GUI + backend) design contract. Do not relax these rules
 * without a new version tag and explicit review.
 *
 * ---------------------------------------------------------------------------
 * WHY V2 SUPERSEDES V1
 * ---------------------------------------------------------------------------
 *
 * V1 was written against RFC-017, where Convergence was a lottery: burn ORU,
 * and at most one block per window paid a winner W(h) RES. Its entire section
 * III existed to stop the wallet from becoming a betting terminal — no win
 * probability, no odds, no outcome prediction, "Convergence does NOT guarantee
 * RES".
 *
 * RFC-019 replaced the draw with a deterministic destroy cost. RFC-020 keeps
 * that body-tx mint and replaces the epoch step with continuous destroy_cost(h):
 * R · (2^25+h)^2 <= B << 40. There is no draw, no winner, no probability,
 * and no mint horizon. Every V1 rule phrased in terms of odds is vacuous,
 * and "Convergence does NOT guarantee RES" is false: paying the price
 * does guarantee RES.
 *
 * The RES mint maturity lock is NOT part of what RFC-019 removed. RFC-019
 * removes the coinbase payment lag; the script-level lock on minted RES
 * outputs survives, and its rationale strengthens — see III.2(e).
 *
 * The hazard did not disappear, it inverted. destroy_cost(h) is strictly increasing
 * in height. A rising, published, deterministic price is exactly the shape
 * of an investment pitch. V1 guarded against gambling framing. V2 must guard
 * against appreciation framing. Section III below is rewritten accordingly;
 * sections I.1-I.3, II, IV and V carry over from V1 essentially unchanged,
 * because dual-native asset separation was never a function of the draw.
 *
 * ---------------------------------------------------------------------------
 * I. NON-NEGOTIABLE PRINCIPLES
 * ---------------------------------------------------------------------------
 *
 * 1) Single address format, multiple assets (never conflate)   [carried from V1]
 *    - All user-facing addresses: Bech32 `rig1...`.
 *    - Address != asset type. The same address string does not imply a single
 *      asset; UTXOs carry asset tags at the protocol level.
 *    - UI MUST always distinguish ORU vs RES. Every send/receive/display path
 *      MUST name the asset explicitly.
 *
 *    FORBIDDEN:
 *    - Auto-inferring asset type from context or "default".
 *    - Showing a single "Total Balance" that adds ORU and RES.
 *    - One wallet-constructed send that mixes ORU and RES inputs/outputs.
 *      The mint transaction is NOT an exception to this: see III.2, it spends
 *      ORU only and creates RES from the burn, it does not transfer RES.
 *
 * 2) Dual-native assets are peers, not a "token" hierarchy     [carried from V1]
 *    - ORU and RES are both L1 native. No "primary coin / secondary token"
 *      framing in UI.
 *    - FORBIDDEN in UI copy: "Token", "Secondary", "wrapped", "ERC-20-like".
 *
 *    REQUIRED balance presentation (example structure):
 *      ORU:                          RES:
 *        Available:  ...               Available:  ...
 *        Pending:    ...               Pending:    ...
 *
 *    FORBIDDEN: "Total Balance: ..." (single number mixing assets).
 *
 * 3) UTXO model stays pure                                     [carried from V1]
 *    - Wallet remains UTXO-based. Do not introduce an account-based
 *      abstraction that hides inputs/outputs from advanced users.
 *
 * 4) Minting is a purchase at a protocol price, not a yield     [REWRITTEN]
 *    - Convergence = irreversibly destroy ORU to create RES at the price the
 *      protocol fixes at the confirming height. It is a cost, not a return.
 *    - FORBIDDEN in UI copy: "guaranteed yield", "APR", "interest rate",
 *      "fixed return", "staking", "rewards".
 *    - ALSO FORBIDDEN, and specific to V2: any presentation of the rising
 *      price curve as gain, appreciation, scarcity or timing pressure. See
 *      III.4 for the exhaustive rule. This is the single highest-risk copy
 *      surface in the wallet.
 *
 * ---------------------------------------------------------------------------
 * II. TRANSFER (SEND) UI                                       [carried from V1]
 * ---------------------------------------------------------------------------
 *
 * Required layout (conceptual):
 *   [ Send ]
 *   Asset:  (o) ORU   ( ) RES     <- explicit, mandatory selection
 *   Address: rig1...
 *   Amount:  ___
 *
 * FORBIDDEN:
 * - Choosing asset from "which balance is larger" or any automatic rule.
 * - Mixed inputs from different assets in one wallet-constructed send.
 *
 * Underlying construction rules (wallet + RPC):
 * - Selected inputs MUST be single-asset for that send.
 * - Outputs for that send MUST be the same asset (change + recipient).
 * - Network fee MUST be paid in ORU (POOL-F); never inferred from the
 *   transfer asset.
 *
 * ---------------------------------------------------------------------------
 * III. MINTING — wallet expression                             [REWRITTEN]
 * ---------------------------------------------------------------------------
 *
 * 1) Correct mental model
 *
 *    Convergence = destroy ORU to mint RES at destroy_cost(h) ORU per RES.
 *
 *    REQUIRED framing: burn-to-mint. The ORU is provably destroyed and the RES
 *    is newly created; total ORU supply falls and total RES supply rises.
 *
 *    FORBIDDEN copy: "Convert ORU to RES", "Exchange", "Swap", "trade", "sell
 *    ORU for RES", "redeem". All of these imply a counterparty, a market or a
 *    reversible position, and none of the three exists. Determinism of the
 *    price does not make it a conversion: nothing receives the ORU.
 *
 *    "guaranteed RES" is no longer forbidden as a factual claim — paying the
 *    price does produce RES — but it MUST NOT be used as promotional copy,
 *    because in an investment sentence "guaranteed" reads as guaranteed
 *    *value*, which nothing here provides.
 *
 * 2) Transaction shape the wallet MUST build (RFC-020)
 *
 *      inputs        ORU only
 *      outputs       exactly one OP_CONVERGE output, value B  (ORU, destroyed)
 *                    one or more RES outputs, total R
 *                    optionally ORU change
 *      fee           oru_in - B - oru_change, paid in ORU as usual
 *
 *    Consensus admits it at the CONFIRMING height h:
 *
 *      R > 0 -> R * (2^25 + h)^2 <= B << 40
 *      R = 0 -> B unconstrained                (a plain burn stays legal)
 *
 *    Consequences the wallet MUST honour:
 *
 *    a) The relation is an INEQUALITY, so underpaying R for a given B is
 *       valid consensus-wise and silently destroys the user's ORU for nothing.
 *       The wallet MUST maximise R for the chosen B under the pole predicate
 *       — or, if it deliberately leaves slack (see (b)), MUST show the
 *       slack as an explicit line item. Silent slack is FORBIDDEN.
 *
 *    b) HEIGHT INVALIDATION. h is taken from the confirming block, not from
 *       the tip the wallet built against, and destroy_cost(h) is strictly increasing.
 *       A mint that is exact at height h can fail at h+1. The wallet MUST pad
 *       B so the transaction remains valid a few blocks ahead, or warn. Building
 *       an exact-fit mint with no slack is FORBIDDEN. This is the V2
 *       replacement for V1's "your ticket did not win" state: the one
 *       remaining way a mint can fail is arithmetic and foreseeable, not chance.
 *
 *    c) Fractional purchase is permitted. R is any positive number of RES
 *       atoms; "one RES costs destroy_cost(h) ORU" is only the R = RES_COIN headline.
 *       The wallet MUST NOT round the user's request up to whole RES.
 *
 *    d) R = 0 is legal. The pure-destroy path (destroy ORU, mint nothing) MUST
 *       remain reachable via the destroyoru RPC and MUST be labelled as a
 *       destroy, never as a mint that failed. The GUI MUST NOT expose this
 *       path: Send is transfer, Convergence is mint.
 *
 *    e) MINTED RES IS TIME-LOCKED, AND THIS SURVIVES RFC-019. The mint UTXO is
 *       a Witness v1 RES output created in the same transaction as the
 *       OP_CONVERGE burn. Consensus marks it COIN_FLAG_RES_MINT and rejects a
 *       spend while nSpendHeight - coin.nHeight < nCoinbaseMaturity
 *       (bad-txns-premature-spend-of-res-mint). Do not encode the lock in
 *       GetResMintScript / ParseResMintScript: body mint using that script is
 *       bad-txns-res-mint-noncoinbase.
 *
 *       Rationale: a mint CREATES supply, and its validity depends on the
 *       height of the block that confirms it. A reorg that re-mines the mint
 *       at a later height can invalidate an exact-fit burn — price(h+1) >
 *       destroy_cost(h) — and unwind every descendant. The lock bounds that
 *       exposure, for the same reason coinbase maturity does.
 *
 *       What RFC-019 removed is the one-block COINBASE PAYMENT LAG, not this
 *       lock. Do not read RFC-019 section 4's "no maturity period" as
 *       abolishing it; section 1 lists the script machinery as unchanged and
 *       out of scope.
 *
 *       The wallet MUST therefore:
 *       - classify Convergence+RES as ResMint (not OruConvergence), including
 *         the outputs-only fallback, so IsTxImmatureResMint / GetTxBlocksToResMintMaturity
 *         keep minted RES out of the spendable balance until the lock expires,
 *         and report it under a distinct pending/locked line, never folded into
 *         Available
 *       - show the unlock height or remaining confirmations
 *       - apply the lock to MINTS ONLY. RES transfers and RES change are not
 *         locked; mis-applying it hides mempool RES change from both available
 *         and locked balances
 *       - label it as reorg protection on newly created supply. Labelling it as
 *         a waiting period for an outcome is FORBIDDEN: the mint already
 *         succeeded, and V1's outcome vocabulary must not survive here.
 *
 * 3) GUI MUST show
 *
 *    (1) Price:  destroy_cost(h) at the confirming height, sourced from the node.
 *    (2) Cost:   B in ORU, and the minimum burn if one is configured.
 *    (3) Result: the exact R in RES the transaction will create. This is
 *        computable and therefore MUST be shown exactly, before signing.
 *    (4) Finality: that the ORU is destroyed and cannot be recovered.
 *    (5) Lock: the unlock height of the minted RES, per III.2(e), presented as
 *        reorg protection on new supply.
 *
 *    There is no outcome state to display. A confirmed mint has minted. The
 *    only thing still pending is spendability, and that is a known height, not
 *    a result.
 *
 *    FORBIDDEN: any "Status / Result" column implying an outcome (Won, Lost,
 *    Not selected, Pending result). Ordinary confirmation depth is not an
 *    outcome and MUST use the same presentation as any other transaction. The
 *    lock from III.2(e) MUST NOT reuse V1's "Maturing" label, which meant
 *    "waiting to learn whether you won".
 *
 * 4) The appreciation prohibition (V2's core new rule)
 *
 *    The wallet MAY show the current destroy_cost(h) and MAY show that a later
 *    height's price is higher, stated as a future COST.
 *
 *    Permitted:   "Current price: 1 024 ORU per RES. At a later height the
 *                  protocol cost is higher."
 *    FORBIDDEN:   any of the following, in copy, tooltip, chart annotation or
 *                  notification:
 *                  - framing the increase as gain, appreciation, ROI, upside,
 *                    "value grows", "183x", "gets more valuable"
 *                  - urgency or timing pressure: "mint before the price
 *                    rises", "last chance", countdowns to an epoch boundary
 *                    presented as opportunity rather than as the validity
 *                    hazard in III.2(b)
 *                  - presenting RES_MAX_SUPPLY as scarcity marketing. The cap
 *                    is 2^22 RES (4 194 304 RES). Cumulative issuance
 *                    approaches it asymptotically; a low realised supply
 *                    is not a scarcity claim.
 *                  - any projection, chart or figure implying a market price
 *                    for RES. destroy_cost(h) is a protocol cost, not a valuation.
 *
 * 5) End of issuance
 *
 *    RFC-020 has no mint horizon. destroy_cost(h) is never 0 for h >= 0. The wallet
 *    MUST NOT disable the mint entry at an observational calendar boundary
 *    and MUST NOT display a zero, free or infinite price.
 *
 * ---------------------------------------------------------------------------
 * IV. ARCHITECTURE BOUNDARIES                                  [tightened]
 * ---------------------------------------------------------------------------
 *
 * Wallet layer:
 * - Builds valid mint transactions and normal per-asset spends.
 * - MAY compute R exactly, because R is a deterministic function of B and
 *   destroy_cost(h). This is arithmetic, not prediction, and V1's ban on "predicting
 *   RES outcomes" does not apply to it.
 *
 * Consensus layer:
 * - Defines destroy_cost(h) and the pole mint inequality.
 *
 * Wallet MUST NOT:
 * - Hardcode or re-derive destroy_cost(h) by its own formula. Take it from the node.
 * - Treat the price as a step function constant inside a 210 000-block epoch.
 * - Reorder, delay or time transactions to obtain a better price.
 *
 * ---------------------------------------------------------------------------
 * V. NAVIGATION (suggested)                                    [carried from V1]
 * ---------------------------------------------------------------------------
 *
 * Home | Send | Receive | History | Convergence
 *
 * ---------------------------------------------------------------------------
 * End WALLET_DESIGN_LOCK_V2
 */

/** Version tag for includes and review gates. Bump only with a new design doc. */
#define WALLET_DESIGN_LOCK_V2 2

#endif // BITCOIN_WALLET_WALLET_DESIGN_LOCK_V2_H
