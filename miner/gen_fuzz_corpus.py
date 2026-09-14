#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Regenerate the fuzz_gbt_solo seed corpus.

The seeds are structured getblocktemplate responses rather than random bytes,
so the fuzzer starts past the JSON parser and inside the coinbase verifier.
Payouts use the all-zero witness-v0 key hash that fuzz_gbt_solo.c passes in,
so at least one seed reaches the accept path.

Usage: ./gen_fuzz_corpus.py [outdir]
"""

import hashlib
import json
import os
import struct
import sys

PAYOUT_SPK = bytes([0x00, 0x14]) + bytes(20)
COMMIT_SPK = bytes.fromhex(
    "6a24aa21a9ede2f61c3f71d1defd3fa999dfa36953755c690689799962b48bebd836974e8cf9"
)
MINT_SPK = bytes.fromhex("0105141111111111111111111111111111111111111111bb")
ROOT = "11" * 32
ZERO = "00" * 32
# RFC-020 height ≥ 1 subsidy: 128 ORU. Fuzz seeds are reject-path
# mutations, not shipping templates, but the amount must not look like
# the retired 1000 ORU / three-halving schedule.
COINBASE_VALUE = 12800000000
MINT_VALUE = 40000000


def sha256d(b):
    return hashlib.sha256(hashlib.sha256(b).digest()).digest()


def scriptsig(height_op, root_hex, winner_hex):
    return (
        bytes([height_op])
        + bytes([0x20])
        + bytes.fromhex(root_hex)
        + bytes([0x20])
        + bytes.fromhex(winner_hex)
    )


def build_coinbase(ss, outs, *, version=2, sequence=0xFFFFFFFF, locktime=0,
                   witness=True, witness_reserved_len=32, trailing=b""):
    """outs is a list of (value, scriptPubKey)."""
    head = struct.pack("<I", version)
    marker = b"\x00\x01" if witness else b""
    vin = (
        b"\x01"
        + bytes(32)
        + b"\xff\xff\xff\xff"
        + bytes([len(ss)])
        + ss
        + struct.pack("<I", sequence)
    )
    vout = bytes([len(outs)])
    for value, spk in outs:
        vout += struct.pack("<q", value) + bytes([len(spk)]) + spk
    wit = b""
    if witness:
        wit = b"\x01" + bytes([witness_reserved_len]) + bytes(witness_reserved_len)
    tail = struct.pack("<I", locktime)
    raw = head + marker + vin + vout + wit + tail + trailing
    txid = sha256d(head + vin + vout + tail)
    return raw, txid


def template(cb_raw, cb_txid, *, mint=None, **overrides):
    txid_hex = cb_txid[::-1].hex()
    t = {
        "version": 536870912,
        "rules": ["csv", "!segwit"],
        "mutable": ["time"],
        "previousblockhash": ROOT,
        "transactions": [],
        "coinbasevalue": COINBASE_VALUE,
        "target": "7fffff" + "00" * 29,
        "mintime": 1,
        "maxtime": 2000000000,
        "curtime": 1700000000,
        "bits": "207fffff",
        "height": 1,
        "default_witness_commitment": COMMIT_SPK.hex(),
        "default_coinbase_scriptsig": None,
        "default_coinbase_tx": cb_raw.hex(),
        "default_coinbase_txid": txid_hex,
        "merkleroot": txid_hex,
        "paid_winner_txid": ZERO,
        "convergence_root": ROOT,
        "rigor_coinbase_immutable": True,
    }
    if mint is not None:
        t["default_res_mint"] = mint
    t.update(overrides)
    return t


def seeds():
    out = {}

    ss_no_mint = scriptsig(0x51, ROOT, ZERO)
    raw, txid = build_coinbase(
        ss_no_mint, [(COINBASE_VALUE, PAYOUT_SPK), (0, COMMIT_SPK)]
    )
    valid = template(raw, txid, default_coinbase_scriptsig=ss_no_mint.hex())
    out["valid_no_mint"] = valid

    ss_mint = scriptsig(0x51, ROOT, ROOT)
    raw_m, txid_m = build_coinbase(
        ss_mint,
        [(COINBASE_VALUE, PAYOUT_SPK), (MINT_VALUE, MINT_SPK), (0, COMMIT_SPK)],
    )
    out["valid_with_mint"] = template(
        raw_m,
        txid_m,
        mint={"value": MINT_VALUE, "scriptPubKey": MINT_SPK.hex()},
        default_coinbase_scriptsig=ss_mint.hex(),
        paid_winner_txid=ROOT,
    )

    # Structural mutations of the coinbase, each keeping txid/merkleroot self-consistent.
    variants = {
        "cb_version_3": dict(version=3),
        "cb_nonfinal_sequence": dict(sequence=0),
        "cb_locktime_set": dict(locktime=1),
        "cb_legacy_serialization": dict(witness=False),
        "cb_short_witness_reserved": dict(witness_reserved_len=1),
        "cb_trailing_junk": dict(trailing=b"\xcc\xcc\xcc"),
    }
    for name, kwargs in variants.items():
        r, t = build_coinbase(
            ss_no_mint, [(COINBASE_VALUE, PAYOUT_SPK), (0, COMMIT_SPK)], **kwargs
        )
        out[name] = template(r, t, default_coinbase_scriptsig=ss_no_mint.hex())

    r, t = build_coinbase(ss_no_mint, [(0, COMMIT_SPK), (COINBASE_VALUE, PAYOUT_SPK)])
    out["cb_payout_not_first"] = template(r, t, default_coinbase_scriptsig=ss_no_mint.hex())

    r, t = build_coinbase(
        ss_no_mint,
        [(COINBASE_VALUE, PAYOUT_SPK), (0, COMMIT_SPK), (0, b"\x6a\x00")],
    )
    out["cb_extra_output"] = template(r, t, default_coinbase_scriptsig=ss_no_mint.hex())

    r, t = build_coinbase(ss_no_mint, [(COINBASE_VALUE - 1, PAYOUT_SPK), (0, COMMIT_SPK)])
    out["cb_wrong_value"] = template(r, t, default_coinbase_scriptsig=ss_no_mint.hex())

    # Template-level mutations.
    out["missing_coinbase_tx"] = {k: v for k, v in valid.items() if k != "default_coinbase_tx"}
    out["missing_merkleroot"] = {k: v for k, v in valid.items() if k != "merkleroot"}
    out["missing_coinbase_txid"] = {
        k: v for k, v in valid.items() if k != "default_coinbase_txid"
    }
    out["wrong_merkleroot"] = dict(valid, merkleroot="00" * 31 + "01")
    out["truncated_coinbase_hex"] = dict(
        valid, default_coinbase_tx=valid["default_coinbase_tx"][:-20]
    )
    out["odd_length_coinbase_hex"] = dict(
        valid, default_coinbase_tx=valid["default_coinbase_tx"][:-1]
    )
    out["mutable_coinbase"] = dict(valid, mutable=["time", "coinbase"])
    out["coinbasevalue_as_real"] = dict(valid, coinbasevalue=1.0e9)
    out["coinbasevalue_as_string"] = dict(valid, coinbasevalue=str(COINBASE_VALUE))
    out["coinbasevalue_overflow"] = dict(valid, coinbasevalue=2**70)
    out["height_zero"] = dict(valid, height=0)
    out["curtime_below_mintime"] = dict(valid, curtime=1, mintime=100)
    out["bits_short"] = dict(valid, bits="7fffff")
    out["bits_non_hex"] = dict(valid, bits="20 7ffff")
    out["huge_tx_array"] = dict(
        valid, transactions=[{"data": "00" * 32, "hash": ZERO}] * 512
    )
    out["tx_data_odd_length"] = dict(
        valid, transactions=[{"data": "000", "hash": ZERO}]
    )
    return out


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "fuzz_corpus", "gbt_solo"
    )
    os.makedirs(outdir, exist_ok=True)
    written = 0
    for name, obj in seeds().items():
        with open(os.path.join(outdir, name + ".json"), "w", encoding="utf-8") as f:
            json.dump(obj, f, separators=(",", ":"), sort_keys=True)
        written += 1
    # Degenerate inputs the JSON parser itself must survive.
    for name, blob in {
        "empty": "",
        "empty_object": "{}",
        "not_an_object": "[1,2,3]",
        "deep_nesting": "[" * 64 + "]" * 64,
    }.items():
        with open(os.path.join(outdir, name + ".json"), "w", encoding="utf-8") as f:
            f.write(blob)
        written += 1
    print(f"wrote {written} seeds to {outdir}")


if __name__ == "__main__":
    main()
