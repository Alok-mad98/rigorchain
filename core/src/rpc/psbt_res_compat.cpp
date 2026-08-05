// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/psbt_res_compat.h>

#include <util/bip32.h>
#include <util/strencodings.h>

#include <tinyformat.h>
#include <univalue.h>

namespace rpc {

const RPCResult RES_KEY_PATH_SIG{
    RPCResult::Type::STR_HEX, "res_v1_key_path_sig", /*optional=*/ true,
    "hex-encoded signature for the Witness v1 (RES Script) key path spend"
};

const RPCResult RES_SCRIPT_PATH_SIGS{
    RPCResult::Type::ARR, "res_v1_script_path_sigs", /*optional=*/ true, "",
    {
        {RPCResult::Type::OBJ, "signature", /*optional=*/ true, "The signature for the pubkey and leaf hash combination",
        {
            {RPCResult::Type::STR, "pubkey", "The x-only pubkey for this signature"},
            {RPCResult::Type::STR, "leaf_hash", "The leaf hash for this signature"},
            {RPCResult::Type::STR, "sig", "The signature itself"},
        }},
    }
};

const RPCResult RES_SCRIPTS{
    RPCResult::Type::ARR, "res_v1_scripts", /*optional=*/ true, "",
    {
        {RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::STR_HEX, "script", "A leaf script"},
            {RPCResult::Type::NUM, "leaf_ver", "The version number for the leaf script"},
            {RPCResult::Type::ARR, "control_blocks", "The control blocks for this script",
            {
                {RPCResult::Type::STR_HEX, "control_block", "A hex-encoded control block for this script"},
            }},
        }},
    }
};

const RPCResult RES_BIP32_DERIVS{
    RPCResult::Type::ARR, "res_v1_bip32_derivs", /*optional=*/ true, "",
    {
        {RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::STR, "pubkey", "The x-only public key this path corresponds to"},
            {RPCResult::Type::STR, "master_fingerprint", "The fingerprint of the master key"},
            {RPCResult::Type::STR, "path", "The path"},
            {RPCResult::Type::ARR, "leaf_hashes", "The hashes of the leaves this pubkey appears in",
            {
                {RPCResult::Type::STR_HEX, "hash", "The hash of a leaf this pubkey appears in"},
            }},
        }},
    }
};

const RPCResult RES_INTERNAL_KEY{
    RPCResult::Type::STR_HEX, "res_v1_internal_key", /*optional=*/ true,
    "The hex-encoded Witness v1 (RES Script) x-only internal key"
};

const RPCResult RES_MERKLE_ROOT{
    RPCResult::Type::STR_HEX, "res_v1_merkle_root", /*optional=*/ true,
    "The hex-encoded Witness v1 (RES Script) merkle root"
};

const RPCResult RES_TREE{
    RPCResult::Type::ARR, "res_v1_tree", /*optional=*/ true,
    "The tuples that make up the Witness v1 (RES Script) tree, in depth first search order",
    {
        {RPCResult::Type::OBJ, "tuple", /*optional=*/ true, "A single leaf script in the Witness v1 (RES Script) tree",
        {
            {RPCResult::Type::NUM, "depth", "The depth of this element in the tree"},
            {RPCResult::Type::NUM, "leaf_ver", "The version of this leaf"},
            {RPCResult::Type::STR, "script", "The hex-encoded script itself"},
        }},
    }
};

