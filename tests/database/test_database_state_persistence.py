#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P0-086 database / CoinsDB unit tests (audit harness, not a formal proof)."""

from __future__ import annotations

import random
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import database_state_codec as db  # noqa: E402


def test_canonical_key_and_disk_layout() -> None:
    coin = db.Coin(50_0000_0000, db._spk_oru(1), 1, True)
    raw = db.encode_coin(coin)
    leaf_len = len(db.write_varint((1 << 1) | 1) + db.write_varint(db.compress_amount(coin.n_value)) + db.compress_script(coin.script_pubkey))
    assert len(raw) == leaf_len + 4
    decoded, _ = db.decode_coin(raw)
    assert decoded.n_value == coin.n_value
    assert decoded.height == 1 and decoded.coinbase
    assert decoded.script_pubkey == coin.script_pubkey
    assert db.encode_undo(coin) == raw
    key = db.encode_coin_key(db._h("genesis-cb"), 0)
    assert key[0] == db.DB_COIN
    txid, n = db.decode_coin_key(key)
    assert txid == db._h("genesis-cb") and n == 0


def test_spent_is_erased_not_stored() -> None:
    view = db.CoinsView()
    cache = db.CoinsViewCache(view)
    outp = (db._h("a"), 0)
    cache.add_coin(outp, db.Coin(1, db._spk_oru(1), 1, False), possible_overwrite=False)
    cache.flush()
    assert view.get(outp) is not None
    cache.spend_coin(outp)
    cache.flush()
    assert view.get(outp) is None


def test_convergence_never_enters_utxo() -> None:
    view = db.CoinsView()
    cache = db.CoinsViewCache(view)
    cache.add_coin((db._h("c"), 0), db.Coin(9, db._spk_converge(1), 1, False), possible_overwrite=False)
    cache.flush()
    assert view.coins == {}


def test_fail_closed_corruption() -> None:
    good = db.encode_coin(db.Coin(50, db._spk_oru(2), 4, True))
    assert db.classify_decode_outcome("coin", good) == "ACCEPTED_VALID"
    for label, blob in db.corruption_cases(good):
        if label == "wrong_type_key":
            continue
        assert db.classify_decode_outcome("coin", blob) == "FAIL_CLOSED", label
    tampered = bytearray(good)
    tampered[0] ^= 0x01
    assert db.classify_decode_outcome("coin", bytes(tampered)) == "FAIL_CLOSED"
    assert db.production_read_maps_error_to_missing(b"\xff") is False


def test_cache_db_equivalence() -> None:
    _, alt = db.golden_chain()
    digests = {mode: db.run_path(alt, mode) for mode in ("clean", "cache_heavy", "restart", "flush_restart")}
    assert len(set(digests.values())) == 1


def test_reorg_and_rebuild() -> None:
    reorg, clean = db.run_reorg_equivalence()
    assert reorg == clean
    _, alt = db.golden_chain()
    assert db.rebuild_from_blocks(alt) == db.run_path(alt, "clean")


def test_crash_before_flush_replays_from_blocks() -> None:
    _, alt = db.golden_chain()
    db_live = db.CoinsView()
    cache = db.CoinsViewCache(db_live)
    for block in alt[:-1]:
        db.apply_block(cache, block)
        cache.flush()
    durable = cache.base.state_digest()
    db.apply_block(cache, alt[-1])
    # Crash before flush: cache is lost.
    restarted = db.CoinsViewCache(
        db.CoinsView.from_serialized(cache.base.serialize_all(), cache.base.best_block)
    )
    assert restarted.base.state_digest() == durable
    db.apply_block(restarted, alt[-1])
    restarted.flush()
    assert restarted.base.state_digest() == db.run_path(alt, "clean")


def test_random_block_sequence_rebuild() -> None:
    rng = random.Random(0x086)
    db_live = db.CoinsView()
    cache = db.CoinsViewCache(db_live)
    prev = (db._h("rand-g"), 0)
    genesis = db.Block(
        db._h("rand-g-block"),
        0,
        [db.Tx(prev[0], [], [db.TxOut(50_0000_0000, db._spk_oru(1))], coinbase=True)],
    )
    blocks = [genesis]
    db.apply_block(cache, genesis)
    cache.flush()
    for height in range(1, 8):
        cb = db._h(f"rand-cb-{height}")
        spend = db._h(f"rand-sp-{height}")
        vouts = [db.TxOut(40_0000_0000, db._spk_oru(height + 3))]
        if rng.randrange(3) == 0:
            vouts.append(db.TxOut(1_0000_0000, db._spk_res(height)))
        if rng.randrange(4) == 0:
            vouts.append(db.TxOut(1, db._spk_converge(height)))
        block = db.Block(
            db._h(f"rand-b-{height}"),
            height,
            [
                db.Tx(cb, [], [db.TxOut(50_0000_0000, db._spk_oru(height + 20))], coinbase=True),
                db.Tx(spend, [prev], vouts),
            ],
        )
        blocks.append(block)
        db.apply_block(cache, block)
        if rng.choice((True, False)):
            cache.flush()
        prev = (spend, 0)
    cache.flush()
    live = cache.base.state_digest()
    rebuilt = db.rebuild_from_blocks(blocks)
    restart = db.run_path(blocks, "restart")
    assert live == rebuilt == restart


def test_amount_overflow_fail_closed() -> None:
    # VARINT of a compressed amount that decompresses above MAX_MONEY.
    code = db.write_varint((1 << 1) | 1)
    huge = db.write_varint(db.compress_amount(db.MAX_MONEY) + 50)
    script = db.compress_script(db._spk_oru(1))
    blob = code + huge + script + (0).to_bytes(4, "little")
    assert db.classify_decode_outcome("coin", blob) == "FAIL_CLOSED"


def main() -> int:
    tests = [
        test_canonical_key_and_disk_layout,
        test_spent_is_erased_not_stored,
        test_convergence_never_enters_utxo,
        test_fail_closed_corruption,
        test_cache_db_equivalence,
        test_reorg_and_rebuild,
        test_crash_before_flush_replays_from_blocks,
        test_random_block_sequence_rebuild,
        test_amount_overflow_fail_closed,
    ]
    failed = 0
    for fn in tests:
        try:
            fn()
            print(f"PASS {fn.__name__}")
        except Exception as exc:  # noqa: BLE001 — audit runner prints the failure
            failed += 1
            print(f"FAIL {fn.__name__}: {exc}")
    if failed:
        print(f"DATABASE_PYTHON_TESTS=FAIL ({failed})")
        return 1
    print("DATABASE_PYTHON_TESTS=PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
