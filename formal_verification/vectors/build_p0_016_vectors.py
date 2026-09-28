#!/usr/bin/env python3
"""Build P0-016 Consensus Golden Vector JSON (mutations, Ed25519 sigs, native tx, ASERT)."""
from __future__ import annotations

import copy
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "python"))

from rigor_ed25519 import seed_to_keypair, sign_detached, verify_detached  # noqa: E402
from rigor_hash160 import hash160  # noqa: E402
from rigor_sighash import signature_hash  # noqa: E402
from rigor_tx_wire import serialize_tx  # noqa: E402

VEC = Path(__file__).resolve().parent
SEED = bytes.fromhex("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f")


def load_sighash() -> list:
    return json.loads((VEC / "rigor_sighash_vectors.json").read_text(encoding="utf-8"))


def write(name: str, obj) -> None:
    dest = VEC / name
    dest.write_text(json.dumps(obj, indent=1) + "\n", encoding="utf-8")
    print(f"wrote {dest}")


def build_ed25519(rows: list) -> list:
    pk, sk = seed_to_keypair(SEED)
    out = []
    for row in rows:
        digest = bytes.fromhex(row["expected_hash"])
        sig = sign_detached(digest, sk)
        assert verify_detached(sig, digest, pk)
        assert sign_detached(digest, sk) == sig
        mutated = bytearray(sig)
        mutated[0] ^= 1
        assert not verify_detached(bytes(mutated), digest, pk)
        out.append(
            {
                "asset": row["asset"],
                "seed": SEED.hex(),
                "pubkey": pk.hex(),
                "message": digest.hex(),
                "signature": sig.hex(),
                "reject_signature": bytes(mutated).hex(),
            }
        )
    return out


def apply_mutation(base: dict, name: str) -> dict:
    m = copy.deepcopy(base)
    m["mutation"] = name
    m["parent"] = base["asset"]
    m["parent_hash"] = base["expected_hash"]
    if name == "amount":
        m["amount"] = str(int(base["amount"]) - 1)
    elif name == "script":
        m["script"] = "0014" + "34" * 20
    elif name == "chain_id":
        m["chain_id"] = "12" * 32
    elif name == "nVersion":
        m["nVersion"] = int(base["nVersion"]) + 1
    elif name == "nLockTime":
        m["nLockTime"] = int(base["nLockTime"]) + 1
    elif name == "prevout_hash":
        m["vin"][0]["hash"] = "23" * 32
    elif name == "prevout_n":
        m["vin"][1]["n"] = int(m["vin"][1]["n"]) + 1
    elif name == "nSequence":
        m["vin"][0]["nSequence"] = int(m["vin"][0]["nSequence"]) - 1
    elif name == "output_value":
        m["vout"][0]["value"] = int(m["vout"][0]["value"]) - 1
    elif name == "output_script":
        m["vout"][1]["scriptPubKey"] = "5120" + "44" * 32
    elif name == "asset_tag_res":
        m["asset"] = "RES"
        m["tag"] = "RGRRES01"
        m["sigversion"] = "WITNESS_V1_RES"
    else:
        raise ValueError(name)
    digest = signature_hash(m)
    m["expected_hash"] = digest.hex()
    pre = __import__("rigor_sighash", fromlist=["sighash_preimage"]).sighash_preimage(m)
    m["preimage"] = pre.hex()
    if digest.hex() == base["expected_hash"]:
        raise SystemExit(f"mutation {name} did not change digest")
    return m


def build_mutations(rows: list) -> list:
    oru = next(r for r in rows if r["asset"] == "ORU")
    names = [
        "amount",
        "script",
        "chain_id",
        "nVersion",
        "nLockTime",
        "prevout_hash",
        "prevout_n",
        "nSequence",
        "output_value",
        "output_script",
        "asset_tag_res",
    ]
    return [apply_mutation(oru, n) for n in names]


def oru_spk(pk: bytes) -> str:
    return "0014" + hash160(pk).hex()


