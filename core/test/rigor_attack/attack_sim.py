#!/usr/bin/env python3
"""Adversarial simulation helpers for RIGOR attack tests.

Models consensus invariants (convergence draw inequality, winner selection,
commitment / RES mint binding) aligned with CONSENSUS_LOCK_FORGE_V3 — not RPC wiring.
"""

from __future__ import annotations

import math
import os
import sys
from typing import List, Optional, Sequence, Tuple

# Reuse invariant model from sibling suite.
_RG = os.path.join(os.path.dirname(__file__), "..", "rigor")
if _RG not in sys.path:
    sys.path.insert(0, _RG)

from rigor_model import (  # noqa: E402
    RigorCommitment,
    convergenceCandidate,
    _xor256_bytes,
    compute_convergence_hash,
    compute_convergence_root_bytes,
    validate_commitment,
    validate_invalid_block_cases,
)

COIN = 100_000_000
FORGE_DRAW_MIN_TXS = 2
FORGE_BETA = 1.4
MAX_RES_MINT_SIM_SUPPLY = 21000000 * 100  # Mirrors Consensus::RES max supply constant (simulation only)


def u256_le(b: bytes) -> int:
    assert len(b) == 32
    return int.from_bytes(b, "little")


def convergence_draw_candidate_effective_weight_u64(convergence_amount: int, remaining_res_supply: int) -> int:
    """Mirror Consensus::convergenceDrawEffectiveWeightU64 (β only; remaining_res_supply ignored)."""
    del remaining_res_supply
    if convergence_amount < COIN:
        return 0
    f = convergence_amount / COIN
    w = (f**FORGE_BETA) * COIN
    if not math.isfinite(w) or w <= 0:
        return 0
    cap = 2**64 - 1
    if w >= cap:
        return cap
    return int(w)


def hash_below_target_times_convergence_amount(
    h_digest: bytes,
    scale: int,
    T_digest: bytes,
    convergence_amount: int,
    remaining_res_supply: int = MAX_RES_MINT_SIM_SUPPLY,
) -> bool:
    """Consensus inequality: h * scale < T * W (W from β/γ); uint256 h, T as LE integers."""
    w = convergence_draw_candidate_effective_weight_u64(convergence_amount, remaining_res_supply)
    if w <= 0:
        return False
    h = u256_le(h_digest)
    t = u256_le(T_digest)
    return h * scale < t * w


def collect_convergence_winner_txid(
    convergence_txs: Sequence[Tuple[bytes, int]],
    prev_block_hash: bytes,
    height: int,
    res_draw_target: bytes,
    remaining_res_supply: int = MAX_RES_MINT_SIM_SUPPLY,
    *,
    enforce_min_two_candidates: bool = True,
) -> Optional[bytes]:
    """Winner = argmin digest among convergence transactions that pass the ConvergenceDrawCandidates inequality; h ties: bytes(txid XOR convergence_root).

    When ``enforce_min_two_candidates`` is True (default), require at least two input rows — used by
    legacy attack simulations. Set to False to mirror ``CollectconvergenceWinnerTxid`` / ``GetBlockWinner``
    (no minimum count on-chain).
    """
    if enforce_min_two_candidates and len(convergence_txs) < FORGE_DRAW_MIN_TXS:
        return None
    convergence_root = compute_convergence_root_bytes([convergenceCandidate(txid) for txid, _ in convergence_txs])
    best_txid: Optional[bytes] = None
    best_h: Optional[int] = None
    for txid, convergence_amount in convergence_txs:
        if convergence_amount < COIN:
            continue
        h_bytes = compute_convergence_hash(txid, prev_block_hash, height, convergence_root)
        if not hash_below_target_times_convergence_amount(
            h_bytes, COIN, res_draw_target, convergence_amount, remaining_res_supply
        ):
            continue
        h_i = u256_le(h_bytes)
        if best_h is None or h_i < best_h or (
            h_i == best_h and _xor256_bytes(txid, convergence_root) < _xor256_bytes(best_txid, convergence_root)
        ):
            best_h = h_i
            best_txid = txid
    return best_txid


def verdict(ok: bool, fail_msg: str) -> str:
    return "PASS" if ok else f"FAIL: {fail_msg}"


def assert_attack(ok: bool, vulnerability: str) -> None:
    if not ok:
        raise AssertionError(f"FAIL\nvulnerability: {vulnerability}")
