#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P0-088: parse production dumptxoutset dumps and compare UTXO fields.

This is not a Consensus Kernel and not a second CoinsDB. It reads the
production snapshot format written by CreateUTXOSnapshot:

    SnapshotMetadata { uint256 base_hash, uint64 coins_count }
    repeated { COutPoint (txid32 || uint32 n), Coin::Serialize leaf }

Leaf encoding has no CRC trailer (CRC is disk/undo only). Asset identity is
inferred from scriptPubKey by the same script-as-asset rules as
ClassifyRigorTxout. Spend maturity is coinbase+height vs nCoinbaseMaturity,
not the mint-script encoded height.
"""

from __future__ import annotations

import hashlib
import json
import sys
from copy import deepcopy
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Tuple

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import database_state_codec as db  # noqa: E402
import dual_asset_database_codec as da  # noqa: E402

N_COINBASE_MATURITY = 100
MARKER_MINT = db.RIGOR_MARKER_RES_MINT
MARKER_CONVERGE = db.RIGOR_MARKER_CONVERGE

COMPARE_FIELDS = (
    "membership",
    "asset",
    "amount",
    "height",
    "coinbase",
    "maturity",
    "script",
    "mint",
)


def txid_rpc_hex(raw32: bytes) -> str:
    if len(raw32) != 32:
        raise ValueError("txid must be 32 bytes")
    return raw32[::-1].hex()


def encode_scriptnum(n: int) -> bytes:
    if n == 0:
        return bytes([0x00])
    if 1 <= n <= 16:
        return bytes([0x50 + n])
    if n < 0:
        raise ValueError("mint maturity is non-negative")
    raw = bytearray()
    v = n
    while v:
        raw.append(v & 0xFF)
        v >>= 8
    if raw[-1] & 0x80:
        raw.append(0x00)
    return bytes([len(raw)]) + bytes(raw)


def get_res_mint_script(maturity_height: int, pkh20: bytes) -> bytes:
    if len(pkh20) != 20:
        raise ValueError("pkh must be 20 bytes")
    return encode_scriptnum(maturity_height) + bytes([20]) + pkh20 + bytes([MARKER_MINT])


def parse_res_mint(script: bytes) -> Optional[Dict[str, Any]]:
    """Replica of ParseResMintScript / IsResMintScript (exact template)."""
    if not script or script[-1] != MARKER_MINT:
        return None
    pos = 0
    if pos >= len(script):
        return None
    first = script[pos]
    pos += 1
    maturity = 0
    if first == 0x00:
        maturity = 0
    elif 0x51 <= first <= 0x60:
        maturity = first - 0x50
    elif first <= 75:
        n = first
        if pos + n > len(script):
            return None
        data = script[pos : pos + n]
        pos += n
        if not data:
            return None
        maturity = int.from_bytes(data, "little")
        if data[-1] & 0x80:
            return None
    else:
        return None
    if pos >= len(script) or script[pos] != 20:
        return None
    pos += 1
    if pos + 20 > len(script):
        return None
    pkh = script[pos : pos + 20]
    pos += 20
    if pos >= len(script) or script[pos] != MARKER_MINT:
        return None
    pos += 1
    if pos != len(script):
        return None
    if script != get_res_mint_script(maturity, pkh):
        return None
    return {"encoded_maturity_height": maturity, "payout_hash160": pkh.hex()}


def is_convergence_script(script: bytes) -> bool:
    return (
        len(script) == 22
        and script[0] == 20
        and script[21] == MARKER_CONVERGE
    )


def classify_asset(script: bytes) -> str:
    """Same fail-closed mapping as dual_asset_database_codec / ClassifyRigorTxout."""
    return da.classify_asset(script)


def coinbase_mature(tip_height: int, coin_height: int, coinbase: bool) -> bool:
    if not coinbase:
        return True
    return (tip_height - coin_height) >= N_COINBASE_MATURITY


def read_varint64(buf: bytes, pos: int) -> Tuple[int, int]:
    """Bitcoin Core ReadVarInt<uint64_t>. AmountCompression uses this width.

    database_state_codec.read_varint is uint32-capped and rejects valid
    CompressAmount encodings for large ORU UTXOs.
    """
    n = 0
    while True:
        if pos >= len(buf):
            raise db.CodecError("VARINT unexpected EOF")
        ch = buf[pos]
        pos += 1
        if n > (0xFFFFFFFFFFFFFFFF >> 7):
            raise db.CodecError("VARINT too large")
        n = (n << 7) | (ch & 0x7F)
        if ch & 0x80:
            if n == 0xFFFFFFFFFFFFFFFF:
                raise db.CodecError("VARINT overflow")
            n += 1
        else:
            return n, pos


def decode_coin_leaf(buf: bytes, pos: int) -> Tuple[db.Coin, int]:
    code, pos = read_varint64(buf, pos)
    height = code >> 1
    if height > 0xFFFFFFFF:
        raise db.CodecError("Coin height exceeds uint32")
    coinbase = bool(code & 1)
    camt, pos = read_varint64(buf, pos)
    n_value = db.decompress_amount(camt)
    if n_value > db.MAX_MONEY:
        raise db.CodecError("AmountCompression: nValue out of MoneyRange")
    script, pos, _ = db.decompress_script(buf, pos)
    return db.Coin(n_value, script, int(height), coinbase), pos


def parse_dumptxoutset(path: Path) -> Dict[str, Any]:
    raw = path.read_bytes()
    if len(raw) < 40:
        raise db.CodecError("snapshot too short")
    base_hash = raw[0:32]
    coins_count = int.from_bytes(raw[32:40], "little")
    pos = 40
    coins: List[Dict[str, Any]] = []
    while pos < len(raw):
        if pos + 36 > len(raw):
            raise db.CodecError("truncated COutPoint")
        txid_raw = raw[pos : pos + 32]
        pos += 32
        n = int.from_bytes(raw[pos : pos + 4], "little")
        pos += 4
        coin, pos = decode_coin_leaf(raw, pos)
        script = bytes(coin.script_pubkey)
        mint_meta = parse_res_mint(script)
        asset = classify_asset(script)
        coins.append(
            {
                "txid": txid_rpc_hex(txid_raw),
                "vout": n,
                "membership": True,
                "asset": asset,
                "amount": int(coin.n_value),
                "height": int(coin.height),
                "coinbase": bool(coin.coinbase),
                "script": script.hex(),
                "mint": mint_meta is not None,
                "convergence": is_convergence_script(script),
                "encoded_maturity_height": None if mint_meta is None else mint_meta["encoded_maturity_height"],
                "payout_hash160": None if mint_meta is None else mint_meta["payout_hash160"],
            }
        )
    if len(coins) != coins_count:
        raise db.CodecError(f"coins_count metadata {coins_count} != parsed {len(coins)}")
    return {
        "base_hash": txid_rpc_hex(base_hash),
        "coins_count": coins_count,
        "coins": coins,
    }


def finalize_records(coins: Iterable[Dict[str, Any]], tip_height: int, extra: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    rows = []
    for c in coins:
        rec = dict(c)
        rec["maturity"] = coinbase_mature(tip_height, int(rec["height"]), bool(rec["coinbase"]))
        rec["spendable"] = rec["maturity"]
        rows.append(rec)
    rows.sort(key=lambda r: (r["txid"], r["vout"]))
    oru = [r for r in rows if r["asset"] == "ORU"]
    res = [r for r in rows if r["asset"] == "RES"]
    mint = [r for r in rows if r["mint"]]
    ordinary_res = [r for r in rows if r["asset"] == "RES" and not r["mint"]]
    conv = [r for r in rows if r["convergence"]]
    invalid = [r for r in rows if r["asset"] == "INVALID"]
    doc = {
        "tip_height": tip_height,
        "nCoinbaseMaturity": N_COINBASE_MATURITY,
        "utxo_count": len(rows),
        "oru_count": len(oru),
        "res_count": len(res),
        "mint_count": len(mint),
        "ordinary_res_count": len(ordinary_res),
        "convergence_in_utxo_count": len(conv),
        "invalid_count": len(invalid),
        "oru_supply": sum(r["amount"] for r in oru),
        "res_supply": sum(r["amount"] for r in res),
        "immature_res_supply": sum(r["amount"] for r in res if not r["maturity"]),
        "coins": rows,
    }
    if extra:
        doc.update(extra)
    doc["digest"] = state_digest(doc)
    return doc


def canonical_coin_tuple(rec: Dict[str, Any]) -> Tuple:
    return (
        rec["txid"],
        rec["vout"],
        rec["membership"],
        rec["asset"],
        rec["amount"],
        rec["height"],
        rec["coinbase"],
        rec["maturity"],
        rec["script"],
        rec["mint"],
    )


def state_digest(doc: Dict[str, Any]) -> str:
    h = hashlib.sha256()
    h.update(str(doc.get("tip_height", 0)).encode())
    for rec in doc.get("coins", []):
        h.update(repr(canonical_coin_tuple(rec)).encode())
    return h.hexdigest()


def compare_states(live: Dict[str, Any], rebuilt: Dict[str, Any], label: str) -> Dict[str, Any]:
    a = {(c["txid"], c["vout"]): c for c in live["coins"]}
    b = {(c["txid"], c["vout"]): c for c in rebuilt["coins"]}
    missing = sorted(set(a) - set(b))
    extra = sorted(set(b) - set(a))
    mismatches: List[Dict[str, Any]] = []
    field_counts = {f: 0 for f in COMPARE_FIELDS}
    asset_mismatch = 0
    mint_mismatch = 0
    amount_mismatch = 0
    height_mismatch = 0
    coinbase_mismatch = 0
    script_mismatch = 0
    for k in sorted(set(a) & set(b)):
        la, rb = a[k], b[k]
        for field in COMPARE_FIELDS:
            if la.get(field) != rb.get(field):
                field_counts[field] += 1
                mismatches.append(
                    {
                        "outpoint": f"{k[0]}:{k[1]}",
                        "field_name": field,
                        "live_value": la.get(field),
                        "rebuild_value": rb.get(field),
                        "classification": "REBUILD",
                    }
                )
                if field == "asset":
                    asset_mismatch += 1
                elif field == "mint":
                    mint_mismatch += 1
                elif field == "amount":
                    amount_mismatch += 1
                elif field == "height":
                    height_mismatch += 1
                elif field == "coinbase":
                    coinbase_mismatch += 1
                elif field == "script":
                    script_mismatch += 1
    oru_ok = [a[k]["asset"] == "ORU" for k in set(a) & set(b) if a[k]["asset"] == "ORU" or b[k]["asset"] == "ORU"]
    res_ok_keys = [k for k in set(a) | set(b) if a.get(k, b.get(k, {})).get("asset") == "RES" or b.get(k, {}).get("asset") == "RES"]
    conv_live = live.get("convergence_in_utxo_count", 0)
    conv_reb = rebuilt.get("convergence_in_utxo_count", 0)
    field_mismatch_count = len(mismatches)
    result = {
        "label": label,
        "missing": [{"outpoint": f"{t}:{n}"} for t, n in missing],
        "extra": [{"outpoint": f"{t}:{n}"} for t, n in extra],
        "mismatches": mismatches,
        "PRODUCTION_REBUILD_MISSING_COUNT": len(missing),
        "PRODUCTION_REBUILD_EXTRA_COUNT": len(extra),
        "PRODUCTION_REBUILD_FIELD_MISMATCH_COUNT": field_mismatch_count,
        "PRODUCTION_REBUILD_SCRIPT_MISMATCH_COUNT": script_mismatch,
        "PRODUCTION_REBUILD_AMOUNT_MISMATCH_COUNT": amount_mismatch,
        "PRODUCTION_REBUILD_HEIGHT_MISMATCH_COUNT": height_mismatch,
        "PRODUCTION_REBUILD_COINBASE_MISMATCH_COUNT": coinbase_mismatch,
        "PRODUCTION_REBUILD_ASSET_MISMATCH_COUNT": asset_mismatch,
        "PRODUCTION_REBUILD_MINT_MISMATCH_COUNT": mint_mismatch,
        "PRODUCTION_REBUILD_DIVERGENCE_COUNT": len(missing) + len(extra) + field_mismatch_count,
        "convergence_utxo_live": conv_live,
        "convergence_utxo_rebuilt": conv_reb,
        "oru_supply_live": live.get("oru_supply"),
        "oru_supply_rebuilt": rebuilt.get("oru_supply"),
        "res_supply_live": live.get("res_supply"),
        "res_supply_rebuilt": rebuilt.get("res_supply"),
        "mint_count_live": live.get("mint_count"),
        "mint_count_rebuilt": rebuilt.get("mint_count"),
        "digest_live": live.get("digest"),
        "digest_rebuilt": rebuilt.get("digest"),
        "equivalent": len(missing) == 0 and len(extra) == 0 and field_mismatch_count == 0 and conv_live == 0 and conv_reb == 0,
    }
    result["ORU_EQUIVALENT"] = (
        result["equivalent"] and live.get("oru_supply") == rebuilt.get("oru_supply") and live.get("oru_count") == rebuilt.get("oru_count")
    )
    result["RES_EQUIVALENT"] = (
        result["equivalent"] and live.get("res_supply") == rebuilt.get("res_supply") and live.get("res_count") == rebuilt.get("res_count")
    )
    result["MINT_EQUIVALENT"] = live.get("mint_count") == rebuilt.get("mint_count") and mint_mismatch == 0 and not any(
        a.get(k, {}).get("mint") != b.get(k, {}).get("mint") for k in set(a) | set(b)
    )
    # Tighten mint: every mint outpoint must match including encoded maturity / payout.
    mint_field_ok = True
    for k in set(a) | set(b):
        la, rb = a.get(k), b.get(k)
        if not la or not rb:
            if (la and la.get("mint")) or (rb and rb.get("mint")):
                mint_field_ok = False
            continue
        if la.get("mint") or rb.get("mint"):
            if (
                la.get("mint") != rb.get("mint")
                or la.get("encoded_maturity_height") != rb.get("encoded_maturity_height")
                or la.get("payout_hash160") != rb.get("payout_hash160")
                or la.get("script") != rb.get("script")
                or la.get("coinbase") != rb.get("coinbase")
                or la.get("height") != rb.get("height")
            ):
                mint_field_ok = False
    result["MINT_EQUIVALENT"] = bool(result["MINT_EQUIVALENT"] and mint_field_ok)
    result["MATURITY_EQUIVALENT"] = field_counts["maturity"] == 0 and field_counts["height"] == 0 and field_counts["coinbase"] == 0
    result["ASSET_CLASSIFICATION_EQUIVALENT"] = asset_mismatch == 0
    _ = oru_ok, res_ok_keys
    return result


def mutate_selftest(live: Dict[str, Any]) -> None:
    if not live["coins"]:
        raise SystemExit("selftest needs at least one coin")
    base = finalize_records(live["coins"], live["tip_height"])
    cases = []

    def expect(name: str, mutated: Dict[str, Any], kind: str) -> None:
        cmp_ = compare_states(base, mutated, name)
        if kind == "MISMATCH" and cmp_["PRODUCTION_REBUILD_FIELD_MISMATCH_COUNT"] < 1:
            raise SystemExit(f"selftest {name}: expected MISMATCH")
        if kind == "MISSING" and cmp_["PRODUCTION_REBUILD_MISSING_COUNT"] < 1:
            raise SystemExit(f"selftest {name}: expected MISSING")
        if kind == "EXTRA" and cmp_["PRODUCTION_REBUILD_EXTRA_COUNT"] < 1:
            raise SystemExit(f"selftest {name}: expected EXTRA")
        cases.append(name)

    m = deepcopy(base)
    m["coins"][0]["amount"] += 1
    expect("amount", m, "MISMATCH")
    m = deepcopy(base)
    m["coins"][0]["height"] += 1
    m = finalize_records(m["coins"], m["tip_height"])
    expect("height", m, "MISMATCH")
    m = deepcopy(base)
    m["coins"][0]["coinbase"] = not m["coins"][0]["coinbase"]
    m = finalize_records(m["coins"], m["tip_height"])
    expect("coinbase", m, "MISMATCH")
    m = deepcopy(base)
    m["coins"][0]["script"] = "00" + m["coins"][0]["script"]
    expect("script", m, "MISMATCH")
    m = deepcopy(base)
    m["coins"][0]["asset"] = "RES" if m["coins"][0]["asset"] != "RES" else "ORU"
    expect("asset", m, "MISMATCH")
    m = deepcopy(base)
    m["coins"][0]["mint"] = not m["coins"][0]["mint"]
    expect("mint", m, "MISMATCH")
    m = deepcopy(base)
    m["coins"] = m["coins"][1:]
    expect("delete", m, "MISSING")
    m = deepcopy(base)
    extra = deepcopy(m["coins"][0])
    extra["vout"] = extra["vout"] + 97
    extra["txid"] = "ab" * 32
    m["coins"] = m["coins"] + [extra]
    expect("extra", m, "EXTRA")
    print("COMPARATOR_MUTATION_SELFTEST=PASS")
    print("MUTATION_CASES=" + ",".join(cases))


def compare_cache_to_dump(cache: Dict[str, Any], dump: Dict[str, Any]) -> Dict[str, Any]:
    """Field compare CoinsTip (gettxout) vs CoinsDB dump.

    Amount is not taken from gettxout (RES Decimal scale is observational).
    Membership, asset, height, coinbase, script, and mint are required.
    """
    cache_fields = {
        "membership",
        "asset",
        "height",
        "coinbase",
        "maturity",
        "script",
        "mint",
    }
    a = {(c["txid"], c["vout"]): c for c in cache["coins"]}
    b = {(c["txid"], c["vout"]): c for c in dump["coins"]}
    missing = sorted(set(b) - set(a))  # in DB dump, absent from cache walk
    extra = sorted(set(a) - set(b))  # in cache, absent from DB dump
    mismatches: List[Dict[str, Any]] = []
    for k in sorted(set(a) & set(b)):
        la, rb = a[k], b[k]
        for field in cache_fields:
            if la.get(field) != rb.get(field):
                mismatches.append(
                    {
                        "outpoint": f"{k[0]}:{k[1]}",
                        "field_name": field,
                        "live_value": la.get(field),
                        "rebuild_value": rb.get(field),
                        "classification": "OBSERVATIONAL" if field in ("maturity",) else "REBUILD",
                    }
                )
    return {
        "label": "CACHE_VS_DB",
        "PRODUCTION_REBUILD_MISSING_COUNT": len(missing),
        "PRODUCTION_REBUILD_EXTRA_COUNT": len(extra),
        "PRODUCTION_REBUILD_FIELD_MISMATCH_COUNT": len(mismatches),
        "missing": [{"outpoint": f"{t}:{n}"} for t, n in missing],
        "extra": [{"outpoint": f"{t}:{n}"} for t, n in extra],
        "mismatches": mismatches,
        "equivalent": len(missing) == 0 and len(extra) == 0 and len(mismatches) == 0,
    }


def write_json(path: Path, doc: Dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(doc, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main(argv: List[str]) -> int:
    if len(argv) >= 2 and argv[1] == "selftest":
        sample = finalize_records(
            [
                {
                    "txid": "11" * 32,
                    "vout": 0,
                    "membership": True,
                    "asset": "ORU",
                    "amount": 50_0000_0000,
                    "height": 1,
                    "coinbase": True,
                    "script": "0014" + "02" * 20,
                    "mint": False,
                    "convergence": False,
                    "encoded_maturity_height": None,
                    "payout_hash160": None,
                }
            ],
            tip_height=101,
        )
        mutate_selftest(sample)
        oru = bytes.fromhex("0014" + "03" * 20)
        res = bytes.fromhex("5120" + "04" * 32)
        if classify_asset(oru) != "ORU" or classify_asset(res) != "RES":
            raise SystemExit("classifier selftest failed")
        if classify_asset(bytes.fromhex("76a914" + "00" * 20 + "88ac")) != "INVALID":
            raise SystemExit("unknown must be INVALID not ORU")
        if classify_asset(b"") != "INVALID":
            raise SystemExit("empty must be INVALID")
        print("ASSET_CLASSIFIER_UNKNOWN_IS_NOT_ORU=PASS")
        return 0
    print("usage: production_reindex_state.py selftest")
    return 2


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
