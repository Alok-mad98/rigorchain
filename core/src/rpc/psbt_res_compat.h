// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPC_PSBT_RES_COMPAT_H
#define BITCOIN_RPC_PSBT_RES_COMPAT_H

#include <psbt.h>
#include <rpc/util.h>

class UniValue;

namespace rpc {

// Bitcoin BIP174 compatibility adapter:
// internal PSBT storage keeps m_tap_* fields for wire compatibility, while RPC
// surface exposes Rigorchain semantic names (res_v1_*).
void AppendResInputFields(const PSBTInput& input, UniValue& in);
void AppendResOutputFields(const PSBTOutput& output, UniValue& out);

// RES-v1 RPC schema fragments for decodepsbt.
// RIGORCHAIN: single source of semantic documentation for res_v1_* keys.

// RIGORCHAIN: The key-path signature field for Witness v1 RES PSBT inputs.
// Replaces Bitcoin Core's "taproot_key_path_sig" in RPC output semantics.
extern const RPCResult RES_KEY_PATH_SIG;
// RIGORCHAIN: Script-path signature set for Witness v1 RES PSBT inputs.
// Replaces Bitcoin Core's "taproot_script_path_sigs" in RPC output semantics.
extern const RPCResult RES_SCRIPT_PATH_SIGS;
// RIGORCHAIN: Leaf script/control-block set for Witness v1 RES PSBT inputs.
// Replaces Bitcoin Core's "taproot_scripts" in RPC output semantics.
extern const RPCResult RES_SCRIPTS;
// RIGORCHAIN: BIP32 derivation data for Witness v1 RES x-only pubkeys.
// Replaces Bitcoin Core's "taproot_bip32_derivs" in RPC output semantics.
extern const RPCResult RES_BIP32_DERIVS;
// RIGORCHAIN: Internal x-only key for Witness v1 RES metadata.
// Replaces Bitcoin Core's "taproot_internal_key" in RPC output semantics.
extern const RPCResult RES_INTERNAL_KEY;
// RIGORCHAIN: Merkle root metadata for Witness v1 RES script tree.
// Replaces Bitcoin Core's "taproot_merkle_root" in RPC output semantics.
extern const RPCResult RES_MERKLE_ROOT;
// RIGORCHAIN: Script tree tuples for Witness v1 RES PSBT outputs.
// Replaces Bitcoin Core's "taproot_tree" in RPC output semantics.
extern const RPCResult RES_TREE;

} // namespace rpc

#endif // BITCOIN_RPC_PSBT_RES_COMPAT_H
