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
# RES and ORU both use eight decimal places (RES_COIN = COIN = 1e8).
COINBASE_MATURITY = 100  # ORU coinbase *and* coinbase RIGOR_MARKER_RES_MINT; consensus CheckTxInputs depth
RES_COIN = 100_000_000
RES_REWARD_ATOMS = 40 * RES_COIN  # historical W(0) atom count used by leftover lottery-era fixtures
MIN_CONVERGENCE_ORU = Decimal("1.0")
# Large OP_CONVERGE burn used by leftover fixtures (not a lottery window).
SATURATING_CONVERGENCE_ORU = Decimal("2097152")

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


def fail_context(node, case_id: str, extra: Optional[dict] = None) -> str:
    """Human-readable dump printed on assertion failure."""
    try:
        snap = supply_snapshot(node)
        bals = node.getbalances()
        lines = [
            f"[{case_id}] FAIL CONTEXT",
            f"  height={snap['height']} tip={snap['block_hash']}",
            f"  mint_count={snap['mint_count']} chain_res={snap['res_balance']}",
            f"  oru trusted={bals['oru']['trusted']} immature={bals['oru']['immature']}",
            f"  res trusted={bals['res']['trusted']} immature={bals['res']['immature']}",
            f"  utxo_count={snap['utxo_count']} state_hash={snap['state_hash']}",
        ]
        if extra:
            lines.append(f"  extra={extra}")
        return "\n".join(lines)
    except Exception as exc:  # dump must never hide the original assertion
        return f"[{case_id}] FAIL CONTEXT unavailable: {exc!r}; extra={extra}"
