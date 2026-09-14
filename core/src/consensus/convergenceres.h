// Convergence / Res consensus (see consensus/convergence_logic.h): **RIGOR_MARKER_CONVERGE** burn outputs.
// A convergence output is `<20-byte HASH160(pk32)> RIGOR_MARKER_CONVERGE` (22 bytes, script/predicate_verify.cpp
// ParseConvergeScript). Under Option B, HASH160(pk32) equals HASH160(v1 program).
// `IsConvergenceScript` detects that lock. The retired witness v1 CVG2 program is NOT a convergence
// output and is rejected outright by `IsAllowedScript` (consensus/tx_check.cpp).

#ifndef BITCOIN_CONSENSUS_CONVERGENCERES_H
#define BITCOIN_CONSENSUS_CONVERGENCERES_H

/**
 * RFC-000 §13: V1 CONTAINER LAW
 * ScriptPubKey = RIGOR_PUSH_1 <32 bytes>
 * semantic = f(input_asset_domain)
 * 
 * CONSENSUS_LOCK_CONVERGENCE_V4 — Convergence v4.0 Consensus Lock (RFC-019 RATE(k))
 *
 * A convergence output destroys ORU. Under RFC-017 that bought a lottery ticket and the
 * prize was paid by the next block's coinbase; under RFC-019 it buys RES outright, in the
 * same transaction, at the epoch's destroy cost. The burn is unchanged — what it buys is not.
 *
 * Normative rules (see also ConnectBlock / consensus/res_issuance.h):
 *   - Structure: at most ONE positive-value convergence output per ORU-classified tx
 *     (CountConvergenceOutputs(tx) <= 1); more than one is invalid-convergence-structure.
 *     Ordinary ORU transfers may carry zero convergence outputs.
 *   - Convergence spends ORU to convergence outputs only: no consensus ORU fee floor on
 *     convergence txs (standard relay/min-fee policy only).
 *   - Smallest OP_CONVERGE burn is one ORU atom (nValue > 0). There is no
 *     1 ORU ticket floor. Minting R ≥ 1 is gated only by
 *     `CheckResMintArithmetic`: the R = 1 case is B ≥ ceil((2^25+h)^2 / 2^40).
 *   - New RES issuance exists ONLY in the transaction body, priced by `Consensus::CheckTxInputs`
 *     against `CheckResMintArithmetic` (consensus/res_issuance.h): a tx with no RES inputs may
 *     create RES iff CheckResMintArithmetic admits (R, B, h).
 *     There is no coinbase mint, no witness self-proof, no PCORB carrier, and no covenant mint.
 *   - The burn is DECLARED by the convergence output, not inferred from inputs minus outputs
 *     minus fee, so the amount minted does not move with fee estimation and burn and fee stay
 *     separable.
 *   - The coinbase commits nothing about issuance. Its scriptSig is the BIP34 height push alone.
 *   - The cap is the closed-form integral (S/P0)·H = 2^22 RES. There is no horizon.
 */
#define CONSENSUS_LOCK_CONVERGENCE_V4 1

#include <consensus/res_amount.h>
#include <primitives/transaction.h>
#include <script/script.h>

#include <cstdint>

class CBlock;

namespace Consensus {

/**
 * Default RIGOR_MARKER_CONVERGE burn used by tests: HASH160 of 32 0xa1 bytes.
 * Wallet construction always supplies a live RES program hash via GetConvergeScript.
 * `IsConvergenceOutputScript` is an alias of `IsConvergenceScript`.
 */
CScript ConvergenceOutputScript(const uint160& owner_hash160);
const CScript& ConvergenceOutputScript();
bool IsConvergenceOutputScript(const CScript& script);

/** Number of non-coinbase transactions in this block that include at least one convergence output (same-block scope only). */
int CountConvergenceTransactions(const CBlock& block);

bool IsConvergenceTx(const CTransaction& tx);
int CountConvergenceOutputs(const CTransaction& tx);

/**
 * Value of the OP_CONVERGE burn. Feeds RFC-002 C(t) and, when the same
 * transaction mints RES, CheckResMintArithmetic. The lottery-era F /
 * `MIN_CONVERGENCE_AMOUNT` ticket floor is retired; live consensus only
 * requires a positive burn.
 *
 * LOAD-BEARING that those two readings coincide: an RIGOR_MARKER_CONVERGE output never enters the UTXO set
 * (`AddCoins` skips it), so its value is destroyed the moment the block connects, which is
 * what makes it legitimate to subtract from S_ORU and to report through the NON-CONSENSUS
 * `CBlockIndex::m_block_converge_sum` / `GetChainConvergeSum()` ledgers and `state_hash`.
 * Those ledgers are observational: no ConnectBlock predicate reads them.
 *
 * A convergence form that stayed in the UTXO set would break that identity and would have to
 * be split back into two functions: counting a live UTXO as destroyed ORU subtracts value the
 * chainstate still holds and that `other_balance` still reports, and then commits the
 * discrepancy — a chain split. The retired OP_CVG_CLAIM ticket and the retired CVG2+RESO
 * sibling declaration were such forms; see script.h on 0xbc and RIGOR_MARKER_CONVERGE.
 */
CAmount GetConvergenceAmount(const CTransaction& tx);

/**
 * `GetConvergenceAmount` clamped to unsigned, for the observational prefix sums that accumulate
 * destroyed ORU into a uint64_t. A non-convergence transaction and a malformed one both
 * contribute nothing rather than wrapping.
 */
inline uint64_t GetTransactionConvergenceAmount(const CTransaction& tx)
{
    const CAmount amount = GetConvergenceAmount(tx);
    if (amount <= 0) return 0;
    return static_cast<uint64_t>(amount);
}

CAmount GetOutputValueByAsset(const CTransaction& tx, AssetType asset);

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_CONVERGENCERES_H