def build_native_tx() -> list:
    pk, sk = seed_to_keypair(SEED)
    spk = oru_spk(pk)
    chain_id = "11" * 32
    amount = 100000000
    base_tx = {
        "nVersion": 2,
        "nLockTime": 0,
        "vin": [{"hash": "22" * 32, "n": 0, "nSequence": 4294967295, "scriptSig": "", "witness": []}],
        "vout": [{"value": amount - 1, "scriptPubKey": spk}],
    }
    sighash_vec = {
        "asset": "ORU",
        "tag": "RGRORU01",
        "sigversion": "WITNESS_V0",
        "chain_id": chain_id,
        "script": spk,
        "amount": str(amount),
        "nVersion": 2,
        "nLockTime": 0,
        "nIn": 0,
        "vin": [{"hash": "22" * 32, "n": 0, "nSequence": 4294967295}],
        "vout": [{"value": amount - 1, "scriptPubKey": spk}],
    }
    digest = signature_hash(sighash_vec)
    sig = sign_detached(digest, sk)
    valid = copy.deepcopy(base_tx)
    valid["vin"][0]["witness"] = [sig.hex(), pk.hex()]
    valid_hex = serialize_tx(valid, with_witness=True).hex()

    bad_sig = copy.deepcopy(valid)
    flip = bytearray(sig)
    flip[0] ^= 1
    bad_sig["vin"][0]["witness"] = [bytes(flip).hex(), pk.hex()]

    p2pkh = copy.deepcopy(base_tx)
    p2pkh["vout"][0]["scriptPubKey"] = "76a914" + "00" * 20 + "88ac"

    p2wsh = copy.deepcopy(base_tx)
    p2wsh["vout"][0]["scriptPubKey"] = "0020" + "22" * 32

    op5 = copy.deepcopy(base_tx)
    op5["vout"][0]["scriptPubKey"] = "55" + "14" + "11" * 20 + "bb"

    mint_body = copy.deepcopy(base_tx)
    mint_body["vout"][0]["scriptPubKey"] = "0164" + "14" + "11" * 20 + "bb"
    mint_body["vout"][0]["value"] = 40000000

    converge_spk = "14" + "00" * 20 + "bd"
    converge_spend = copy.deepcopy(base_tx)
    converge_spend["vout"][0]["scriptPubKey"] = spk

    dup = copy.deepcopy(base_tx)
    dup["vin"].append(copy.deepcopy(dup["vin"][0]))

    coinbase_mint = {
        "nVersion": 1,
        "nLockTime": 0,
        "vin": [
            {
                "hash": "00" * 32,
                "n": 4294967295,
                "nSequence": 4294967295,
                "scriptSig": "5101",
                "witness": [],
            }
        ],
        "vout": [{"value": 40000000, "scriptPubKey": "0164" + "14" + "11" * 20 + "bb"}],
    }

    def pack(name, expect, layer, tx, **extra):
        wit = any(v.get("witness") for v in tx["vin"])
        row = {
            "name": name,
            "expect": expect,
            "layer": layer,
            "chain_id": chain_id,
            "tx_hex": serialize_tx(tx, with_witness=bool(wit)).hex(),
        }
        row.update(extra)
        return row

    return [
        pack(
            "valid_oru_spend",
            True,
            "predicate",
            valid,
            prevouts=[{"hash": "22" * 32, "n": 0, "scriptPubKey": spk, "amount": amount}],
            seed=SEED.hex(),
            sighash=digest.hex(),
            signature=sig.hex(),
        ),
        pack(
            "invalid_signature",
            False,
            "predicate",
            bad_sig,
            prevouts=[{"hash": "22" * 32, "n": 0, "scriptPubKey": spk, "amount": amount}],
            reason="ed25519-verify-false",
        ),
        pack("p2pkh_output_rejected", False, "checktransaction", p2pkh, reason="bad-rigor-script-type"),
        pack("p2wsh_output_rejected", False, "checktransaction", p2wsh, reason="bad-rigor-script-type"),
        pack("op5_mint_output_rejected", False, "checktransaction", op5, reason="bad-rigor-script-type"),
        pack("canonical_mint_noncoinbase_rejected", False, "checktransaction", mint_body, reason="bad-txns-res-mint-noncoinbase"),
        pack(
            "converge_spend_rejected",
            False,
            "checktxinputs",
            converge_spend,
            prevouts=[{"hash": "22" * 32, "n": 0, "scriptPubKey": converge_spk, "amount": amount}],
            reason="bad-txns-spend-convergence",
        ),
        pack("duplicate_vin_rejected", False, "checktransaction", dup, reason="bad-txns-inputs-duplicate"),
        pack("canonical_mint_coinbase_accepted", True, "checktransaction", coinbase_mint),
        {
            "name": "valid_oru_checktransaction",
            "expect": True,
            "layer": "checktransaction",
            "chain_id": chain_id,
            "tx_hex": valid_hex,
        },
    ]


def build_asert() -> list:
    genesis_time = 1792908000
    spacing = 600
    nbits = "1d008000"
    return [
        {
            "id": "height1_copies_genesis_nbits",
            "parent_height": 0,
            "parent_nbits": nbits,
            "parent_time": genesis_time,
            "child_time": genesis_time + spacing,
            "expected_nbits": nbits,
        },
        {
            "id": "height2_spacing_unchanged",
            "parent_height": 1,
            "parent_nbits": nbits,
            "parent_time": genesis_time + spacing,
            "genesis_nbits": nbits,
            "genesis_time": genesis_time,
            "child_time": genesis_time + 2 * spacing,
            "expected_nbits": nbits,
        },
    ]


def main() -> int:
    rows = load_sighash()
    assert len(rows) == 3
    write("rigor_ed25519_signature_vectors.json", build_ed25519(rows))
    write("rigor_sighash_mutation_vectors.json", build_mutations(rows))
    write("rigor_native_tx_vectors.json", build_native_tx())
    write("rigor_asert_vectors.json", build_asert())
    print("P0-016 vector build: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
