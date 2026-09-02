# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Bitcoin-shaped witness tx wire encoding for golden hex. Harness only."""
from __future__ import annotations

from typing import Any, Mapping, Sequence

from rigor_sighash import compact_size, i32le, i64le, ser_outpoint, sha256d, u32le, _hex


def ser_vin(vin: Mapping[str, Any]) -> bytes:
    script_sig = _hex(vin.get("scriptSig", ""))
    return ser_outpoint(vin["hash"], int(vin["n"])) + compact_size(len(script_sig)) + script_sig + u32le(int(vin["nSequence"]))


def ser_vout(vout: Mapping[str, Any]) -> bytes:
    spk = _hex(vout["scriptPubKey"])
    return i64le(int(vout["value"])) + compact_size(len(spk)) + spk


def ser_witness_stack(stack: Sequence[str]) -> bytes:
    out = compact_size(len(stack))
    for item in stack:
        raw = _hex(item)
        out += compact_size(len(raw)) + raw
    return out


def serialize_tx(tx: Mapping[str, Any], *, with_witness: bool) -> bytes:
    vin = tx["vin"]
    vout = tx["vout"]
    body = i32le(int(tx["nVersion"]))
    witnesses = [v.get("witness", []) for v in vin]
    has_wit = with_witness and any(w for w in witnesses)
    if has_wit:
        body += b"\x00\x01"
    body += compact_size(len(vin))
    for v in vin:
        body += ser_vin(v)
    body += compact_size(len(vout))
    for o in vout:
        body += ser_vout(o)
    if has_wit:
        for w in witnesses:
            body += ser_witness_stack(w)
    body += u32le(int(tx["nLockTime"]))
    return body


def txid(tx: Mapping[str, Any]) -> bytes:
    return sha256d(serialize_tx(tx, with_witness=False))
