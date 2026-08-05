#!/usr/bin/env python3
"""
RIGOR CONSENSUS SPECIFICATION - LAYER 1: EXECUTABLE SPEC
=========================================================

Executable semantics for the frozen draw kernel (baseline
``mainnet-freeze-rfc015-2026-05-26`` / commit ``c05f1a94beece54df872dc89c08715bd555fd987``).

Aligned to:
- ``rigor::ComputeSigmaHash`` / ``SelectionDigest``
- ``Consensus::ComputeConvergenceRootFromEligibilityTxids``
- ``Consensus::ConvergenceDrawInequalityHolds`` / ``IsEligible``
- ``ResolveConvergenceDraw`` / ``Consensus::RunSelection``

SECURITY-001-A8 (Sigma-PoW-01) — the draw runs one block behind. Block h-1 commits
the convergence set **F** and the root **C** over it; block h pays the winner. The σ
seed is B_{h-1}, the parent block hash, so every σ input is committed by a header
that already cost a proof-of-work solution and re-rolling the draw costs a block
rather than a hash.

REQUIRED:
- Byte-level SHA256d over the Sigma-PoW-01 100-byte preimage
  ``txid_i(32) || C(32) || B_{h-1}(32) || LE32(h)``
- B_{h-1} enters at offset 64 verbatim: no inner hash, no ancestor at any other depth
- The height field is the PAYOUT height h, i.e. (height of the block committing F) + 1
- memcmp txid order (bytes lexicographic)
- uint512-style integer comparisons (via ``rigor_uint512``)
- RFC-014 v1.1 ``RES_PROBABILITY_TARGET = floor(2^240 / MIN_CONVERGENCE_AMOUNT)``
  for the production inequality RHS (unit correction, SECURITY-001-A finding A-6)
"""

from __future__ import annotations

import hashlib
from dataclasses import dataclass
from typing import List, Optional, Tuple

from rigor_uint512 import (
    TXID_SIZE,
    uint256_bytes_to_arith_int,
    uint512_compare_u256_product,
    winner_cross_multiply_equal,
    winner_cross_multiply_less,
)

# =============================================================================
# CONSTANTS (frozen)
# =============================================================================

COIN = 100_000_000
MIN_CONVERGENCE_AMOUNT = 100_000_000
CONVERGENCE_DRAW_MIN_TXS_FOR_RES = 1

# SECURITY-001-A8: the σ seed is the parent block hash, so the draw is bound at depth 1
# unconditionally. There is no configurable bind depth and no bootstrap window: B_{h-1}
# is defined for every height from 1 upward.
CONVERGENCE_DRAW_SEED_DEPTH = 1

# SECURITY-001-A9-C: width, in blocks, of the sliding window F is enumerated over. The draw
# resolved at height h spans blocks [h-W, h-1] rather than block h-1 alone.
#
# Under W = 1 the producer of block h-1 chose F unilaterally and for free, so with
# CONVERGENCE_DRAW_MIN_TXS_FOR_RES == 1 he could win every block he produced at the minimum
# 1 ORU burn and RES allocation tracked hashrate instead of burn. Widening the window does not
# shrink his share of F; it removes EXCLUSIVITY, because the candidates in [h-W, h-2] are
# already buried and cannot be censored. W = 32 keeps the probability that one producer supplies
# the whole window (α^W) below 1e-9 at the design maximum α = 0.5, and being a power of two
# makes the target division below an exact shift.
CONVERGENCE_DRAW_WINDOW = 32
CONVERGENCE_DRAW_WINDOW_LOG2 = 5
assert CONVERGENCE_DRAW_WINDOW == 1 << CONVERGENCE_DRAW_WINDOW_LOG2

