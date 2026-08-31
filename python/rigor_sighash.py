# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P0-024 vector / tool harness for the unique Ed25519 SignatureHash preimage.

This module reconstructs the C++ `SignatureHash()` byte string and SHA256d
digest for golden-vector checks. It is **not** a consensus implementation
and must not be imported by any node, wallet, or mint path.

C++ (predicate_verify.cpp), frozen:

    tag || chain_id || nVersion || hashPrevouts || hashSequence
    || hashOutputs || this prevout || spent scriptPubKey || amount
    || nSequence || nLockTime

Tags: RGRORU01 / RGRRES01 / RGRMNT01. chain_id is an explicit argument.
"""
from __future__ import annotations

import hashlib
from typing import Any, Mapping, Sequence

TAG_ORU = b"RGRORU01"
TAG_RES = b"RGRRES01"
TAG_MINT = b"RGRMNT01"

_TAGS = {
    "ORU": TAG_ORU,
    "RES": TAG_RES,
    "RES_MINT": TAG_MINT,
    "MINT": TAG_MINT,
}


def sha256(payload: bytes) -> bytes:
    return hashlib.sha256(payload).digest()


def sha256d(payload: bytes) -> bytes:
    """C++ CHash256 / HashWriter::GetHash."""
    return sha256(sha256(payload))


def u32le(n: int) -> bytes:
    return int(n).to_bytes(4, "little", signed=False)


def i32le(n: int) -> bytes:
    return int(n).to_bytes(4, "little", signed=True)


def i64le(n: int) -> bytes:
    return int(n).to_bytes(8, "little", signed=True)


def compact_size(n: int) -> bytes:
    if n < 0:
        raise ValueError("compact_size of negative")
    if n < 253:
        return bytes([n])
    if n <= 0xFFFF:
        return b"\xfd" + n.to_bytes(2, "little")
    if n <= 0xFFFFFFFF:
        return b"\xfe" + n.to_bytes(4, "little")
    return b"\xff" + n.to_bytes(8, "little")


def _hex(s: str) -> bytes:
    if not s:
        return b""
    return bytes.fromhex(s)


def ser_outpoint(txid_hex: str, n: int) -> bytes:
    h = _hex(txid_hex)
    if len(h) != 32:
        raise ValueError(f"outpoint hash must be 32 bytes, got {len(h)}")
    return h + u32le(n)


def ser_txout(value: int, script_pubkey: bytes) -> bytes:
    return i64le(value) + compact_size(len(script_pubkey)) + script_pubkey


def hash_prevouts(vin: Sequence[Mapping[str, Any]]) -> bytes:
    buf = b"".join(ser_outpoint(i["hash"], int(i["n"])) for i in vin)
    return sha256d(buf)


def hash_sequence(vin: Sequence[Mapping[str, Any]]) -> bytes:
    buf = b"".join(u32le(int(i["nSequence"])) for i in vin)
    return sha256d(buf)


def hash_outputs(vout: Sequence[Mapping[str, Any]]) -> bytes:
    buf = b"".join(
        ser_txout(int(o["value"]), _hex(o["scriptPubKey"])) for o in vout
    )
    return sha256d(buf)


def sighash_preimage(vec: Mapping[str, Any]) -> bytes:
    """Independent reconstruction of the C++ SignatureHash preimage."""
    asset = str(vec["asset"])
    tag = vec.get("tag")
    tag_b = tag.encode("ascii") if isinstance(tag, str) and tag else _TAGS[asset]
    chain_id = _hex(vec["chain_id"])
    if len(chain_id) != 32:
        raise ValueError("chain_id must be 32 bytes")
    script = _hex(vec["script"])
    amount = int(vec["amount"])
    n_in = int(vec["nIn"])
    vin = vec["vin"]
    vout = vec["vout"]
    this = vin[n_in]
    return (
        tag_b
        + chain_id
        + i32le(int(vec["nVersion"]))
        + hash_prevouts(vin)
        + hash_sequence(vin)
        + hash_outputs(vout)
        + ser_outpoint(this["hash"], int(this["n"]))
        + compact_size(len(script))
        + script
        + i64le(amount)
        + u32le(int(this["nSequence"]))
        + u32le(int(vec["nLockTime"]))
    )


def signature_hash(vec: Mapping[str, Any]) -> bytes:
    """SHA256d of the unique tagged preimage. Harness only."""
    return sha256d(sighash_preimage(vec))
