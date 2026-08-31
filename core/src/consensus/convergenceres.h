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
 * CONSENSUS_LOCK_CONVERGENCE_V3 — Convergence v3.0 Consensus Lock (Res mint hardening layer)
 *
 * Res mint preimage inputs MUST NOT depend on:
 *   - the coinbase transaction (as a whole or truncated),
 *   - any coinbase txid (including "orum-only" or other modified projections),
 *   - coinbase scriptSig,
 *   - block hashMerkleRoot (header transaction merkle root).
 *
 * Valid consensus Res mint inputs are strictly limited to what `rigor_res.cpp` and
 * `res_proof.cpp` enforce: convergence transaction txid, bind_hash, block context
 * (height, parent compact target), and explicit witness proof pushes — never
 * coinbase-derived material.
 *
 * Normative rules (see also convergence_logic / convergence_commitment / ConnectBlock):
 *   - Structure: at most ONE positive-value convergence output per ORU-classified tx (CountConvergenceOutputs(tx) <= 1); more than one is invalid-convergence-structure. Ordinary ORU transfers may carry zero convergence outputs.
 *   - Draw inequality: H < T · f (RFC-000 §3) where f is the integer convergence amount, untransformed (RFC-000 §7).
 *   - Convergence spends ORU to convergence outputs only: no consensus ORU fee floor on convergence txs (standard relay/min-fee policy only).
 *   - Minimum ORU convergenced per convergence tx is fixed MIN_CONVERGENCE_AMOUNT (1 ORU unit at COIN scale); no height/halving ladder.
 *   - The block's ONLY convergence commitment is the coinbase scriptSig: `<BIP34 height> <OP_PUSHBYTES_32 C> <OP_PUSHBYTES_32 paid-winner-txid>`,
 *     byte-exact with no trailing data (`consensus/convergence_root.cpp`; `bad-cb-convergence-root`).
 *     There is no coinbase CVGE `RIGOR_MARKER_RETURN` check: `bad-convergence-draw-candidate-root` and
 *     `bad-convergence-cumulative` no longer exist. `CBlockIndex::m_draw_candidate_root` is a local
 *     recomputation for RPC / state_snapshot, never compared against producer-supplied data.
 *   - New Res issuance exists ONLY in the **coinbase**, as a single `RIGOR_MARKER_RES_MINT` output evaluated by
 *     `rigor::ConnectEvaluateCoinbaseResMint` (consensus/rigor_res.cpp). `Consensus::CheckTxInputs`
 *     rejects every body transaction that creates RES (`mixed-asset-output`), so the coinbase is the
 *     single issuance gate. There is no body self-proof mint, no PCORB carrier, and no covenant mint.
 *   - Draw eligibility for convergence txs is fully recomputable from block txs + prior headers (bind/target);
 *     not from mempool ordering, coinbase txid, or `hashMerkleRoot`.
 *   - Draw eligibility RHS is the immutable RFC-014 constant
 *     `Consensus::Params::RES_PROBABILITY_TARGET` (= floor(2^246 / MIN_CONVERGENCE_AMOUNT) / CONVERGENCE_DRAW_WINDOW; RFC-017, C=1024 ORU); see
 *     `ConvergenceDrawInequalityHolds`. Parent compact `nBits` is not a σ input
 *     (SECURITY-001-A8 / Sigma-PoW-01). Historical Sigma-CR-01 `bind_commit` is superseded.
 */
#define CONSENSUS_LOCK_CONVERGENCE_V3 1

#include <arith_uint256.h>
#include <consensus/res_amount.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <uint256.h>

#include <cstdint>
#include <optional>

class CBlock;
class CBlockIndex;

namespace Consensus {
struct Params;

// SECURITY-001-A8 removed `CONVERGENCE_DRAW_BIND_DEPTH` (k = 10) and the RFC-000 §5 bootstrap
// preimage that went with it. σ now binds to the parent block hash, which is defined at every
// height from 1 up, so there is no ancestor depth to configure and no bootstrap gap to fill.

/** Heuristic window (in blocks) for Res emission / convergence RPC progress displays (~1 Res mint per this many blocks on average). */
inline constexpr int RES_TARGET_INTERVAL = 6;

/** Res draw: σ = SelectionDigest(txid,C,B_{h-1},h); valid iff σ < T·f (RFC-000 §3, see convergence_logic). */
inline constexpr unsigned int RES_MINT_TARGET_BITS = 14;

/** Returns the convergence amount as an untransformed integer weight f (RFC-000 §7). */
arith_uint256 PowConvergenceAmount(CAmount amount);

/** Truncated uint64 view of f (tests / diagnostics). */
uint64_t ConvergenceDrawEffectiveWeightU64(CAmount convergence_amount);

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
 * Value committed to the convergence draw by this transaction. Feeds
 * `ConvergenceDrawCandidate::weight`, **F** membership, the `MIN_CONVERGENCE_AMOUNT` floor,
 * and — because RIGOR_MARKER_CONVERGE is the only convergence form — RFC-002 C(t) as well.
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

CAmount GetOutputValueByAsset(const CTransaction& tx, AssetType asset);

/** Target from compact bits representation (genesis default only). */
arith_uint256 ResMintTargetArith(uint8_t bits);

/** Fixed draw target T from Params::res_mint_target_bits (same at every height). */
uint256 InitialResDrawTarget(const Params& consensusParams);

/**
 * Draw target T for validating a block at height H: protocol constant from `InitialResDrawTarget`.
 */
uint256 ResDrawTargetForBlock(const CBlockIndex* pindex_prev, const Params& consensusParams);

/** Easiest allowed draw target (256-bit max) for tests / bounds. */
uint256 MaxResDrawTarget();

CAmount ComputeResReward(const CBlock& block, const CBlockIndex* pindex_prev, int nHeight, const Params& consensusParams);

/**
 * @deprecated Dead API — coinbase-derived Res mint program generation is forbidden.
 * These symbols remain only so mistaken link-time references fail loudly at runtime.
 * Consensus Res mint validation uses `rigor_res.cpp` + `res_proof.cpp` only.
 */
[[deprecated("Consensus forbids coinbase-derived Res mint; use witness proof path (rigor_res / res_proof)")]]
uint256 GetOruOnlyCoinbaseTxId(const CTransaction& coinbase_tx);
[[deprecated("Consensus forbids coinbase-derived Res mint; use witness proof path (rigor_res / res_proof)")]]
uint256 ResMintWitnessProgram(const uint256& oru_only_coinbase_txid, const uint256& parent_block_hash);
[[deprecated("Consensus forbids coinbase-derived Res mint; use witness proof path (rigor_res / res_proof)")]]
CScript ResMintScriptPubKey(const uint256& oru_only_coinbase_txid, const uint256& parent_block_hash);
[[deprecated("Consensus forbids coinbase-derived Res mint; use witness proof path (rigor_res / res_proof)")]]
bool IsValidResMintOutput(const CTxOut& out, const uint256& oru_only_cb_txid, const uint256& parent_block_hash);

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_CONVERGENCERES_H