# RFC-014 v1.1 / RFC-015: immutable inequality RHS (uint256 little-endian integer value).
#
# SECURITY-001-A finding A-6: RFC-014 v1.0 wrote this as `2^240`, derived from
# `p = T / 2^256` at `f = 1`. The implemented weight is the convergence amount in
# satoshis, floored at MIN_CONVERGENCE_AMOUNT = 1e8, so `T * f_min / 2^256 = 1525.9`
# and the eligibility filter `H < T * f` was unconditionally true. Dividing out the
# unit error restores `p(n ORU) = min(1, n / 65536)`, i.e. `p(1 ORU) = 2^-16` exactly
# as RFC-014 §2.1 intended. See docs/rfcs/RFC-014-v1.1.md.
#
# SECURITY-001-A9-C then divided by the window W. Each convergence is now a candidate in W
# consecutive draws instead of one, so without this the issuance rate would be W times higher.
# Dividing the target by the same W restores neutrality exactly:
#
#   per-draw probability  p_draw(n) = n / (65536 * W)
#   expected wins per tx  W * p_draw(n) = n / 65536      <- unchanged from the pre-A9C model
#
# Per-draw saturation therefore moves from 65536 ORU to 65536 * W = 2_097_152 ORU. That
# softening is deliberate: design review finding F-2 named "burn to saturation, then censor" as
# the cheapest path to GUARANTEED acquisition, and the guarantee is what A-9-C removes.
#
# INVARIANT (RFC-014 v1.1 §4.3): RES_PROBABILITY_TARGET_INT * MIN_CONVERGENCE_AMOUNT < 2^256.
RES_PROBABILITY_TARGET_UNWINDOWED_INT = (1 << 240) // MIN_CONVERGENCE_AMOUNT
RES_PROBABILITY_TARGET_INT = RES_PROBABILITY_TARGET_UNWINDOWED_INT >> CONVERGENCE_DRAW_WINDOW_LOG2

assert RES_PROBABILITY_TARGET_INT * MIN_CONVERGENCE_AMOUNT < (1 << 256), (
    "RES_PROBABILITY_TARGET is vacuous: every candidate would be eligible unconditionally"
)
# T' is floor(T / W). T is not a multiple of W (T mod 32 == 28), so the division discards a
# remainder; the resulting deviation from exact issuance neutrality is 28/T ~ 1.6e-63, which is
# far below any quantity this protocol can express. The floor relation is what is pinned.
assert RES_PROBABILITY_TARGET_INT == RES_PROBABILITY_TARGET_UNWINDOWED_INT // CONVERGENCE_DRAW_WINDOW
assert (
    RES_PROBABILITY_TARGET_INT * CONVERGENCE_DRAW_WINDOW
    <= RES_PROBABILITY_TARGET_UNWINDOWED_INT
    < (RES_PROBABILITY_TARGET_INT + 1) * CONVERGENCE_DRAW_WINDOW
)


# =============================================================================
# DATA TYPES
# =============================================================================


@dataclass(frozen=True)
class ConvergenceTx:
    """Block-entry convergence candidate (member of **F** when predicates hold)."""

    txid: bytes
    convergence_amount: int
    is_valid: bool
    fee_positive: bool = True
    # SECURITY-001-A / A-5 R1: exactly one well-formed RESO owner declaration. Defaults to
    # True so pre-A-5 vectors keep their meaning; the predicate is exercised explicitly.
    has_owner_declaration: bool = True


@dataclass(frozen=True)
class BlockContext:
    """
    The whole σ context of one draw.

    ``height`` is the payout height h; ``prev_block_hash`` is B_{h-1}, the hash of the
    block that committed **F**. Nothing about the paying block other than h enters σ, so
    these two fields plus the committed set are the complete context. Mirrors the pair
    ``(ConvergenceDrawSet::source_height + 1, ConvergenceDrawSet::source_block_hash)``.
    """

    height: int
    prev_block_hash: bytes


@dataclass
class SelectionResult:
    """Mirrors ``Consensus::SelectionResult`` / ``ConvergenceDrawOutcome``."""

    convergence_root: bytes
    sigma_hashes: List[bytes]
    eligible: List[bool]
    winner_index: int


# =============================================================================
# BYTE-LEVEL PRIMITIVES
# =============================================================================


def sha256d(data: bytes) -> bytes:
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()


def memcmp_compare(a: bytes, b: bytes) -> int:
    if a < b:
        return -1
    if a > b:
        return 1
    return 0


def parse_hex256_sethex(hex_str: str) -> bytes:
    """Bitcoin ``uint256::SetHex`` / C++ ``ParseHex256`` memory layout."""
    h = hex_str.removeprefix("0x")
    raw = bytes.fromhex(h)
    if len(raw) != TXID_SIZE:
        raise ValueError("hex must decode to 32 bytes")
    return bytes(reversed(raw))


def hash_hex_to_uint256_bytes(hex_str: str) -> bytes:
    """``HashHexToUint256``: hash output byte order (m_data[0] = first hash byte)."""
    h = hex_str.removeprefix("0x")
    return bytes.fromhex(h)


def res_probability_target_from_bytes(target_bytes: bytes) -> int:
    """Interpret inequality RHS ``T`` as ``arith_uint256`` (LE uint256)."""
    return uint256_bytes_to_arith_int(target_bytes)


