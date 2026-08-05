// Copyright (c) The RigorChain / RingCore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_WALLET_DESIGN_LOCK_V1_H
#define BITCOIN_WALLET_WALLET_DESIGN_LOCK_V1_H

/**
 * @file  WALLET_DESIGN_LOCK_V1.h
 *
 * TAG: WALLET_DESIGN_LOCK_V1
 *
 * RigorChain wallet (GUI + backend) design contract. Do not relax these rules
 * without a new version tag and explicit review.
 *
 * IMPORTANT (read before changing wallet, RPC, or Qt):
 *
 * - RigorChain is a Dual-Native Asset system (ORU and RES are both L1 native).
 * - The wallet MUST NOT mix ORU and RES in presentation, selection, or a single
 *   user-facing send operation.
 * - Convergence is a probabilistic competition mechanism, not a guaranteed return.
 * - Consensus / draw policy MUST NOT leak into wallet prediction logic.
 * - Addresses are asset-agnostic (Bech32 `rig1...`); asset type is NOT encoded
 *   in the address string.
 *
 * Wallet responsibilities:
 * - Clearly label operations and balances as ORU or RES.
 * - Provide entry points: Send, Receive, History, Convergence (as specified).
 * - Never explain exact win probability; never merge or infer asset types.
 *
 * ---------------------------------------------------------------------------
 * I. NON-NEGOTIABLE PRINCIPLES (English — keep in sync with product docs)
 * ---------------------------------------------------------------------------
 *
 * 1) Single address format, multiple assets (never conflate)
 *    - All user-facing addresses: Bech32 `rig1...`.
 *    - Address ≠ asset type. The same address string does not imply a single
 *      asset; UTXOs carry asset tags at the protocol level.
 *    - UI MUST always distinguish ORU vs RES. Every send/receive/display path
 *      MUST name the asset explicitly.
 *
 *    FORBIDDEN:
 *    - Auto-inferring asset type from context or “default”.
 *    - Showing a single “Total Balance” that adds ORU and RES.
 *    - One user transaction that mixes ORU inputs/outputs with RES inputs/outputs
 *      in the same tx (except coinbase / consensus-defined exceptions, not
 *      wallet-constructed normal sends).
 *
 * 2) Dual-native assets are peers, not “token” hierarchy
 *    - ORU and RES are both L1 native. There is no “primary coin / secondary
 *      token” framing in UI.
 *    - FORBIDDEN in UI copy: “Token”, “Secondary”, “wrapped”, “ERC-20-like”.
 *
 *    REQUIRED balance presentation (example structure):
 *      ORU:
 *        Available:  …
 *        Pending:    …
 *      RES:
 *        Available:  …
 *        Pending:    …
 *
 *    FORBIDDEN:
 *      “Total Balance: …” (single number mixing assets)
 *
 * 3) UTXO model stays pure
 *    - Wallet remains UTXO-based. Do not introduce an account-based abstraction
 *      that hides inputs/outputs from advanced users.
 *    - Advanced mode MAY expose coin control / per-UTXO detail; do not pretend
 *      accounts are the truth layer.
 *
 * 4) Convergence is competition, not a "feature yield"
 *    - Convergence = spend ORU to participate in RES draw competition (PoW-like
 *      probabilistic framing, not a savings APR).
 *    - UI MUST NOT show: “guaranteed yield”, “APR”, “interest rate”, “fixed
 *      return”.
 *
 * ---------------------------------------------------------------------------
 * II. TRANSFER (SEND) UI — highest risk of user error
 * ---------------------------------------------------------------------------
 *
 * Required layout (conceptual):
 *   [ Send ]
 *   Asset:  (•) ORU   ( ) RES     <- explicit, mandatory selection
 *   Address: rig1...
 *   Amount:  ___
 *
 * FORBIDDEN:
 * - Choosing asset from “which balance is larger” or any automatic rule.
 * - Mixed inputs from different assets in one wallet-constructed send.
 *
 * Underlying construction rules (wallet + RPC):
 * - Selected inputs MUST be single-asset for that send.
 * - Outputs for that send MUST be the same asset (change + recipient).
 * - Network fee MUST be paid in ORU (POOL-F); never inferred from transfer asset.
 *
 * ---------------------------------------------------------------------------
 * III. CONVERGENCE — wallet expression (correct abstraction)
 * ---------------------------------------------------------------------------
 *
 * Correct mental model: Convergence = consume ORU to compete for RES (draw).
 * NOT: "convert ORU to RES" or "exchange" or "swap".
 *
 * Recommended copy direction:
 * - “Use ORU to compete for RES”
 * FORBIDDEN copy:
 * - “Convert ORU to RES”, “Exchange”, “Swap”, “guaranteed RES”.
 *
 * GUI MUST show:
 * (1) Cost: convergence_amount in ORU (and Minimum Convergence (current): X ORU).
 * (2) Scale / participation: “participation size” / “relative weight” — NOT a
 *     displayed numeric win probability.
 * (3) Outcome: e.g. Won RES / Not selected (per consensus rules), not wallet
 *     prediction.
 *
 * Suggested screens:
 *   [ Convergence ]
 *   Create Convergence:
 *     Convergence Amount: [____] ORU
 *     [ Start Convergence ]
 *   Your Convergence History:
 *     TxID | Amount | Status | Result
 *
 * ---------------------------------------------------------------------------
 * IV. ARCHITECTURE BOUNDARIES
 * ---------------------------------------------------------------------------
 *
 * Wallet layer:
 * - Build valid Convergence transactions and normal spends per asset.
 * Consensus layer:
 * - Decides draw outcome and inclusion rules.
 * Wallet MUST NOT:
 * - Predict RES outcomes, show P(win) as a precise number, or reorder txs
 *   to “improve luck”.
 *
 * ---------------------------------------------------------------------------
 * V. NAVIGATION (suggested)
 * ---------------------------------------------------------------------------
 *
 * Home | Send | Receive | History | Convergence
 *
 * ---------------------------------------------------------------------------
 * End WALLET_DESIGN_LOCK_V1
 */

/** Version tag for includes and review gates. Bump only with a new design doc. */
#define WALLET_DESIGN_LOCK_V1 1

#endif // BITCOIN_WALLET_WALLET_DESIGN_LOCK_V1_H
