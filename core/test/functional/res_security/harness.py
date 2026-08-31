#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Shared helpers for the pre-mainnet RES window / reorg / monopoly attack suite.

Consensus constants are mirrored from ``consensus/rigor_consensus_lock.h`` for assertions
only. This module does not change protocol rules.
"""

from __future__ import annotations

from decimal import Decimal
from typing import Any, Iterable, Optional

ZERO_TXID = "0" * 64

# Locked with consensus/consensus.h and consensus/rigor_consensus_lock.h.
# RES uses six decimal places (RES_COIN = 1e6). ORU uses eight (COIN = 1e8).
COINBASE_MATURITY = 100  # ORU coinbase *and* coinbase RIGOR_MARKER_RES_MINT; consensus CheckTxInputs depth
RES_COIN = 1_000_000
RES_REWARD_ATOMS = 40 * RES_COIN  # W(0) = 40.000000 RES; epoch-0 tests (devtest << 210_000)
MIN_CONVERGENCE_ORU = Decimal("1.0")
CONVERGENCE_DRAW_WINDOW = 32
CONVERGENCE_DRAW_MIN_TXS_FOR_RES = 1
# 65536 * W * MIN_CONVERGENCE_AMOUNT, in ORU. Guarantees draw_eligible for any σ.
SATURATING_CONVERGENCE_ORU = Decimal(65536 * CONVERGENCE_DRAW_WINDOW)

# Deterministic witness-v1 RES receive used to arm automint on mockable chains.
DEVTEST_MINING_RES_FEE_ADDR = "dvt1pqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqmsufsz"

# prioritisetransaction fee_delta large enough that a 1 ORU-fee tx is never selected.
CENSOR_FEE_DELTA = -10**15

# Expected wins per draw for a 1 ORU ticket: 1 / (65536 * W) ≈ 2^-21.
# Large burns used when a test must observe an actual mint without saturating the wallet.
LARGE_BURN_ORU = Decimal("100000")


def txid_le_bytes(txid_hex: str) -> bytes:
    """RPC txids are big-endian display; the draw tie-break compares raw bytes."""
    return bytes.fromhex(txid_hex)[::-1]


def is_null_txid(txid: Optional[str]) -> bool:
    return not txid or txid == ZERO_TXID


def res_atoms_in_tx_vouts(tx: dict) -> int:
    total = 0
    for o in tx.get("vout", []):
        if str(o.get("asset", "")).upper() != "RES":
            continue
        total += int(Decimal(str(o["value"])) * Decimal(RES_COIN))
    return total


def is_res_mint_vout(o: dict) -> bool:
    """True for the canonical coinbase RIGOR_MARKER_RES_MINT lock (not witness-v1 bootstrap RES)."""
    spk = o.get("scriptPubKey") or {}
    if str(spk.get("type", "")).lower() == "res_mint":
        return True
    hx = str(spk.get("hex", "")).lower()
    return hx.endswith("bb") and str(o.get("asset", "")).upper() == "RES"


def res_mint_atoms_in_tx(tx: dict) -> int:
    total = 0
    for o in tx.get("vout", []):
        if not is_res_mint_vout(o):
            continue
        total += int(Decimal(str(o["value"])) * Decimal(RES_COIN))
    return total


def body_res_mint_rows(node, block_hash: str) -> list[tuple[str, int]]:
    """(txid, res_atoms) for this block's coinbase RIGOR_MARKER_RES_MINT, if present.

    Issuance is coinbase-only. Witness-v1 bootstrap RES on the same coinbase is ignored.
    The name is kept so existing callers keep working.
    """
    blk = node.getblock(block_hash, 2)
    txs = blk.get("tx") or []
    if not txs:
        return []
    cb = txs[0]
    atoms = res_mint_atoms_in_tx(cb)
    if atoms <= 0:
        return []
    return [(cb["txid"], atoms)]


def paid_ticket_txid_from_payout_block(node, mint_txid: str, block_hash: str) -> str:
    """Paid ticket is the parent-draw winner. ``mint_txid`` must be this block's coinbase."""
    blk = node.getblock(block_hash, 1)
    coinbase_txid = blk["tx"][0]
    assert mint_txid == coinbase_txid, (
        f"RES mint {mint_txid} must be the coinbase of {block_hash}, coinbase={coinbase_txid}"
    )
    parent = blk.get("previousblockhash")
    if not parent:
        parent = node.getblockheader(block_hash)["previousblockhash"]
    fr = node.getconvergenceresult(parent)
    return str(fr.get("draw_winner_txid") or ZERO_TXID).lower()


def expected_winner(fr: dict) -> Optional[str]:
    """Recompute argmin σ/f by cross-multiplication (independent of the node's loop)."""
    eligible = [c for c in fr.get("candidates", []) if c.get("draw_eligible")]
    if not eligible:
        return None
    best = None
    for c in eligible:
        sigma = int(c["h"], 16)
        weight = int(Decimal(str(c["convergence_amount"])) * 100_000_000)
        tid_le = txid_le_bytes(c["txid"])
        if best is None:
            best = (sigma, weight, tid_le, c["txid"])
            continue
        b_sigma, b_weight, b_tid_le, _ = best
        lhs, rhs = sigma * b_weight, b_sigma * weight
        if lhs < rhs or (lhs == rhs and tid_le < b_tid_le):
            best = (sigma, weight, tid_le, c["txid"])
    return best[3]