# =============================================================================
# CONVERGENCE ROOT
# =============================================================================


def compute_convergence_root_from_txids(txids: List[bytes]) -> bytes:
    if len(txids) < 2:
        return bytes(TXID_SIZE)
    for txid in txids:
        if len(txid) != TXID_SIZE:
            raise ValueError("txid must be 32 bytes")
    sorted_txids = sorted(txids)
    return sha256d(b"".join(sorted_txids))


def list_convergence_commitment_txids(txs: List[ConvergenceTx]) -> List[bytes]:
    """
    ``ListConvergenceRootCommitmentTxids`` predicate surface (block-entry view).

    Caller supplies ``is_valid`` (= CheckTxInputs ok) and ``fee_positive`` (= fee > 0).
    """
    out: List[bytes] = []
    for tx in txs:
        if not tx.is_valid:
            continue
        if not tx.fee_positive:
            continue
        if tx.convergence_amount < MIN_CONVERGENCE_AMOUNT:
            continue
        # SECURITY-001-A / A-5 R1: an undeclared convergence is a valid ORU burn that does
        # not play, so it is absent from F and therefore from the ConvergenceRoot preimage.
        if not tx.has_owner_declaration:
            continue
        out.append(tx.txid)
    return out


# =============================================================================
# SIGMA-POW-01
# =============================================================================

SIGMA_INPUT_SIZE = 100

# Normative offsets (RFC-000 §5). Changing any of them is a hard fork.
SIGMA_OFF_TXID = 0
SIGMA_OFF_CROOT = 32
SIGMA_OFF_PREV_BLOCK = 64
SIGMA_OFF_HEIGHT = 96


def compute_sigma_hash(
    txid: bytes,
    prev_block_hash: bytes,
    height: int,
    convergence_root: bytes,
) -> bytes:
    """``rigor::ComputeSigmaHash`` / ``SelectionDigest``."""
    return compute_pcobr_hash(txid, prev_block_hash, height, convergence_root)


def compute_pcobr_hash(
    txid: bytes,
    prev_block_hash: bytes,
    height: int,
    convergence_root: bytes,
) -> bytes:
    """
    σ_i = SHA256d( txid_i(32) || C(32) || B_{h-1}(32) || LE32(h) ), exactly 100 bytes.

    ``txid`` and ``convergence_root`` are drawn from block h-1; ``prev_block_hash`` is
    that block's hash and goes in at offset 64 verbatim. ``height`` is the payout height
    h. All 32-byte arguments are raw (native) byte order, not RPC display order.

    ``target_prev`` is deliberately not an input: B_{h-1} hashes the parent header, which
    already contains nBits, so the parent's compact target is committed more strongly than
    the pre-A-8 explicit fold achieved.
    """
    assert len(txid) == TXID_SIZE
    assert len(prev_block_hash) == TXID_SIZE
    assert len(convergence_root) == TXID_SIZE
    assert height >= 0

    height_le = (height & 0xFFFFFFFF).to_bytes(4, "little")
    buf = txid + convergence_root + prev_block_hash + height_le
    assert len(buf) == SIGMA_INPUT_SIZE
    return sha256d(buf)


# =============================================================================
# INEQUALITY (RFC-014 constant T)
# =============================================================================


def check_inequality(
    h: bytes,
    res_probability_target_int: int,
    weight: int,
) -> bool:
    """
    ``Consensus::ConvergenceDrawInequalityHolds`` / ``IsEligible``.

    ``H_i < T · f_i`` with ``T = res_probability_target_int`` (production: ``2^240``).
    ``weight`` is on-chain integer ``f_i`` (mint cost / fee in base units), ``>= 1``.
    """
    return uint512_compare_u256_product(h, res_probability_target_int, weight)


def convergence_draw_inequality_holds(
    h: bytes,
    res_probability_target_int: int,
    f_sats: int,
) -> bool:
    return check_inequality(h, res_probability_target_int, f_sats)


# =============================================================================
# WINNER SELECTION (uint512 cross-multiply + txid tie-break)
# =============================================================================


def txid_memcmp_less(a: bytes, b: bytes) -> bool:
    """``uint256`` operator ``<`` — unsigned byte-wise order."""
    return a < b


