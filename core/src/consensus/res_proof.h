#ifndef BITCOIN_CONSENSUS_RES_PROOF_H
#define BITCOIN_CONSENSUS_RES_PROOF_H

#include <primitives/transaction.h>
#include <script/rigor_tags.h>
#include <script/script.h>
#include <uint256.h>

#include <cstddef>
#include <vector>

class CPubKey;

namespace Consensus {

/**
 * PCORB (tagged coinbase RIGOR_MARKER_RETURN) is a retired mint carrier. Every such
 * output is consensus-invalid (`bad-pcorb-carrier`). These predicates exist so
 * `CheckTransaction` / `CheckTxInputs` can reject the shape; they do not decode
 * a proof and they do not authorize RES. Historical field layout and the
 * retired parser live in `test/util/pcorb_carrier.h` (TASK-010 C-3 / R-5).
 */

/** True iff `script` is the canonical tagged PCORB RIGOR_MARKER_RETURN. Value is checked on the txout. */
bool IsPcorbCarrierScript(const CScript& script);
/** True iff nValue == 0 and the script is `IsPcorbCarrierScript`. */
bool IsPcorbCarrierOutput(const CTxOut& out);
/** How many vouts are `IsPcorbCarrierOutput`. Consensus rejects every carrier (`bad-pcorb-carrier`). */
size_t CountPcorbCarrierOutputs(const CTransaction& tx);

static_assert(RigorTags::PCORB_PROOF_FIELDS_BYTES == 32 + 8 + 32 + 8 + 4 + 32, "PCORB field concat");
static_assert(RigorTags::PCORB_OPRETURN_PAYLOAD_BYTES == 4 + RigorTags::PCORB_PROOF_FIELDS_BYTES, "PCOR ‖ fields");
static_assert(RigorTags::PCORB_CARRIER_SCRIPT_BYTES == 1 + 2 + RigorTags::PCORB_OPRETURN_PAYLOAD_BYTES, "RIGOR_MARKER_RETURN PUSHDATA1 120");

// TASK-H2 R-8: `GenerateResKeypair` has been relocated to
// `wallet/res_keypair.h`. The function used strong randomness, which // consensus-discipline-allow
// must never appear in any consensus-adjacent translation unit. If you
// need it, include `<wallet/res_keypair.h>` from a non-consensus path.
bool DeriveResWitnessProgramFromPubKey(const CPubKey& pubkey, std::vector<unsigned char>& program32_out);

} // namespace Consensus

#endif
