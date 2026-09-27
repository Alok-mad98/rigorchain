#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P0-086B audit-only dual-asset (ORU / RES) CoinsDB harness.

Reuses the P0-086 Coin / undo / key codec. This is not a Consensus Kernel.
It locks:

    shared Coin container
    +
    script-as-asset identity
    +
    reconstruction / reorg / restart
    +
    failure boundary (including ORU ↔ RES reclassification)

Asset identity is NOT a Coin field. Production ClassifyRigorTxout infers
it from scriptPubKey after the record is loaded.
"""

from __future__ import annotations

import hashlib
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Tuple

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import database_state_codec as db  # noqa: E402

VECTOR_DIR = ROOT / "formal_verification" / "dual_asset_database_vectors"
CATALOG = VECTOR_DIR / "catalog.json"

CVG2 = b"CVG2"
RES2 = b"RES2"
MAX_MONEY = 10_000_000_000 * 100_000_000
RES_ATOMIC = 1
N_COINBASE_MATURITY = 100


def classify_asset(script: bytes) -> str:
    """Replica of ClassifyRigorTxout → AssetTypeFromScriptPubKey.

    Fail-closed: unknown / malformed / retired prefixes → INVALID.
    Not a second Consensus Kernel.
    """
    if not script:
        return "INVALID"
    if script[0] == db.RIGOR_MARKER_RETURN:
        return "NONE"
    if (
        len(script) == 22
        and script[0] == 20
        and script[21] == db.RIGOR_MARKER_CONVERGE
    ):
        return "ORU"
    if len(script) == 22 and script[0] == 0x00 and script[1] == 20:
        return "ORU"
    if len(script) == 34 and script[0] == 0x51 and script[1] == 32:
        prog = script[2:34]
        if prog[:4] == CVG2 or prog[:4] == RES2:
            return "INVALID"
        return "RES"
    if len(script) >= 23 and script[-1] == db.RIGOR_MARKER_RES_MINT:
        return "RES"
    return "INVALID"


def money_range(n: int) -> bool:
    return 0 <= n <= MAX_MONEY


def res_amount_ok(n: int) -> bool:
    return money_range(n) and n % RES_ATOMIC == 0


def coinbase_mature(spend_h: int, coin_h: int) -> bool:
    return spend_h - coin_h >= N_COINBASE_MATURITY


def encode_key(txid: bytes, n: int) -> bytes:
    return db.encode_coin_key(txid, n)


def same_key_schema(oru_key: bytes, res_key: bytes) -> bool:
    return (
        len(oru_key) >= 34
        and len(res_key) >= 34
        and oru_key[0] == db.DB_COIN
        and res_key[0] == db.DB_COIN
    )


@dataclass
class DualCoin:
    n_value: int
    script: bytes
    height: int
    coinbase: bool

    def asset(self) -> str:
        return classify_asset(self.script)

    def as_db(self) -> db.Coin:
        return db.Coin(self.n_value, self.script, self.height, self.coinbase)

    def encode(self) -> bytes:
        return db.encode_coin(self.as_db())

    def digest_tuple(self) -> Tuple[int, bytes, int, bool, str]:
        return (self.n_value, self.script, self.height, self.coinbase, self.asset())


def decode_dual(raw: bytes) -> Tuple[DualCoin, dict]:
    coin, notes = db.decode_coin(raw)
    return DualCoin(coin.n_value, coin.script_pubkey, coin.height, coin.coinbase), notes


def _h(tag: str) -> bytes:
    return hashlib.sha256(tag.encode()).digest()


def spk_oru(seed: int) -> bytes:
    return bytes([0x00, 20]) + bytes([(seed + i) % 256 for i in range(20)])


def spk_res(seed: int) -> bytes:
    return bytes([0x51, 32]) + bytes([(seed + i) % 256 for i in range(32)])


def spk_mint(height: int, seed: int) -> bytes:
    if 1 <= height <= 16:
        hbytes = bytes([0x50 + height])
    elif height == 0:
        hbytes = bytes([0x00])
    else:
        raw = height.to_bytes((height.bit_length() + 8) // 8, "little")
        hbytes = bytes([len(raw)]) + raw
    return hbytes + bytes([20]) + bytes([(seed + i) % 256 for i in range(20)]) + bytes(
        [db.RIGOR_MARKER_RES_MINT]
    )


def spk_converge(seed: int) -> bytes:
    return bytes([20]) + bytes([(seed + i) % 256 for i in range(20)]) + bytes(
        [db.RIGOR_MARKER_CONVERGE]
    )


def spk_unknown() -> bytes:
    return bytes([0x76, 0xA9, 20]) + bytes(range(20)) + bytes([0x88, 0xAC])


def spk_malformed() -> bytes:
    return bytes([0x51, 16]) + bytes(range(16))


def spk_retired_res2() -> bytes:
    return bytes([0x51, 32]) + RES2 + bytes(28)


def mutate_oru_to_res(script: bytes) -> bytes:
    """Flip a v0/20 ORU script into a v1/32 RES script (same payload padded)."""
    if len(script) == 22 and script[0] == 0x00 and script[1] == 20:
        return bytes([0x51, 32]) + script[2:22] + bytes(12)
    return script


def mutate_res_to_oru(script: bytes) -> bytes:
    if len(script) == 34 and script[0] == 0x51 and script[1] == 32:
        return bytes([0x00, 20]) + script[2:22]
    return script


def supply_of(view: db.CoinsView) -> Dict[str, int]:
    oru = 0
    res = 0
    other = 0
    for coin in view.coins.values():
        a = classify_asset(coin.script_pubkey)
        if a == "ORU":
            oru += coin.n_value
        elif a == "RES":
            res += coin.n_value
        else:
            other += coin.n_value
    return {"oru": oru, "res": res, "other": other, "total_atoms": oru + res + other}


def state_digest_by_asset(view: db.CoinsView) -> Dict[str, str]:
    buckets: Dict[str, List[Tuple]] = {"ORU": [], "RES": [], "NONE": [], "INVALID": []}
    for (txid, n), coin in sorted(view.coins.items()):
        a = classify_asset(coin.script_pubkey)
        buckets.setdefault(a, []).append(
            (txid, n, coin.n_value, coin.script_pubkey, coin.height, coin.coinbase)
        )
    out = {}
    for asset, rows in buckets.items():
        h = hashlib.sha256()
        h.update(asset.encode())
        for txid, n, val, spk, height, cb in rows:
            h.update(txid)
            h.update(n.to_bytes(4, "little"))
            h.update(val.to_bytes(8, "little", signed=True))
            h.update(len(spk).to_bytes(4, "little"))
            h.update(spk)
            h.update(height.to_bytes(4, "little"))
            h.update(bytes([1 if cb else 0]))
        out[asset] = h.hexdigest()
    return out


@dataclass
class DualBlock:
    block_hash: bytes
    height: int
    txs: List[db.Tx]


def apply_dual(cache: db.CoinsViewCache, block: DualBlock) -> List[bytes]:
    return db.apply_block(cache, db.Block(block.block_hash, block.height, block.txs))


def disconnect_dual(cache: db.CoinsViewCache, block: DualBlock, undos: List[bytes]) -> None:
    db.disconnect_block(cache, db.Block(block.block_hash, block.height, block.txs), undos)


def golden_dual_chains() -> Tuple[List[DualBlock], List[DualBlock]]:
    """Genesis → A → B → C   vs   Genesis → A → D → E

    Contains ORU create/spend, RES mint, RES transfer, same-block dual asset,
    same-tx dual-pool shape, and a convergence skip.
    """
    g_txid = _h("da-genesis-cb")
    genesis = DualBlock(
        _h("da-genesis"),
        0,
        [db.Tx(g_txid, [], [db.TxOut(50_0000_0000, spk_oru(1))], coinbase=True)],
    )

    a_cb = _h("da-a-cb")
    a_oru = _h("da-a-oru")
    a_dual = _h("da-a-dual")
    # Height-1 coinbase carries ORU subsidy + RES mint. Body spends genesis ORU
    # and also creates a non-mint RES (harness persistence only).
    block_a = DualBlock(
        _h("da-a"),
        1,
        [
            db.Tx(
                a_cb,
                [],
                [
                    db.TxOut(50_0000_0000, spk_oru(2)),
                    db.TxOut(40_0000_00, spk_mint(101, 3)),
                ],
                coinbase=True,
            ),
            db.Tx(
                a_oru,
                [(g_txid, 0)],
                [
                    db.TxOut(40_0000_0000, spk_oru(4)),
                    db.TxOut(9_0000_0000, spk_converge(5)),
                    db.TxOut(1_0000_0000, spk_res(6)),
                ],
            ),
        ],
    )

    b_cb = _h("da-b-cb")
    b_oru = _h("da-b-oru")
    b_res = _h("da-b-res")
    block_b = DualBlock(
        _h("da-b"),
        2,
        [
            db.Tx(b_cb, [], [db.TxOut(50_0000_0000, spk_oru(7))], coinbase=True),
            db.Tx(b_oru, [(a_oru, 0)], [db.TxOut(39_0000_0000, spk_oru(8))]),
            db.Tx(b_res, [(a_oru, 2)], [db.TxOut(1_0000_0000, spk_res(9))]),
        ],
    )

    c_cb = _h("da-c-cb")
    c_mix = _h("da-c-mix")
    block_c = DualBlock(
        _h("da-c"),
        3,
        [
            db.Tx(
                c_cb,
                [],
                [db.TxOut(50_0000_0000, spk_oru(10)), db.TxOut(40_0000_00, spk_mint(103, 11))],
                coinbase=True,
            ),
            # Dual-pool shape: RES input + ORU fee input, both outputs persist.
            db.Tx(
                c_mix,
                [(b_oru, 0), (b_res, 0)],
                [
                    db.TxOut(38_0000_0000, spk_oru(12)),
                    db.TxOut(1_0000_0000, spk_res(13)),
                ],
            ),
        ],
    )

    d_cb = _h("da-d-cb")
    d_oru = _h("da-d-oru")
    block_d = DualBlock(
        _h("da-d"),
        2,
        [
            db.Tx(d_cb, [], [db.TxOut(50_0000_0000, spk_oru(20))], coinbase=True),
            db.Tx(d_oru, [(a_oru, 0)], [db.TxOut(40_0000_0000, spk_oru(21))]),
        ],
    )

    e_cb = _h("da-e-cb")
    e_res = _h("da-e-res")
    block_e = DualBlock(
        _h("da-e"),
        3,
        [
            db.Tx(e_cb, [], [db.TxOut(50_0000_0000, spk_oru(22))], coinbase=True),
            db.Tx(e_res, [(a_oru, 2)], [db.TxOut(1_0000_0000, spk_res(23))]),
        ],
    )

    main = [genesis, block_a, block_b, block_c]
    alt = [genesis, block_a, block_d, block_e]
    return main, alt


def run_path(blocks: List[DualBlock], mode: str) -> Tuple[str, Dict[str, str], Dict[str, int]]:
    store = db.CoinsView()
    cache = db.CoinsViewCache(store)
    for i, block in enumerate(blocks):
        apply_dual(cache, block)
        if mode == "clean":
            cache.flush()
        elif mode == "cache_heavy":
            if i == len(blocks) - 1:
                cache.flush()
        elif mode == "restart":
            cache.flush()
            cache = db.CoinsViewCache(
                db.CoinsView.from_serialized(cache.base.serialize_all(), cache.base.best_block)
            )
        elif mode == "flush_restart":
            cache.flush()
            cache = cache.restart()
    cache.flush()
    return cache.base.state_digest(), state_digest_by_asset(cache.base), supply_of(cache.base)


def run_reorg() -> Tuple[dict, dict]:
    main, alt = golden_dual_chains()
    store = db.CoinsView()
    cache = db.CoinsViewCache(store)
    undos: Dict[bytes, List[bytes]] = {}
    for block in main:
        undos[block.block_hash] = apply_dual(cache, block)
        cache.flush()
    disconnect_dual(cache, main[-1], undos[main[-1].block_hash])
    disconnect_dual(cache, main[-2], undos[main[-2].block_hash])
    for block in alt[2:]:
        undos[block.block_hash] = apply_dual(cache, block)
        cache.flush()
    via_reorg = {
        "digest": cache.base.state_digest(),
        "by_asset": state_digest_by_asset(cache.base),
        "supply": supply_of(cache.base),
    }

    store2 = db.CoinsView()
    cache2 = db.CoinsViewCache(store2)
    for block in alt:
        apply_dual(cache2, block)
        cache2.flush()
    via_clean = {
        "digest": cache2.base.state_digest(),
        "by_asset": state_digest_by_asset(cache2.base),
        "supply": supply_of(cache2.base),
    }
    return via_reorg, via_clean


def rebuild_from_blocks(blocks: List[DualBlock]) -> Tuple[str, Dict[str, str], Dict[str, int]]:
    store = db.CoinsView()
    cache = db.CoinsViewCache(store)
    for block in blocks:
        apply_dual(cache, block)
    cache.flush()
    rebuilt = db.CoinsView.from_serialized(store.serialize_all(), store.best_block)
    return rebuilt.state_digest(), state_digest_by_asset(rebuilt), supply_of(rebuilt)


def mutation_cases() -> List[dict]:
    oru = DualCoin(50_0000_0000, spk_oru(1), 1, True)
    res = DualCoin(1_0000_0000, spk_res(2), 1, False)
    mint = DualCoin(40_0000_00, spk_mint(101, 3), 1, True)

    cases = []
    flipped = DualCoin(oru.n_value, mutate_oru_to_res(oru.script), oru.height, oru.coinbase)
    cases.append(
        {
            "id": "oru_script_mutated_to_res",
            "before": oru.asset(),
            "after": flipped.asset(),
            "reclassified": oru.asset() != flipped.asset(),
            "same_key_would_decode_as_other_asset": True,
            "production_checksum": False,
            "fail_closed": False,
            "note": "No asset tag. Script mutation is asset mutation.",
        }
    )
    flipped_r = DualCoin(res.n_value, mutate_res_to_oru(res.script), res.height, res.coinbase)
    cases.append(
        {
            "id": "res_script_mutated_to_oru",
            "before": res.asset(),
            "after": flipped_r.asset(),
            "reclassified": res.asset() != flipped_r.asset(),
            "same_key_would_decode_as_other_asset": True,
            "production_checksum": False,
            "fail_closed": False,
            "note": "Corrupt RES v1/32 → v0/20 is classified as ORU after load.",
        }
    )
    mint_as_res = DualCoin(mint.n_value, spk_res(99), mint.height, mint.coinbase)
    cases.append(
        {
            "id": "mint_script_mutated_to_v1_res",
            "before": mint.asset(),
            "after": mint_as_res.asset(),
            "reclassified": False,
            "lost_mint_identity": mint.asset() == "RES" and mint.script != mint_as_res.script,
            "fail_closed": False,
            "note": "Still ASSET_RES, but mint lock / script-encoded maturity is gone.",
        }
    )
    return cases


def amount_cases() -> List[dict]:
    rows = []
    for label, n in (
        ("zero", 0),
        ("one", 1),
        ("oru_coin", 100_000_000),
        ("res_coin_display", 1_000_000),
        ("max_money", MAX_MONEY),
        ("max_money_plus_one", MAX_MONEY + 1),
        ("negative", -1),
    ):
        rows.append(
            {
                "case": label,
                "n": n,
                "oru_money_range": money_range(n),
                "res_money_range": money_range(n),
                "res_amount_ok": res_amount_ok(n) if n >= 0 else False,
                "same_storage_type": True,
                "compress_roundtrip": (
                    db.decompress_amount(db.compress_amount(n)) == n if n >= 0 else None
                ),
            }
        )
    # Cross-asset arithmetic is the same integer type. Consensus must not
    # treat the sum as one monetary unit.
    rows.append(
        {
            "case": "oru_plus_res_atoms",
            "oru": 50_0000_0000,
            "res": 40_0000_00,
            "sum_atoms": 50_0000_0000 + 40_0000_00,
            "same_monetary_unit": False,
            "consensus_forbids_unfiltered_sum_as_fee": True,
            "legacy_GetValueOut_sums_all_assets": True,
        }
    )
    return rows


def build_vectors() -> dict:
    oru = DualCoin(50_0000_0000, spk_oru(1), 1, True)
    res = DualCoin(1_0000_0000, spk_res(5), 1, False)
    mint = DualCoin(40_0000_00, spk_mint(101, 7), 1, True)
    unknown = DualCoin(1, spk_unknown(), 1, False)
    malformed = DualCoin(1, spk_malformed(), 1, False)
    retired = DualCoin(1, spk_retired_res2(), 1, False)

    oru_key = encode_key(_h("oru-out"), 0)
    res_key = encode_key(_h("res-out"), 0)
    mint_key = encode_key(_h("mint-out"), 1)

    main, alt = golden_dual_chains()
    modes = {}
    for mode in ("clean", "cache_heavy", "restart", "flush_restart"):
        digest, by_asset, supply = run_path(alt, mode)
        modes[mode] = {"digest": digest, "by_asset": by_asset, "supply": supply}

    reorg, clean_alt = run_reorg()
    rebuild_d, rebuild_a, rebuild_s = rebuild_from_blocks(alt)

    oru_raw = oru.encode()
    res_raw = res.encode()
    corrupt = []
    for label, blob, kind in (
        ("oru_empty", b"", "ORU"),
        ("oru_trunc", oru_raw[:2], "ORU"),
        ("res_empty", b"", "RES"),
        ("res_trunc", res_raw[:2], "RES"),
        ("oru_flipped_last", oru_raw[:-1] + bytes([oru_raw[-1] ^ 0xFF]), "ORU"),
        ("res_flipped_last", res_raw[:-1] + bytes([res_raw[-1] ^ 0xFF]), "RES"),
    ):
        outcome = db.classify_decode_outcome("coin", blob)
        corrupt.append(
            {
                "case": label,
                "asset_intent": kind,
                "harness_outcome": outcome,
                "production_read_maps_to_missing": db.production_read_maps_error_to_missing(blob)
                if outcome == "SILENT_MISSING"
                else False,
                "silently_becomes_other_asset": False,
            }
        )

    # Script-swap corruption: decode succeeds, asset identity changes.
    oru_as_res = DualCoin(oru.n_value, mutate_oru_to_res(oru.script), oru.height, oru.coinbase)
    res_as_oru = DualCoin(res.n_value, mutate_res_to_oru(res.script), res.height, res.coinbase)
    corrupt.append(
        {
            "case": "corrupt_oru_script_to_res",
            "asset_intent": "ORU",
            "decoded_asset": oru_as_res.asset(),
            "harness_outcome": "SILENT_RECLASSIFY",
            "silently_becomes_other_asset": oru_as_res.asset() == "RES",
            "fail_closed": False,
        }
    )
    corrupt.append(
        {
            "case": "corrupt_res_script_to_oru",
            "asset_intent": "RES",
            "decoded_asset": res_as_oru.asset(),
            "harness_outcome": "SILENT_RECLASSIFY",
            "silently_becomes_other_asset": res_as_oru.asset() == "ORU",
            "fail_closed": False,
        }
    )

    mode_digests = {k: v["digest"] for k, v in modes.items()}
    oru_eq = len({v["by_asset"]["ORU"] for v in modes.values()}) == 1
    res_eq = len({v["by_asset"]["RES"] for v in modes.values()}) == 1
    supply_eq = all(v["supply"] == modes["clean"]["supply"] for v in modes.values())

    return {
        "task": "",
        "note": "Locks serialization, reconstruction, and the failure boundary. Not a formal proof.",
        "oru_res_storage_model": "SHARED_COIN_MODEL",
        "res_persistence_class": "C_HYBRID",
        "res_persistence_class_note": (
            "Ordinary RES (WITNESS_V1_RES) is a Coin. RES mint is a Coin whose "
            "script is RIGOR_MARKER_RES_MINT. Draw / convergence are not Coin fields. "
            "Mint-time maturity_height lives in the script; spend-time maturity uses "
            "Coin.nHeight + fCoinBase."
        ),
        "canonical": {
            "coin_layout": "VARINT((coinbase?1:0)|(height<<1)) || VARINT(CompressAmount) || ScriptCompression",
            "key_layout": "0x43 || txid32 || VARINT(vout)",
            "asset_field_in_coin": False,
            "asset_in_key": False,
            "asset_source": "scriptPubKey via ClassifyRigorTxout after load",
            "oru_key_prefix": "0x43",
            "res_key_prefix": "0x43",
            "separate_column": False,
            "separate_database": False,
            "separate_state_table": False,
        },
        "vectors": {
            "01_oru_only_coin": {
                "value_hex": oru.encode().hex(),
                "key_hex": oru_key.hex(),
                "asset": oru.asset(),
            },
            "02_res_only_coin": {
                "value_hex": res.encode().hex(),
                "key_hex": res_key.hex(),
                "asset": res.asset(),
            },
            "03_oru_spend": {"note": "spent coins are erased; see chain digest"},
            "04_res_spend": {"note": "spent coins are erased; see chain digest"},
            "05_oru_res_same_block": {"block": "A", "contains": ["ORU coinbase", "RES mint", "ORU transfer", "RES transfer output"]},
            "06_oru_res_same_transaction": {
                "allowed_by_protocol": "dual-pool RES spend with ORU fee inputs",
                "harness_tx": "C mix tx persists both outputs",
            },
            "07_res_mint": {
                "value_hex": mint.encode().hex(),
                "key_hex": mint_key.hex(),
                "asset": mint.asset(),
                "coinbase": True,
            },
            "08_res_maturity": {
                "spend_height_100_coin_1": coinbase_mature(100, 1),
                "spend_height_101_coin_1": coinbase_mature(101, 1),
                "nCoinbaseMaturity": N_COINBASE_MATURITY,
                "script_maturity_checked_at_mint_only": True,
                "spend_uses_coinbase_flag_and_height": True,
            },
            "09_reorg_oru": {"covered_by": "reorg_recovery.by_asset.ORU"},
            "10_reorg_res": {"covered_by": "reorg_recovery.by_asset.RES"},
            "11_restart_oru": {"covered_by": "cache_db_equivalence.by_asset.ORU"},
            "12_restart_res": {"covered_by": "cache_db_equivalence.by_asset.RES"},
            "13_reindex_oru": {"harness": True, "production_reindex": False},
            "14_reindex_res": {"harness": True, "production_reindex": False},
            "15_corrupted_oru": {"see": "corruption oru_*"},
            "16_corrupted_res": {"see": "corruption res_*"},
            "17_unknown_asset": {
                "script_hex": unknown.script.hex(),
                "asset": unknown.asset(),
                "fail_closed_classify": unknown.asset() == "INVALID",
            },
            "18_malformed_asset": {
                "script_hex": malformed.script.hex(),
                "asset": malformed.asset(),
                "retired_res2_asset": retired.asset(),
                "fail_closed_classify": malformed.asset() == "INVALID" and retired.asset() == "INVALID",
            },
            "19_oru_res_mutation": {"see": "mutation"},
        },
        "keys": {
            "oru_key_hex": oru_key.hex(),
            "res_key_hex": res_key.hex(),
            "shared_schema": same_key_schema(oru_key, res_key),
            "asset_distinguishes_key": False,
            "collision_only_if_same_outpoint": True,
        },
        "cache_db_equivalence": {
            "path_digests": mode_digests,
            "equivalent": len(set(mode_digests.values())) == 1,
            "oru_equivalent": oru_eq,
            "res_equivalent": res_eq,
            "supply_equivalent": supply_eq,
            "by_asset": {k: v["by_asset"] for k, v in modes.items()},
            "supply": {k: v["supply"] for k, v in modes.items()},
        },
        "reorg_recovery": {
            "via_reorg": reorg,
            "via_clean_alt": clean_alt,
            "equivalent": reorg["digest"] == clean_alt["digest"],
            "oru_equivalent": reorg["by_asset"]["ORU"] == clean_alt["by_asset"]["ORU"],
            "res_equivalent": reorg["by_asset"]["RES"] == clean_alt["by_asset"]["RES"],
            "supply_equivalent": reorg["supply"] == clean_alt["supply"],
        },
        "rebuild_equivalence": {
            "from_block_storage_replay": rebuild_d,
            "from_live_alt": modes["clean"]["digest"],
            "consensus_state_equivalent": rebuild_d == modes["clean"]["digest"],
            "oru_equivalent": rebuild_a["ORU"] == modes["clean"]["by_asset"]["ORU"],
            "res_equivalent": rebuild_a["RES"] == modes["clean"]["by_asset"]["RES"],
            "supply_equivalent": rebuild_s == modes["clean"]["supply"],
            "production_reindex_executed": False,
        },
        "mutation": mutation_cases(),
        "amounts": amount_cases(),
        "corruption": corrupt,
        "production_failure_boundary": {
            "malformed_value": "Read catch → missing → TX_CONSENSUS",
            "script_mutation_reclassifies_asset": True,
            "database_fail_closed_for_value_corruption": False,
            "oru_corruption_fail_closed": False,
            "res_corruption_fail_closed": False,
        },
    }


def verify_vectors(doc: dict) -> List[str]:
    errors: List[str] = []
    fresh = build_vectors()
    if fresh["oru_res_storage_model"] != "SHARED_COIN_MODEL":
        errors.append("storage model drift")
    for key in ("01_oru_only_coin", "02_res_only_coin", "07_res_mint"):
        if fresh["vectors"][key]["value_hex"] != doc["vectors"][key]["value_hex"]:
            errors.append(f"vector drift: {key}")
        if fresh["vectors"][key]["asset"] != doc["vectors"][key]["asset"]:
            errors.append(f"asset drift: {key}")
    raw_oru = bytes.fromhex(doc["vectors"]["01_oru_only_coin"]["value_hex"])
    raw_res = bytes.fromhex(doc["vectors"]["02_res_only_coin"]["value_hex"])
    coin_o, _ = decode_dual(raw_oru)
    coin_r, _ = decode_dual(raw_res)
    if coin_o.asset() != "ORU":
        errors.append("ORU vector does not classify as ORU")
    if coin_r.asset() != "RES":
        errors.append("RES vector does not classify as RES")
    if coin_o.asset() == coin_r.asset():
        errors.append("ORU and RES vectors share asset identity")
    if not doc["cache_db_equivalence"]["equivalent"]:
        errors.append("CACHE_DB_EQUIVALENCE failed in stored vectors")
    if not fresh["cache_db_equivalence"]["equivalent"]:
        errors.append("CACHE_DB_EQUIVALENCE failed on recomputation")
    if fresh["cache_db_equivalence"]["path_digests"] != doc["cache_db_equivalence"]["path_digests"]:
        errors.append("cache path digest drift")
    if not fresh["reorg_recovery"]["equivalent"]:
        errors.append("REORG digest mismatch")
    if not fresh["reorg_recovery"]["oru_equivalent"] or not fresh["reorg_recovery"]["res_equivalent"]:
        errors.append("REORG per-asset mismatch")
    if not fresh["rebuild_equivalence"]["consensus_state_equivalent"]:
        errors.append("REBUILD mismatch")
    if not doc["vectors"]["17_unknown_asset"]["fail_closed_classify"]:
        errors.append("unknown asset must classify INVALID")
    if not doc["vectors"]["18_malformed_asset"]["fail_closed_classify"]:
        errors.append("malformed / retired must classify INVALID")
    reclass = [m for m in doc["mutation"] if m.get("reclassified")]
    if len(reclass) < 2:
        errors.append("expected ORU↔RES script-mutation reclassification cases")
    if not doc["production_failure_boundary"]["script_mutation_reclassifies_asset"]:
        errors.append("must keep script-mutation reclassification recorded")
    if doc["production_failure_boundary"]["oru_corruption_fail_closed"]:
        errors.append("do not mark ORU fail-closed while deserialize maps to missing")
    if doc["production_failure_boundary"]["res_corruption_fail_closed"]:
        errors.append("do not mark RES fail-closed while deserialize maps to missing")
    return errors


def write_vectors() -> dict:
    doc = build_vectors()
    VECTOR_DIR.mkdir(parents=True, exist_ok=True)
    CATALOG.write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")
    # Per-vector shards required by the TASK listing.
    shards = {
        "01_oru_only_coin.json": doc["vectors"]["01_oru_only_coin"],
        "02_res_only_coin.json": doc["vectors"]["02_res_only_coin"],
        "07_res_mint.json": doc["vectors"]["07_res_mint"],
        "17_unknown_asset.json": doc["vectors"]["17_unknown_asset"],
        "18_malformed_asset.json": doc["vectors"]["18_malformed_asset"],
        "19_oru_res_mutation.json": doc["mutation"],
        "corruption.json": doc["corruption"],
        "reorg.json": doc["reorg_recovery"],
        "rebuild.json": doc["rebuild_equivalence"],
    }
    for name, payload in shards.items():
        (VECTOR_DIR / name).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    return doc


def main(argv: List[str]) -> int:
    cmd = argv[1] if len(argv) > 1 else "verify"
    if cmd == "write":
        doc = write_vectors()
        print(f"wrote {CATALOG}")
        print(f"ORU_RES_STORAGE_MODEL={doc['oru_res_storage_model']}")
        print(f"ORU_CACHE_DB_EQUIVALENCE={'PASS' if doc['cache_db_equivalence']['oru_equivalent'] else 'FAIL'}")
        print(f"RES_CACHE_DB_EQUIVALENCE={'PASS' if doc['cache_db_equivalence']['res_equivalent'] else 'FAIL'}")
        print(f"ORU_REORG_RECOVERY={'PASS' if doc['reorg_recovery']['oru_equivalent'] else 'FAIL'}")
        print(f"RES_REORG_RECOVERY={'PASS' if doc['reorg_recovery']['res_equivalent'] else 'FAIL'}")
        return 0
    if cmd == "verify":
        if not CATALOG.is_file():
            print(f"FAIL: missing {CATALOG}", file=sys.stderr)
            return 1
        doc = json.loads(CATALOG.read_text(encoding="utf-8"))
        errors = verify_vectors(doc)
        if errors:
            for e in errors:
                print(f"FAIL: {e}", file=sys.stderr)
            return 1
        print("DUAL_ASSET_GOLDEN_VECTORS=PASS")
        print(f"ORU_RES_STORAGE_MODEL={doc['oru_res_storage_model']}")
        print(f"ORU_CACHE_DB_EQUIVALENCE={'PASS' if doc['cache_db_equivalence']['oru_equivalent'] else 'FAIL'}")
        print(f"RES_CACHE_DB_EQUIVALENCE={'PASS' if doc['cache_db_equivalence']['res_equivalent'] else 'FAIL'}")
        print(f"ORU_REORG_RECOVERY={'PASS' if doc['reorg_recovery']['oru_equivalent'] else 'FAIL'}")
        print(f"RES_REORG_RECOVERY={'PASS' if doc['reorg_recovery']['res_equivalent'] else 'FAIL'}")
        print(f"ORU_REBUILD_EQUIVALENCE={'PASS' if doc['rebuild_equivalence']['oru_equivalent'] else 'PARTIAL'}")
        print(f"RES_REBUILD_EQUIVALENCE={'PASS' if doc['rebuild_equivalence']['res_equivalent'] else 'PARTIAL'}")
        print("ORU_CORRUPTION_FAIL_CLOSED=FALSE")
        print("RES_CORRUPTION_FAIL_CLOSED=FALSE")
        print("SCRIPT_MUTATION_RECLASSIFIES_ASSET=TRUE")
        return 0
    print("usage: dual_asset_database_codec.py [write|verify]", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
