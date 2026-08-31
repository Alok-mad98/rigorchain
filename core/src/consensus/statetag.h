// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_STATETAG_H
#define BITCOIN_CONSENSUS_STATETAG_H

#include <consensus/amount.h>
#include <primitives/transaction.h>

#include <cstddef>
#include <cstdint>
#include <vector>

class CCoinsViewCache;

/**
 * RFC-003: StateTag and Asset State Model
 * Version: 1.0 (Canonical Lock)
 * Status: FINAL
 */

namespace Consensus {

// ---------------------------------------------------------------------------
// RFC-003 §3: Asset Domains
// ---------------------------------------------------------------------------

/** ASSET_ID_ORU = 0x01 (PoW energy asset) */
inline constexpr uint8_t ASSET_ID_ORU = 0x01;

/** ASSET_ID_RES = 0x02 (scarce derived asset) */
inline constexpr uint8_t ASSET_ID_RES = 0x02;

static_assert(ASSET_ID_ORU == 0x01, "ASSET_ID_ORU locked to 0x01 per RFC-003");
static_assert(ASSET_ID_RES == 0x02, "ASSET_ID_RES locked to 0x02 per RFC-003");

// ---------------------------------------------------------------------------
// RFC-003 §4.1: StateTag TYPE
// ---------------------------------------------------------------------------

/** StateTag TYPE enumeration per RFC-003 §4.1 */
enum class StateTagType : uint8_t {
    NORMAL = 0x01,   // Normal state
    LOCK = 0x02,     // Locked state
    CONVERGE = 0x03, // Converge state (destroys ORU)
    MINT = 0x04,     // Mint state (RES mint)
    CLAIM = 0x05     // Claim state (RES claim)
};

static_assert(static_cast<uint8_t>(StateTagType::NORMAL) == 0x01, "NORMAL locked to 0x01 per RFC-003");
static_assert(static_cast<uint8_t>(StateTagType::LOCK) == 0x02, "LOCK locked to 0x02 per RFC-003");
static_assert(static_cast<uint8_t>(StateTagType::CONVERGE) == 0x03, "CONVERGE locked to 0x03 per RFC-003");
static_assert(static_cast<uint8_t>(StateTagType::MINT) == 0x04, "MINT locked to 0x04 per RFC-003");
static_assert(static_cast<uint8_t>(StateTagType::CLAIM) == 0x05, "CLAIM locked to 0x05 per RFC-003");

// ---------------------------------------------------------------------------
// RFC-003 §5: Lock Program
// ---------------------------------------------------------------------------

/** Lock program format: RIGOR_PUSH_1 <32-byte program> = 0x51 0x20 <32 bytes> */
inline constexpr size_t LOCK_PROGRAM_SIZE = 34; // 1 (RIGOR_PUSH_1) + 1 (push) + 32 (program)
inline constexpr uint8_t LOCK_PROGRAM_PREFIX = 0x51; // RIGOR_PUSH_1
inline constexpr uint8_t LOCK_PROGRAM_PUSH = 0x20; // Push 32 bytes

static_assert(LOCK_PROGRAM_SIZE == 34, "Lock program size locked to 34 bytes per RFC-003");
static_assert(LOCK_PROGRAM_PREFIX == 0x51, "Lock program prefix locked to RIGOR_PUSH_1 per RFC-003");
static_assert(LOCK_PROGRAM_PUSH == 0x20, "Lock program push locked to 0x20 per RFC-003");

// ---------------------------------------------------------------------------
// RFC-003 §5: StateOutput Structure
// ---------------------------------------------------------------------------

/**
 * StateOutput structure per RFC-003 §5
 * StateOutput = {
 *     asset_type,
 *     amount,
 *     lock_program,
 *     state_tag
 * }
 */
struct StateOutput {
    uint8_t asset_type;                          // ASSET_ID_ORU or ASSET_ID_RES
    CAmount amount;                              // Asset amount
    std::vector<unsigned char> lock_program;     // 32-byte program (34 bytes total with prefix)
    StateTagType state_tag_type;                 // StateTag TYPE
    std::vector<unsigned char> state_tag_params; // StateTag PARAMS (variable)

    StateOutput() : asset_type(0), amount(0), state_tag_type(StateTagType::NORMAL) {}
};

// ---------------------------------------------------------------------------
// RFC-003 §6: Transaction Classification Functions
// ---------------------------------------------------------------------------

/**
 * RFC-003 §6.1: RES Mint Transaction
 * Returns true if transaction is a RES Mint transaction.
 * Full validation requires UTXO set access.
 */
bool IsRESMintTx(const CTransaction& tx, const CCoinsViewCache& inputs);

/**
 * RFC-003 §6.2: RES Spend Transaction
 * Returns true if transaction is a RES Spend transaction.
 * Full validation requires UTXO set access.
 */
bool IsRESSpendTx(const CTransaction& tx, const CCoinsViewCache& inputs);

/**
 * RFC-003 §6.3: ORU Transfer Transaction
 * Returns true if transaction is an ORU Transfer transaction.
 * Full validation requires UTXO set access.
 */
bool IsORUTransferTx(const CTransaction& tx, const CCoinsViewCache& inputs);

// ---------------------------------------------------------------------------
// RFC-003 §7: State Transition Validation
// ---------------------------------------------------------------------------

/**
 * RFC-003 §7.5: Forbidden Transitions
 * Returns true if state transition is valid.
 * Full validation requires UTXO set access.
 */
bool IsValidStateTransition(const CTransaction& tx, const CCoinsViewCache& inputs);

// ---------------------------------------------------------------------------
// RFC-003 §10: Validation Engine - UTXO Access
// ---------------------------------------------------------------------------

/**
 * RFC-003 §10: GetInputAsset
 * Determines the asset type of transaction inputs from UTXO set.
 * RES prevouts dominate (dual-pool ORU fee inputs do not change semantics).
 * Returns 0 when no ORU/RES prevout is present.
 */
uint8_t GetInputAsset(const CTransaction& tx, const CCoinsViewCache& inputs);

// ---------------------------------------------------------------------------
// RFC-003 §9: Witness verification does not live here
// ---------------------------------------------------------------------------

// The mint witness is parsed and verified by `ParseConvergeProof` and
// `Consensus::VerifyConvergeProof` in <consensus/res_proof.h>, which are the only
// definitions of that layout. This header once carried a parallel `MintWitness`
// struct and a same-named `VerifyConvergeProof(tx, MintWitness)` stub that accepted
// any structurally non-empty witness. Both were unreachable, both described a
// layout that A-7 (signature and pubkey removed) and A-8 (bind_hash replaced by the
// parent block hash, target_prev dropped from σ) have since falsified, and the stub
// overloaded the real verifier in the same namespace — so a translation unit that
// included both headers could have resolved a consensus check to the stub. They are
// gone, along with the `ComputeRigorSighash` and `VerifyRigorSignature` stubs that
// served the removed signature. A RIGOR transaction is authenticated solely by its
// own input signatures.

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_STATETAG_H
