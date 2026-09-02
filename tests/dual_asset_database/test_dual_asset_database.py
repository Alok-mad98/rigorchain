#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P0-086B dual-asset database unit tests (audit harness, not a formal proof)."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import dual_asset_database_codec as da  # noqa: E402


def test_shared_key_and_distinct_asset() -> None:
    oru = da.DualCoin(50_0000_0000, da.spk_oru(1), 1, True)
    res = da.DualCoin(1_0000_0000, da.spk_res(2), 1, False)
    assert oru.asset() == "ORU"
    assert res.asset() == "RES"
    k0 = da.encode_key(da._h("x"), 0)
    k1 = da.encode_key(da._h("x"), 1)
    assert k0[0] == k1[0] == 0x43
    assert k0 != k1
    raw_o = oru.encode()
    raw_r = res.encode()
    d_o, _ = da.decode_dual(raw_o)
    d_r, _ = da.decode_dual(raw_r)
    assert d_o.asset() == "ORU" and d_r.asset() == "RES"
    assert d_o.encode() == raw_o and d_r.encode() == raw_r


def test_unknown_and_malformed_fail_closed() -> None:
    assert da.classify_asset(da.spk_unknown()) == "INVALID"
    assert da.classify_asset(da.spk_malformed()) == "INVALID"
    assert da.classify_asset(da.spk_retired_res2()) == "INVALID"
    assert da.classify_asset(b"") == "INVALID"
    assert da.classify_asset(da.spk_mint(101, 1)) == "RES"
    assert da.classify_asset(da.spk_converge(1)) == "ORU"


def test_mutation_reclassifies() -> None:
    oru = da.spk_oru(9)
    res = da.spk_res(9)
    assert da.classify_asset(da.mutate_oru_to_res(oru)) == "RES"
    assert da.classify_asset(da.mutate_res_to_oru(res)) == "ORU"


def test_reorg_and_rebuild() -> None:
    reorg, clean = da.run_reorg()
    assert reorg["digest"] == clean["digest"]
    assert reorg["by_asset"]["ORU"] == clean["by_asset"]["ORU"]
    assert reorg["by_asset"]["RES"] == clean["by_asset"]["RES"]
    assert reorg["supply"] == clean["supply"]
    _, alt = da.golden_dual_chains()
    digest, by_a, supply = da.rebuild_from_blocks(alt)
    live_d, live_a, live_s = da.run_path(alt, "clean")
    assert digest == live_d
    assert by_a == live_a
    assert supply == live_s


def test_cache_modes() -> None:
    _, alt = da.golden_dual_chains()
    rows = [da.run_path(alt, m) for m in ("clean", "cache_heavy", "restart", "flush_restart")]
    assert len({r[0] for r in rows}) == 1
    assert len({r[1]["ORU"] for r in rows}) == 1
    assert len({r[1]["RES"] for r in rows}) == 1


def test_maturity_boundary() -> None:
    assert not da.coinbase_mature(100, 1)
    assert da.coinbase_mature(101, 1)
    assert not da.coinbase_mature(99, 1)


def test_cross_asset_sum_is_not_a_unit() -> None:
    view_supply = {"oru": 50_0000_0000, "res": 40_0000_00}
    atom_sum = view_supply["oru"] + view_supply["res"]
    assert atom_sum == 50_4000_0000
    # Same CAmount type; not the same monetary unit.
    assert view_supply["oru"] != view_supply["res"]


def main() -> int:
    tests = [
        test_shared_key_and_distinct_asset,
        test_unknown_and_malformed_fail_closed,
        test_mutation_reclassifies,
        test_reorg_and_rebuild,
        test_cache_modes,
        test_maturity_boundary,
        test_cross_asset_sum_is_not_a_unit,
    ]
    failed = 0
    for t in tests:
        try:
            t()
            print(f"PASS {t.__name__}")
        except Exception as e:  # noqa: BLE001 — audit harness
            failed += 1
            print(f"FAIL {t.__name__}: {e}", file=sys.stderr)
    if failed:
        print(f"DUAL_ASSET_DATABASE_PYTESTS=FAIL ({failed})", file=sys.stderr)
        return 1
    print("DUAL_ASSET_DATABASE_PYTESTS=PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
