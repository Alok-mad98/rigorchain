#!/usr/bin/env python3
# Copyright (c) 2019-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Useful Script constants and utils."""
from test_framework.script import (
    CScript,
    CScriptOp,
    RIGOR_PUSH_0,
    RIGOR_PUSH_1,
    RIGOR_RESERVED_LEGACY_CHECKMULTISIG,
    RIGOR_RESERVED_LEGACY_CHECKSIG,
    RIGOR_RESERVED_LEGACY_DUP,
    RIGOR_RESERVED_LEGACY_EQUAL,
    RIGOR_RESERVED_LEGACY_EQUALVERIFY,
    RIGOR_RESERVED_LEGACY_HASH160,
    RIGOR_MARKER_RETURN,
    RIGOR_MARKER_RES_MINT,
    RIGOR_MARKER_CONVERGE,
    hash160,
    sha256,
)

# To prevent a "tx-size-small" policy rule error, a transaction has to have a
# non-witness size of at least 65 bytes (MIN_STANDARD_TX_NONWITNESS_SIZE in
# src/policy/policy.h). Considering a Tx with the smallest possible single
# input (blank, empty scriptSig), and with an output with empty scriptPubKey,
# we get to a minimum size of 60 bytes (Bitcoin-style CTxOut: nValue + script only):
#
# Tx Skeleton: 4 [Version] + 1 [InCount] + 1 [OutCount] + 4 [LockTime] = 10 bytes
# Blank Input: 32 [PrevTxHash] + 4 [Index] + 1 [scriptSigLen] + 4 [SeqNo] = 41 bytes
# Output:      8 [Amount] + 1 [scriptPubKeyLen] = 9 bytes (empty scriptPubKey)
#
# Hence, the scriptPubKey of the single output has to have a size of at
# least 5 bytes.
MIN_STANDARD_TX_NONWITNESS_SIZE = 65
MIN_PADDING = MIN_STANDARD_TX_NONWITNESS_SIZE - 10 - 41 - 9
assert MIN_PADDING == 5


ASSET_NONE = 2
ASSET_INVALID = 4


def asset_type_from_scriptpubkey(script_pub_key):
    """Infer native asset domain from scriptPubKey (matches consensus AssetTypeFromScriptPubKey).

    Uses CScript.IsWitnessProgram (same shape as C++). Witness v1 is RIGOR_PUSH_1 (0x51), not raw byte 1.
    Unrecognized scripts are ASSET_INVALID (fail-closed), never ORU.
    """
    from test_framework.messages import ASSET_ORU, ASSET_RES, ASSET_NONE, ASSET_INVALID

    s = script_pub_key if isinstance(script_pub_key, CScript) else CScript(script_pub_key)
    raw = bytes(s)
    if len(raw) >= 1 and raw[0] == RIGOR_MARKER_RETURN:
        return ASSET_NONE
    if len(raw) == 22 and raw[0] == 20 and raw[21] == int(RIGOR_MARKER_CONVERGE):
        return ASSET_ORU
    try:
        items = list(s)
    except Exception:
        items = None
    if items is not None and len(items) == 3 and items[2] == RIGOR_MARKER_RES_MINT:
        pkh = items[1]
        if isinstance(pkh, (bytes, bytearray)) and len(pkh) == 20:
            rebuilt = CScript([items[0], bytes(pkh), RIGOR_MARKER_RES_MINT])
            if bytes(rebuilt) == raw:
                return ASSET_RES
    if not s.IsWitnessProgram():
        return ASSET_INVALID
    if s[0] == RIGOR_PUSH_0:
        program = s[2:]
        return ASSET_ORU if len(program) == 20 else ASSET_INVALID
    if s[0] == RIGOR_PUSH_1:
        program = s[2:]
        if len(program) == 32 and program[:4] not in (b"CVG2", b"RES2"):
            return ASSET_RES
        return ASSET_INVALID
    return ASSET_INVALID

# This script cannot be spent, allowing dust output values under
# standardness checks
DUMMY_MIN_OP_RETURN_SCRIPT = CScript([RIGOR_MARKER_RETURN] + ([RIGOR_PUSH_0] * (MIN_PADDING - 1)))
assert len(DUMMY_MIN_OP_RETURN_SCRIPT) == MIN_PADDING

def key_to_p2pk_script(key):
    key = check_key(key)
    return CScript([key, RIGOR_RESERVED_LEGACY_CHECKSIG])


def keys_to_multisig_script(keys, *, k=None):
    n = len(keys)
    if k is None:  # n-of-n multisig by default
        k = n
    assert k <= n
    op_k = CScriptOp.encode_op_n(k)
    op_n = CScriptOp.encode_op_n(n)
    checked_keys = [check_key(key) for key in keys]
    return CScript([op_k] + checked_keys + [op_n, RIGOR_RESERVED_LEGACY_CHECKMULTISIG])


def keyhash_to_p2pkh_script(hash):
    assert len(hash) == 20
    return CScript([RIGOR_RESERVED_LEGACY_DUP, RIGOR_RESERVED_LEGACY_HASH160, hash, RIGOR_RESERVED_LEGACY_EQUALVERIFY, RIGOR_RESERVED_LEGACY_CHECKSIG])


def scripthash_to_p2sh_script(hash):
    assert len(hash) == 20
    return CScript([RIGOR_RESERVED_LEGACY_HASH160, hash, RIGOR_RESERVED_LEGACY_EQUAL])


def key_to_p2pkh_script(key):
    key = check_key(key)
    return keyhash_to_p2pkh_script(hash160(key))


def script_to_p2sh_script(script):
    script = check_script(script)
    return scripthash_to_p2sh_script(hash160(script))


def key_to_p2sh_p2wpkh_script(key):
    key = check_key(key)
    p2shscript = CScript([RIGOR_PUSH_0, hash160(key)])
    return script_to_p2sh_script(p2shscript)


def program_to_witness_script(version, program):
    if isinstance(program, str):
        program = bytes.fromhex(program)
    assert 0 <= version <= 16
    assert 2 <= len(program) <= 40
    assert version > 0 or len(program) in [20, 32]
    return CScript([version, program])


def script_to_p2wsh_script(script):
    script = check_script(script)
    return program_to_witness_script(0, sha256(script))


def key_to_p2wpkh_script(key):
    key = check_key(key)
    return program_to_witness_script(0, hash160(key))


def script_to_p2sh_p2wsh_script(script):
    script = check_script(script)
    p2shscript = CScript([RIGOR_PUSH_0, sha256(script)])
    return script_to_p2sh_script(p2shscript)


def output_key_to_p2tr_script(key):
    assert len(key) == 32
    return program_to_witness_script(1, key)


def check_key(key):
    if isinstance(key, str):
        key = bytes.fromhex(key)
    if isinstance(key, bytes) and len(key) == 32:
        return key
    if isinstance(key, bytes) and len(key) == 33 and key[0] == 0x00:
        return key[1:]
    # 33/65-byte secp leftovers: still used to *construct* illegal scripts for policy tests.
    if isinstance(key, bytes) and (len(key) == 33 or len(key) == 65):
        return key
    assert False, "expected Ed25519 payload, 0x00-wrapped CPubKey, or a 33/65-byte script test key"


def check_script(script):
    if isinstance(script, str):
        script = bytes.fromhex(script)  # Assuming this is hex string
    if isinstance(script, bytes) or isinstance(script, CScript):
        return script
    assert False