def candidate_txids(fr: dict) -> set[str]:
    return {c["txid"] for c in fr.get("candidates", [])}


def supply_snapshot(node) -> dict[str, Any]:
    snap = node.getreplaysnapshot()
    tip = node.getbestblockhash()
    hdr = node.getblockheader(tip)
    return {
        "height": int(snap["height"]),
        "block_hash": snap["block_hash"],
        "mint_count": int(snap["mint_count"]),
        "res_balance": str(snap["res_balance"]),
        "state_hash": snap["state_hash"],
        "utxo_count": int(snap["utxo_count"]),
        "rigor_res_minted": int(hdr.get("rigor_res_minted", 0)),
    }


def draw_of(node, block_hash: str) -> dict:
    fr = node.getconvergenceresult(block_hash)
    assert fr["hash"] == block_hash, f"getconvergenceresult hash mismatch {fr['hash']} != {block_hash}"
    return fr


def window_self_check(fr: dict, label: str) -> None:
    first, last = int(fr["window_first_height"]), int(fr["window_last_height"])
    assert last == int(fr["height"]), f"[{label}] window_last_height {last} != height {fr['height']}"
    assert last - first + 1 == min(int(fr["window_size"]), last + 1), f"[{label}] window span"
    for c in fr["candidates"]:
        src = int(c["source_height"])
        assert first <= src <= last, f"[{label}] candidate height {src} outside [{first},{last}]"
    assert int(fr["convergence_root_set_size"]) == len(fr["candidates"]), f"[{label}] |F|"
    n = len(fr["candidates"])
    if n == 0:
        assert fr["convergence_root"] == ZERO_TXID, (
            f"[{label}] C must be null when F is empty, got {fr['convergence_root']}"
        )
    expected = expected_winner(fr)
    reported = fr["draw_winner_txid"]
    if expected is None:
        assert is_null_txid(reported), f"[{label}] expected no winner, got {reported}"
    else:
        assert reported == expected, f"[{label}] winner {reported} != recomputed {expected}"


def fail_context(node, case_id: str, extra: Optional[dict] = None) -> str:
    """Human-readable dump printed on assertion failure (heights, txids, supply, F)."""
    try:
        snap = supply_snapshot(node)
        tip = node.getbestblockhash()
        fr = draw_of(node, tip)
        paid = paid_winners(node, from_height=max(0, int(fr["window_first_height"]) - 1))
        bals = node.getbalances()
        lines = [
            f"[{case_id}] FAIL CONTEXT",
            f"  height={snap['height']} tip={snap['block_hash']}",
            f"  mint_count={snap['mint_count']} chain_res={snap['res_balance']}",
            f"  oru trusted={bals['oru']['trusted']} immature={bals['oru']['immature']}",
            f"  res trusted={bals['res']['trusted']} immature={bals['res']['immature']}",
            f"  utxo_count={snap['utxo_count']} state_hash={snap['state_hash']}",
            f"  |F|={fr.get('convergence_root_set_size')} winner={fr.get('draw_winner_txid')}",
            f"  window=[{fr.get('window_first_height')},{fr.get('window_last_height')}] W={fr.get('window_size')}",
            f"  candidates={[c['txid'] for c in fr.get('candidates', [])]}",
            f"  paid_in_walk={paid}",
        ]
        if extra:
            lines.append(f"  extra={extra}")
        return "\n".join(lines)
    except Exception as exc:  # dump must never hide the original assertion
        return f"[{case_id}] FAIL CONTEXT unavailable: {exc!r}; extra={extra}"


def paid_winners(node, from_height: int = 0, to_height: Optional[int] = None) -> dict[str, int]:
    """Map convergence txid → payout height for every non-null draw on the active chain."""
    tip = node.getblockcount()
    end = tip if to_height is None else to_height
    paid: dict[str, int] = {}
    for height in range(from_height, end + 1):
        fr = draw_of(node, node.getblockhash(height))
        winner = fr["draw_winner_txid"]
        if is_null_txid(winner):
            continue
        payout_height = int(fr["payout_height"])
        if winner in paid:
            raise AssertionError(
                f"convergence {winner} paid at height {paid[winner]} and again at {payout_height}"
            )
        paid[winner] = payout_height
    return paid


def assert_no_double_pay(node, from_height: int, to_height: Optional[int], case_id: str) -> dict[str, int]:
    try:
        return paid_winners(node, from_height, to_height)
    except AssertionError as exc:
        raise AssertionError(fail_context(node, case_id, {"from": from_height, "to": to_height}) + f"\n{exc}") from exc


def mint_on_payout_block(node, awarding_hash: str) -> list[tuple[str, int]]:
    """RES mint rows in the block that pays the draw awarded by ``awarding_hash``."""
    awarding = node.getblock(awarding_hash, 1)
    nxt = awarding.get("nextblockhash")
    if not nxt:
        return []
    return body_res_mint_rows(node, nxt)