def compute_convergence_draw_selection(
    candidate_txids: List[bytes],
    weights: List[int],
    convergence_root: bytes,
    prev_block_hash: bytes,
    block_height: int,
    res_probability_target_int: int,
) -> SelectionResult:
    """
    ``ResolveConvergenceDraw`` in ``convergence_logic.cpp``.

    ``candidate_txids``, ``weights`` and ``convergence_root`` are the set committed by
    block h-1; ``prev_block_hash`` is that block's hash and ``block_height`` the payout
    height h. The C++ entry point takes neither height nor seed because both are derived
    from the set itself, which is what stops a caller resolving a set against the wrong
    context; this function keeps them explicit so vectors can vary them independently.
    """
    out = SelectionResult(
        convergence_root=convergence_root,
        sigma_hashes=[],
        eligible=[],
        winner_index=-1,
    )
    if len(candidate_txids) != len(weights):
        return out

    sigmas: List[bytes] = []
    for txid in candidate_txids:
        sigmas.append(
            compute_sigma_hash(txid, prev_block_hash, block_height, convergence_root)
        )
    out.sigma_hashes = sigmas

    if len(candidate_txids) < CONVERGENCE_DRAW_MIN_TXS_FOR_RES:
        out.eligible = [False] * len(candidate_txids)
        return out

    eligible = [
        check_inequality(sigmas[i], res_probability_target_int, weights[i])
        for i in range(len(candidate_txids))
    ]
    out.eligible = eligible

    winner_index = -1
    for i in range(len(candidate_txids)):
        if not eligible[i]:
            continue
        if winner_index < 0:
            winner_index = i
            continue
        w = winner_index
        if winner_cross_multiply_less(
            sigmas[i], weights[i], sigmas[w], weights[w]
        ):
            winner_index = i
        elif winner_cross_multiply_equal(
            sigmas[i], weights[i], sigmas[w], weights[w]
        ):
            if txid_memcmp_less(candidate_txids[i], candidate_txids[w]):
                winner_index = i

    out.winner_index = winner_index
    return out


def run_selection(
    txids: List[bytes],
    weights: List[int],
    height: int,
    prev_block_hash: bytes,
    res_probability_target_int: int,
) -> SelectionResult:
    """
    ``Consensus::RunSelection`` — full draw trace for golden tests.

    ``txids`` / ``weights`` are the set committed by block h-1, ``prev_block_hash`` is
    B_{h-1} and ``height`` is the payout height h.
    """
    c_root = compute_convergence_root_from_txids(txids)
    return compute_convergence_draw_selection(
        txids,
        weights,
        c_root,
        prev_block_hash,
        height,
        res_probability_target_int,
    )


def resolve_convergence_draw(
    txids: List[bytes],
    weights: List[int],
    convergence_root: bytes,
    source_block_hash: bytes,
    source_height: int,
    res_probability_target_int: int = RES_PROBABILITY_TARGET_INT,
) -> SelectionResult:
    """
    ``ResolveConvergenceDraw(set, T)`` with the C++ argument surface: the caller supplies
    the committed set as block h-1 recorded it and cannot choose the payout height or the
    seed separately, because both come from that block.
    """
    return compute_convergence_draw_selection(
        txids,
        weights,
        convergence_root,
        source_block_hash,
        source_height + 1,
        res_probability_target_int,
    )


def convergence_draw_eligible_root(
    txids: List[bytes], eligible: List[bool]
) -> bytes:
    """
    ``ConvergenceDrawEligibleRoot`` — flat commitment over the inequality-eligible subset,
    carried in the coinbase of the block that PAYS the draw. Same sorted-concat-then-
    SHA256d shape as the convergence root but with no ``N < 2`` boundary, matching
    ``HashConcatenatedSortedTxids``.
    """
    return sha256d(b"".join(sorted(t for t, ok in zip(txids, eligible) if ok)))


def select_convergence_winner_from_candidates(
    candidate_txids: List[bytes],
    weights: List[int],
    prev_block_hash: bytes,
    height: int,
    convergence_root: bytes,
    res_probability_target_int: int = RES_PROBABILITY_TARGET_INT,
) -> Optional[bytes]:
    sel = compute_convergence_draw_selection(
        candidate_txids,
        weights,
        convergence_root,
        prev_block_hash,
        height,
        res_probability_target_int,
    )
    if sel.winner_index < 0:
        return None
    return candidate_txids[sel.winner_index]


