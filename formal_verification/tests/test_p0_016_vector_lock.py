#!/usr/bin/env python3
"""P0-016: Python lock for sighash mutations, Ed25519 sig64, native tx hex, ASERT pins."""
from __future__ import annotations

import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "python"))

from rigor_ed25519 import seed_to_keypair, sign_detached, verify_detached  # noqa: E402
from rigor_sighash import sha256d, sighash_preimage, signature_hash  # noqa: E402

VEC = REPO / "formal_verification" / "vectors"


def test_mutations() -> None:
    parents = json.loads((VEC / "rigor_sighash_vectors.json").read_text(encoding="utf-8"))
    parent_hash = parents[0]["expected_hash"]
    rows = json.loads((VEC / "rigor_sighash_mutation_vectors.json").read_text(encoding="utf-8"))
    assert len(rows) >= 11
    seen = set()
    for row in rows:
        name = row["mutation"]
        assert name not in seen, name
        seen.add(name)
        pre = sighash_preimage(row)
        digest = signature_hash(row)
        assert pre.hex() == row["preimage"], name
        assert digest.hex() == row["expected_hash"], name
        assert sha256d(pre).hex() == row["expected_hash"], name
        assert digest.hex() != parent_hash, name
        assert row["parent_hash"] == parent_hash, name


def test_ed25519() -> None:
    rows = json.loads((VEC / "rigor_ed25519_signature_vectors.json").read_text(encoding="utf-8"))
    assert len(rows) == 3
    parents = json.loads((VEC / "rigor_sighash_vectors.json").read_text(encoding="utf-8"))
    by_asset = {r["asset"]: r for r in parents}
    for row in rows:
        seed = bytes.fromhex(row["seed"])
        pk, sk = seed_to_keypair(seed)
        assert pk.hex() == row["pubkey"]
        msg = bytes.fromhex(row["message"])
        assert msg.hex() == by_asset[row["asset"]]["expected_hash"]
        sig = sign_detached(msg, sk)
        assert sig.hex() == row["signature"]
        assert sign_detached(msg, sk) == sig
        assert verify_detached(sig, msg, pk)
        assert not verify_detached(bytes.fromhex(row["reject_signature"]), msg, pk)


def test_native_tx_hex_stable() -> None:
    rows = json.loads((VEC / "rigor_native_tx_vectors.json").read_text(encoding="utf-8"))
    names = {r["name"] for r in rows}
    for required in (
        "valid_oru_spend",
        "invalid_signature",
        "p2pkh_output_rejected",
        "p2wsh_output_rejected",
        "op5_mint_output_rejected",
        "canonical_mint_noncoinbase_rejected",
        "converge_spend_rejected",
        "duplicate_vin_rejected",
        "canonical_mint_coinbase_accepted",
        "valid_oru_checktransaction",
    ):
        assert required in names, required
    for row in rows:
        assert len(row["tx_hex"]) >= 16
        bytes.fromhex(row["tx_hex"])


def test_asert_pins() -> None:
    rows = json.loads((VEC / "rigor_asert_vectors.json").read_text(encoding="utf-8"))
    assert len(rows) == 2
    assert rows[0]["expected_nbits"] == "1d008000"
    assert rows[1]["expected_nbits"] == "1d008000"
    assert rows[0]["parent_time"] == 1786125600


def test_gbt_layout() -> None:
    rows = json.loads((VEC / "rigor_gbt_layout_vectors.json").read_text(encoding="utf-8"))
    assert len(rows) == 1
    row = rows[0]
    assert row["height"] == 1
    # RFC-020 flat ORU subsidy: 128 ORU = 12_800_000_000 Tharns (empty mempool, no fees).
    assert row["coinbasevalue"] == 12800000000
    assert row["mint_present"] is False
    assert row["mint_scriptPubKey"] == ""
    assert row["witness_commitment_index"] == 1
    assert row["payout_scriptPubKey"].startswith("0014")
    assert len(bytes.fromhex(row["payout_scriptPubKey"])) == 22
    commit = bytes.fromhex(row["default_witness_commitment"])
    assert commit[:6] == bytes.fromhex("6a24aa21a9ed")
    assert len(commit) == 38
    mint = bytes.fromhex(row["canonical_mint_script"])
    assert mint[-1] == 0xBB
    assert row["canonical_mint_maturity_height"] == 2
    # Coinbase scriptSig: BIP34 height push (OP_1) + one OP_0 padding byte.
    assert row["coinbase_scriptSig"] == "5100"
    assert row["merkle_root"] == "0b1c6d00facf32c1dc7c7c70e41de14e7c7ff9a4903ad76e24a41bbd488b96ce"
    assert row["prevhash"] == "3070f47e2c1b153a3288382f3258396bb48b016eab76eae9f235d006dcc2af63"
    assert row["bits"] == "207fffff"
    assert row["nTime"] == 1778224815


def test_connectblock_hex_stable() -> None:
    rows = json.loads((VEC / "rigor_connectblock_vectors.json").read_text(encoding="utf-8"))
    names = {r["name"] for r in rows}
    assert "valid_height1_coinbase_only" in names
    assert "invalid_stale_merkle_extra_tx" in names
    for row in rows:
        raw = bytes.fromhex(row["block_hex"])
        assert len(raw) > 80
        assert row["expect"] is (row["name"].startswith("valid"))


def test_connectblock_convergence_catalog_honest() -> None:
    cat = json.loads((VEC / "rigor_connectblock_convergence_catalog.json").read_text(encoding="utf-8"))
    assert cat["live_mint_golden_vector"] == "NOT_COVERED"
    assert cat["reorgbind_golden_vector"] == "NOT_COVERED"
    names = [c["name"] for c in cat["cases"]]
    assert names == [
        "valid_block",
        "invalid_merkle",
        "invalid_tx",
        "invalid_coinbase",
        "invalid_witness",
        "invalid_predicate",
        "invalid_pow",
        "invalid_target",
        "invalid_timestamp",
        "invalid_issuance",
        "invalid_res",
        "invalid_draw",
        "invalid_supply",
        "invalid_maturity",
        "duplicate_tx",
        "malformed_block",
        "state_mutation_failure",
        "connect_disconnect_connect",
        "reorg",
        "invalid_after_valid",
    ]
    hexes = cat["header_hex_mutations"]
    bytes.fromhex(hexes["invalid_merkle_root_xor"])
    bytes.fromhex(hexes["invalid_nbits_zero"])
    assert len(bytes.fromhex(hexes["malformed_truncated_header"])) < 80


def main() -> int:
    test_mutations()
    test_ed25519()
    test_native_tx_hex_stable()
    test_asert_pins()
    test_gbt_layout()
    test_connectblock_hex_stable()
    test_connectblock_convergence_catalog_honest()
    print("P0-016 Python vector lock: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
