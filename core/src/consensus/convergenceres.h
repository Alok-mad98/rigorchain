// Convergence / Res consensus (see consensus/convergence_logic.h): witness v1 **CVG2** convergence outputs.
// `IsConvergenceV1ScriptPubKey` is the CVG2 detector (alias of `IsConvergenceScript`). Not Bitcoin Taproot spending rules.

#ifndef BITCOIN_CONSENSUS_CONVERGENCERES_H
#define BITCOIN_CONSENSUS_CONVERGENCERES_H

/**
 * RFC-000 §13: V1 CONTAINER LAW
 * ScriptPubKey = OP_1 <32 bytes>
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
 *   - Coinbase CVGE v2: `m_draw_candidates.root` must equal `ComputeConvergenceDrawCandidateRoot` for the same block body
 *     (ConnectBlock / ContextualCheckBlock: `bad-convergence-draw-candidate-root`); cumulative convergence must match sum of convergence amounts.
 *   - New Res issuance is validated only on **non-coinbase** transactions via `rigor_res.cpp` + `res_proof.cpp`
 *     (convergence txid, bind hash, height, parent compact target, witness proof). Coinbase is never an Res mint program source.
 *   - Draw eligibility for convergence txs is fully recomputable from block txs + prior headers (bind/target);
 *     not from mempool ordering, coinbase txid, or `hashMerkleRoot`.
 *   - Draw eligibility RHS is the immutable RFC-014 constant
 *     `Consensus::Params::RES_PROBABILITY_TARGET` (= floor(2^246 / MIN_CONVERGENCE_AMOUNT) / CONVERGENCE_DRAW_WINDOW; RFC-017, C=1024 ORU); see
 *     `ConvergenceDrawInequalityHolds`. Parent compact `nBits` is retained on
 *     the σ side ONLY for Sigma-CR-01 replay-domain separation.
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
 * Default OP_CONVERGE burn used by tests: HASH160 of 32 0xa1 bytes.
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
 * and — because OP_CONVERGE is the only convergence form — RFC-002 C(t) as well.
 *
 * LOAD-BEARING that those two readings coincide: an OP_CONVERGE output never enters the UTXO set
 * (`AddCoins` skips it), so its value is destroyed the moment the block connects, which is
 * what makes it legitimate to subtract from S_ORU and to commit through the coinbase CVGE
 * payload (`bad-convergence-cumulative`, via `CBlockIndex::GetChainConvergeSum`) and
 * `state_hash`.
 *
 * A convergence form that stayed in the UTXO set would break that identity and would have to
 * be split back into two functions: counting a live UTXO as destroyed ORU subtracts value the
 * chainstate still holds and that `other_balance` still reports, and then commits the
 * discrepancy — a chain split. The retired OP_CVG_CLAIM ticket and the retired CVG2+RESO
 * sibling declaration were such forms; see script.h on 0xbc and OP_CONVERGE.
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
