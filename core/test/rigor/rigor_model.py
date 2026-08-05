#!/usr/bin/env python3
"""RIGOR consensus invariant model used by constitution tests.

This module intentionally models invariants (what must hold), not current
implementation details (how a specific function is written).
"""

from __future__ import annotations

from dataclasses import dataclass
import hashlib
from typing import Iterable, List, Optional, Sequence, Tuple


SCRIPT_WITNESS_V0 = 0
SCRIPT_WITNESS_V1 = 1
SCRIPT_OP_RETURN = 106  # OP_RETURN

CVG2 = b"CVG2"
ORB2 = b"ORB2"


@dataclass(frozen=True)
class Script:
    opcode: int
    program: bytes


@dataclass(frozen=True)
class convergenceCandidate:
    txid: bytes  # 32-byte txid


@dataclass(frozen=True)
class RigorCommitment:
    winner_txid: bytes  # 32-byte txid
    nbits: int          # uint32


def is_convergence_script(script: Script) -> bool:
    return (
        script.opcode == SCRIPT_WITNESS_V1
        and len(script.program) == 32
        and script.program.startswith(CVG2)
    )


def is_res_mint_script(script: Script) -> bool:
    return (
        script.opcode == SCRIPT_WITNESS_V1
        and len(script.program) == 36
        and script.program.startswith(ORB2)
    )


def is_asset_script(script: Script) -> bool:
    if script.opcode == SCRIPT_OP_RETURN:
        return False
    return is_convergence_script(script) or is_res_mint_script(script) or script.opcode == SCRIPT_WITNESS_V0


def compute_convergence_root_bytes(convergence_set: Sequence[convergenceCandidate]) -> bytes:
    """Match ``ComputeconvergenceRootFromEligibilityTxids`` (sorted raw txid concat, Hash256)."""
    if len(convergence_set) < 2:
        return bytes(32)
    data = b"".join(sorted((c.txid for c in convergence_set)))
    h = hashlib.sha256(data).digest()
    return hashlib.sha256(h).digest()


def compute_convergence_hash(txid: bytes, prev_block_hash: bytes, height: int, convergence_root: bytes) -> bytes:
    """Match ``rigor::ComputeSigmaHash`` (Sigma-PoW-01): Hash256 over exactly 100 bytes.

    Layout, all normative (SECURITY-001-A8 / RFC-009 §4)::

        0  .. 31   txid              (a member of F, the convergence set of block h-1)
        32 .. 63   C                 (ConvergenceRoot over that same set)
        64 .. 95   prev_block_hash         (B_{h-1}, the parent block hash, verbatim)
        96 .. 99   LE32(height)      (the payout height)

    ``target_prev`` is deliberately absent: the parent header already commits its own
    nBits, so folding it in separately bought nothing.
    """
    assert len(txid) == 32
    assert len(prev_block_hash) == 32
    assert len(convergence_root) == 32
    preimage = txid + convergence_root + prev_block_hash + (height & 0xFFFFFFFF).to_bytes(4, "little")
    assert len(preimage) == 100
    h = hashlib.sha256(preimage).digest()
    return hashlib.sha256(h).digest()


def _xor256_bytes(a: bytes, b: bytes) -> bytes:
    assert len(a) == len(b) == 32
    return bytes(x ^ y for x, y in zip(a, b))


def compute_winner(
    convergence_set: Sequence[convergenceCandidate],
    prev_block_hash: bytes,
    height: int,
) -> Optional[bytes]:
    # RIGOR: fewer than 2 convergence candidates => no winner.
    if len(convergence_set) < 2:
        return None
    convergence_root = compute_convergence_root_bytes(convergence_set)
    scored: List[Tuple[bytes, bytes, bytes]] = []
    for c in convergence_set:
        h = compute_convergence_hash(c.txid, prev_block_hash, height, convergence_root)
        tie = _xor256_bytes(c.txid, convergence_root)
        scored.append((h, tie, c.txid))
    scored.sort(key=lambda x: (x[0], x[1]))
    return scored[0][2]


def validate_res_mint_binding(res_mint_txids: Sequence[bytes], winner: Optional[bytes]) -> bool:
    if len(res_mint_txids) > 1:
        return False
    if not res_mint_txids:
        return winner is None
    if winner is None:
        return False
    return res_mint_txids[0] == winner


def validate_commitment(commitment: RigorCommitment, computed_winner: Optional[bytes], computed_nbits: int) -> bool:
    winner = computed_winner if computed_winner is not None else bytes(32)
    return commitment.winner_txid == winner and commitment.nbits == computed_nbits


def validate_invalid_block_cases(
    commitment: RigorCommitment,
    computed_winner: Optional[bytes],
    computed_nbits: int,
    res_mint_txids: Sequence[bytes],
) -> bool:
    if not validate_commitment(commitment, computed_winner, computed_nbits):
        return False
    return validate_res_mint_binding(res_mint_txids, computed_winner)


def deterministic_reindex_result(
    convergence_sets: Iterable[Sequence[convergenceCandidate]],
    prev_block_hashes: Iterable[bytes],
    heights: Iterable[int],
) -> List[Optional[bytes]]:
    winners: List[Optional[bytes]] = []
    for convergence_set, prev_block_hash, height in zip(convergence_sets, prev_block_hashes, heights):
        winners.append(compute_winner(convergence_set, prev_block_hash, height))
    return winners
