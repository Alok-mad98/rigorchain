// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_RES_KEYPAIR_H
#define BITCOIN_WALLET_RES_KEYPAIR_H

#include <crypto/ed25519_provider.h>

#include <array>
#include <cstddef>
#include <vector>

// This helper used to live in `consensus/res_proof.{h,cpp}`,
// where it sat next to consensus code while internally calling
// `GetStrongRandBytes`. That co-location is a latent replay hazard: any
// future copy-paste or accidental call from a consensus path would pull
// non-deterministic randomness into chain state. The function itself is
// dead code today (no production caller exists), so we relocate it into
// the wallet translation unit, which is the only architectural layer
// where local randomness is permitted. The namespace `Consensus::` is
// intentionally preserved so the symbol remains link-compatible for any
// out-of-tree consumer that linked it historically; the relocation is a
// pure directory / Makefile-source-list change. See
// `core/doc/replay-determinism-checklist.md` items D-2 and D-3 for the
// owning rule.

namespace Consensus {

/**
 * Generate a fresh Ed25519 keypair suitable for use as a RES claim key.
 *
 * NOT FOR CONSENSUS PATHS. This function calls `GetStrongRandBytes`,
 * which is intrinsically non-deterministic. Calling it inside any code
 * path that contributes to block validation, replay, or chainstate
 * snapshot computation is a replay-equivalence violation.
 *
 * @param[out] program32_out  The 32-byte Ed25519 public key.
 * @param[out] secret_out     The 64-byte expanded Ed25519 secret key.
 * @return true on success.
 */
bool GenerateResKeypair(std::vector<unsigned char>& program32_out,
                        std::array<unsigned char, ed25519::SECRETKEY_SIZE>& secret_out);

} // namespace Consensus

#endif // BITCOIN_WALLET_RES_KEYPAIR_H