void AppendResInputFields(const PSBTInput& input, UniValue& in)
{
    if (!input.m_tap_key_sig.empty()) {
        in.pushKV("res_v1_key_path_sig", HexStr(input.m_tap_key_sig));
    }

    if (!input.m_tap_script_sigs.empty()) {
        UniValue script_sigs(UniValue::VARR);
        for (const auto& [pubkey_leaf, sig] : input.m_tap_script_sigs) {
            const auto& [xonly, leaf_hash] = pubkey_leaf;
            UniValue sigobj(UniValue::VOBJ);
            sigobj.pushKV("pubkey", HexStr(xonly));
            sigobj.pushKV("leaf_hash", HexStr(leaf_hash));
            sigobj.pushKV("sig", HexStr(sig));
            script_sigs.push_back(sigobj);
        }
        in.pushKV("res_v1_script_path_sigs", script_sigs);
    }

    if (!input.m_tap_scripts.empty()) {
        UniValue tap_scripts(UniValue::VARR);
        for (const auto& [leaf, control_blocks] : input.m_tap_scripts) {
            const auto& [script, leaf_ver] = leaf;
            UniValue script_info(UniValue::VOBJ);
            script_info.pushKV("script", HexStr(script));
            script_info.pushKV("leaf_ver", leaf_ver);
            UniValue control_blocks_univ(UniValue::VARR);
            for (const auto& control_block : control_blocks) {
                control_blocks_univ.push_back(HexStr(control_block));
            }
            script_info.pushKV("control_blocks", control_blocks_univ);
            tap_scripts.push_back(script_info);
        }
        in.pushKV("res_v1_scripts", tap_scripts);
    }

    if (!input.m_tap_bip32_paths.empty()) {
        UniValue keypaths(UniValue::VARR);
        for (const auto& [xonly, leaf_origin] : input.m_tap_bip32_paths) {
            const auto& [leaf_hashes, origin] = leaf_origin;
            UniValue path_obj(UniValue::VOBJ);
            path_obj.pushKV("pubkey", HexStr(xonly));
            path_obj.pushKV("master_fingerprint", strprintf("%08x", ReadBE32(origin.fingerprint)));
            path_obj.pushKV("path", WriteHDKeypath(origin.path));
            UniValue leaf_hashes_arr(UniValue::VARR);
            for (const auto& leaf_hash : leaf_hashes) {
                leaf_hashes_arr.push_back(HexStr(leaf_hash));
            }
            path_obj.pushKV("leaf_hashes", leaf_hashes_arr);
            keypaths.push_back(path_obj);
        }
        in.pushKV("res_v1_bip32_derivs", keypaths);
    }

    if (!input.m_tap_internal_key.IsNull()) {
        in.pushKV("res_v1_internal_key", HexStr(input.m_tap_internal_key));
    }

    if (!input.m_tap_merkle_root.IsNull()) {
        in.pushKV("res_v1_merkle_root", HexStr(input.m_tap_merkle_root));
    }
}

void AppendResOutputFields(const PSBTOutput& output, UniValue& out)
{
    if (!output.m_tap_internal_key.IsNull()) {
        out.pushKV("res_v1_internal_key", HexStr(output.m_tap_internal_key));
    }

    if (!output.m_tap_tree.empty()) {
        UniValue tree(UniValue::VARR);
        for (const auto& [depth, leaf_ver, script] : output.m_tap_tree) {
            UniValue elem(UniValue::VOBJ);
            elem.pushKV("depth", (int)depth);
            elem.pushKV("leaf_ver", (int)leaf_ver);
            elem.pushKV("script", HexStr(script));
            tree.push_back(elem);
        }
        out.pushKV("res_v1_tree", tree);
    }

    if (!output.m_tap_bip32_paths.empty()) {
        UniValue keypaths(UniValue::VARR);
        for (const auto& [xonly, leaf_origin] : output.m_tap_bip32_paths) {
            const auto& [leaf_hashes, origin] = leaf_origin;
            UniValue path_obj(UniValue::VOBJ);
            path_obj.pushKV("pubkey", HexStr(xonly));
            path_obj.pushKV("master_fingerprint", strprintf("%08x", ReadBE32(origin.fingerprint)));
            path_obj.pushKV("path", WriteHDKeypath(origin.path));
            UniValue leaf_hashes_arr(UniValue::VARR);
            for (const auto& leaf_hash : leaf_hashes) {
                leaf_hashes_arr.push_back(HexStr(leaf_hash));
            }
            path_obj.pushKV("leaf_hashes", leaf_hashes_arr);
            keypaths.push_back(path_obj);
        }
        out.pushKV("res_v1_bip32_derivs", keypaths);
    }
}

} // namespace rpc