def select_winner(
    txs: List[ConvergenceTx],
    prev_block_hash: bytes,
    height: int,
    convergence_root: Optional[bytes] = None,
    res_probability_target_int: int = RES_PROBABILITY_TARGET_INT,
) -> Optional[bytes]:
    """High-level helper: build **F**, **C**, then draw."""
    f_txids = list_convergence_commitment_txids(txs)
    if len(f_txids) < CONVERGENCE_DRAW_MIN_TXS_FOR_RES:
        return None
    c_root = (
        convergence_root
        if convergence_root is not None
        else compute_convergence_root_from_txids(f_txids)
    )
    weights = []
    txid_to_weight = {tx.txid: tx.convergence_amount for tx in txs if tx.is_valid and tx.fee_positive}
    for txid in f_txids:
        weights.append(txid_to_weight[txid])
    return select_convergence_winner_from_candidates(
        f_txids,
        weights,
        prev_block_hash,
        height,
        c_root,
        res_probability_target_int,
    )


# Legacy aliases — deprecated (pre-RFC-014); do not use in new tests
def convergence_amount_to_weight(convergence_amount: int) -> int:
    raise NotImplementedError(
        "Removed: C++ uses raw convergence amount as f_i, not pow(7/5)"
    )


def compact_target_to_target(compact: int) -> int:
    raise NotImplementedError(
        "Removed: inequality T is RES_PROBABILITY_TARGET, not compact nBits"
    )


def convergence_winner_tie_break_compare(
    txid_a: bytes, txid_b: bytes, convergence_root: bytes
) -> int:
    raise NotImplementedError("Removed: tie-break is txid memcmp only (RFC-000 §9)")


def select_convergence_winner(
    candidates: List[Tuple[bytes, bytes, int, int]],
    convergence_root: bytes,
    res_probability_target_int: int = RES_PROBABILITY_TARGET_INT,
) -> Optional[bytes]:
    """
    Tuple form: ``(txid, prev_block_hash, height, weight)``.

    The seed and height are read from the first tuple: every candidate in a draw shares
    them by construction, since they come from the one block that committed the set.
    """
    if len(candidates) < CONVERGENCE_DRAW_MIN_TXS_FOR_RES:
        return None
    txids = [c[0] for c in candidates]
    weights = [c[3] for c in candidates]
    prev_block_hash = candidates[0][1]
    height = candidates[0][2]
    return select_convergence_winner_from_candidates(
        txids,
        weights,
        prev_block_hash,
        height,
        convergence_root,
        res_probability_target_int,
    )


# =============================================================================
# PROOF / ASSET HELPERS (unchanged surface)
# =============================================================================


def validate_orb_proof(
    convergence_txid: bytes,
    convergence_root: bytes,
    in_F: bool,
    inequality_holds: bool,
    expected_convergence_root: Optional[bytes] = None,
) -> Tuple[bool, Optional[str]]:
    # SECURITY-001-A / A-7: there is no signature term. The witness used to carry an Ed25519
    # signature over the sigma digest, verified under a key the witness supplied itself, so it
    # constrained nothing that the field bindings above do not already constrain.
    if not in_F:
        return False, "bad-ORB-Convergence-not-in-Convergenceroot-set"
    if expected_convergence_root is not None and convergence_root != expected_convergence_root:
        return False, "bad-ORB-root-mismatch"
    if not inequality_holds:
        return False, "bad-ORB-inequality"
    return True, None


ASSET_ORU = "ORU"
ASSET_RES = "RES"


def _normalize_asset_symbol(asset: str) -> str:
    """Golden-vector aliases only (not consensus wire format)."""
    aliases = {"ORI": ASSET_ORU, "ORB": ASSET_RES}
    return aliases.get(asset, asset)


def determine_res_mode(
    input_assets: List[str], has_proof: bool
) -> Tuple[str, bool, Optional[str]]:
    if not input_assets:
        return "claim", False, None
    normalized = [_normalize_asset_symbol(a) for a in input_assets]
    if len(set(normalized)) > 1:
        return "", False, "mixed-asset-output"
    asset = normalized[0]
    if asset == ASSET_ORU:
        if has_proof:
            return "mint", True, None
        return "claim", False, "bad-res-no-proof"
    if asset == ASSET_RES:
        return "claim", False, None
    return "", False, "bad-txns-spend-non-asset-input"


def create_test_txid(i: int) -> bytes:
    return sha256d(i.to_bytes(4, "little"))


if __name__ == "__main__":
    print("rigor_consensus_spec self-check")
    txids = [create_test_txid(i) for i in range(2)]
    r = run_selection(
        txids,
        [1, 1],
        10,
        create_test_txid(99),
        RES_PROBABILITY_TARGET_INT,
    )
    print("C:", r.convergence_root.hex())
    print("winner_index:", r.winner_index)
