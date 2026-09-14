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
- RFC-017 shipping ``RES_PROBABILITY_TARGET = floor(2^246 / MIN_CONVERGENCE_AMOUNT) / CONVERGENCE_DRAW_WINDOW``
  (mint cost ``C = 1024`` ORU). Historical RFC-014 v1.1 used ``floor(2^240 / MIN_CONVERGENCE_AMOUNT)``
  (unit correction, SECURITY-001-A finding A-6).
"""

from __future__ import annotations

import hashlib
import math
from dataclasses import dataclass
from typing import Any, Dict, List, Optional, Tuple

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
# as RFC-014 §2.1. See docs/archive/rfc/RFC-014-frozen-convergence-target.md.
#
# SECURITY-001-A9-C then divided by the window W. Each convergence is now a candidate in W
# consecutive draws instead of one, so without this the issuance rate would be W times higher.
# Dividing the target by the same W restores neutrality exactly:
#
#   per-draw probability  p_draw(n) = n / (C * W)
#   expected wins per tx  W * p_draw(n) = n / C          <- unchanged from the pre-A9C model
#
# Per-draw saturation therefore moves from C ORU to C * W ORU. That softening is deliberate:
# design review finding F-2 named "burn to saturation, then censor" as the cheapest path to
# GUARANTEED acquisition, and the guarantee is what A-9-C removes.
#
# RFC-017 §5.3 then moved the exponent from 240 to 246. Unlike A-6 and A-9-C this is a monetary
# decision rather than a correction: E fixes C = 2^(256-E), the ORU burned per mint, and with a
# scheduled (demand-independent) reward C is the entire cost side of the system. At E = 240,
# C = 65536 ORU exceeded 65 blocks of total chain issuance, so cumulative RES stayed under 1% of
# the cap and the cap described nothing. E = 246 gives C = 1024 ORU.
#
# INVARIANT (RFC-014 v1.1 §4.3): RES_PROBABILITY_TARGET_INT * MIN_CONVERGENCE_AMOUNT < 2^256.
RES_PROBABILITY_TARGET_EXPONENT = 246
RES_PROBABILITY_TARGET_UNWINDOWED_INT = (
    1 << RES_PROBABILITY_TARGET_EXPONENT
) // MIN_CONVERGENCE_AMOUNT
RES_PROBABILITY_TARGET_INT = RES_PROBABILITY_TARGET_UNWINDOWED_INT >> CONVERGENCE_DRAW_WINDOW_LOG2

# ORU burned per successful draw, and the weight at which the filter stops discriminating.
CONVERGENCE_MINT_COST = (1 << 256) // RES_PROBABILITY_TARGET_UNWINDOWED_INT
CONVERGENCE_SATURATION_WEIGHT = CONVERGENCE_MINT_COST * CONVERGENCE_DRAW_WINDOW
assert CONVERGENCE_MINT_COST == 1024 * MIN_CONVERGENCE_AMOUNT
assert CONVERGENCE_SATURATION_WEIGHT == 32768 * MIN_CONVERGENCE_AMOUNT

assert RES_PROBABILITY_TARGET_INT * MIN_CONVERGENCE_AMOUNT < (1 << 256), (
    "RES_PROBABILITY_TARGET is vacuous: every candidate would be eligible unconditionally"
)
# T' is floor(T / W). T is not a multiple of W (T mod 32 == 27 at E = 246; it was 28 at E = 240),
# so the division discards a remainder; the resulting deviation from exact issuance neutrality is
# 27/T, far below any quantity this protocol can express. The floor relation is what is pinned.
assert RES_PROBABILITY_TARGET_UNWINDOWED_INT % CONVERGENCE_DRAW_WINDOW == 27
assert RES_PROBABILITY_TARGET_INT == RES_PROBABILITY_TARGET_UNWINDOWED_INT // CONVERGENCE_DRAW_WINDOW
assert (
    RES_PROBABILITY_TARGET_INT * CONVERGENCE_DRAW_WINDOW
    <= RES_PROBABILITY_TARGET_UNWINDOWED_INT
    < (RES_PROBABILITY_TARGET_INT + 1) * CONVERGENCE_DRAW_WINDOW
)

# RFC-020: continuous destroy cost destroy_cost(h).
#
# Display destroy_cost(h) = P0 · (2^25 + h)^2 >> 50. Consensus evaluates
# R · (2^25 + h)^2 ≤ B << 40. Mirrors Consensus::ResDestroyCostAtHeight /
# CheckResMintArithmetic in core/src/consensus/res_issuance.h,
# tools/res_continuous_price_schedule.py, and ResSchedule.v.
#
# Observational calendar k = height // 210000 is not a mint argument.
# There is no mint horizon.
RES_COIN = 100_000_000
RES_MAX_SUPPLY = 419_430_400_000_000
RES_DESTROY_COST_GENESIS = 1024 * COIN
RES_DESTROY_POLE = 1 << 25
RES_DESTROY_SHIFT = 40
RES_REWARD_EPOCH = 210_000
# Observational calendar bucket only. Not a mint horizon.
RES_REWARD_LAST_EPOCH = 250
MAX_MONEY = 10_000_000_000 * COIN


def res_destroy_cost_at_height(height: int) -> int:
    if height < 0:
        return 0
    pole_h = RES_DESTROY_POLE + height
    return (RES_DESTROY_COST_GENESIS * pole_h * pole_h) >> 50


def res_destroy_cost_for_epoch(k: int) -> int:
    """Observational display destroy cost at the first height of calendar k."""
    if k < 0:
        return 0
    return res_destroy_cost_at_height(k * RES_REWARD_EPOCH)


def res_mint_authorized(height: int) -> bool:
    return height >= 0


def check_res_mint_arith(res_atoms: int, burn_atoms: int, height: int) -> bool:
    if res_atoms < 0 or burn_atoms < 0 or height < 0:
        return False
    if res_atoms == 0:
        return True
    if res_atoms > RES_MAX_SUPPLY or burn_atoms > MAX_MONEY:
        return False  # same fail-closed bounds as CheckResMintArithmetic
    pole_h = RES_DESTROY_POLE + height
    return res_atoms * pole_h * pole_h <= (burn_atoms << RES_DESTROY_SHIFT)


def get_res_mint_reward(height: int) -> int:
    """Compatibility alias: the destroy cost at this height, not a mint quantity."""
    return res_destroy_cost_at_height(height)


def res_reward_for_epoch(k: int) -> int:
    """Compatibility alias: observational calendar price, not a grant."""
    return res_destroy_cost_for_epoch(k)


RES_REWARD_GENESIS = RES_DESTROY_COST_GENESIS
RES_SCHEDULE_SUM = RES_MAX_SUPPLY

assert res_destroy_cost_for_epoch(0) == RES_DESTROY_COST_GENESIS
assert res_destroy_cost_at_height(0) == RES_DESTROY_COST_GENESIS
assert res_destroy_cost_at_height(1) == 102400006103
assert res_destroy_cost_at_height(210000) == 103685749152
assert res_mint_authorized(210000 * (RES_REWARD_LAST_EPOCH + 1))
assert check_res_mint_arith(RES_COIN, RES_DESTROY_COST_GENESIS, 0)
assert not check_res_mint_arith(RES_COIN, RES_DESTROY_COST_GENESIS - 1, 0)


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
    if len(txids) == 0:
        return bytes(TXID_SIZE)
    for txid in txids:
        if len(txid) != TXID_SIZE:
            raise ValueError("txid must be 32 bytes")
    sorted_txids = sorted(txids)
    return sha256d(b"".join(sorted_txids))


# =============================================================================
# ABSTRACT CVG2 / CONVERGENCE-TX SHAPE (not a script interpreter)
# =============================================================================

CVG2_TAG = b"CVG2"
RES2_TAG = b"RES2"
RIGOR_PUSH_0 = 0x00
RIGOR_PUSH_1 = 0x51
OP_PUSH20 = 0x14
OP_PUSH32 = 0x20


def canonical_cvg2_script(tail: bytes = bytes(28)) -> bytes:
    """Witness v1 CVG2 container: RIGOR_PUSH_1 <32-byte program with CVG2 prefix>."""
    if len(tail) != 28:
        raise ValueError("CVG2 program tail must be 28 bytes")
    prog = CVG2_TAG + tail
    return bytes([RIGOR_PUSH_1, OP_PUSH32]) + prog


def is_cvg2_script(spk: bytes) -> bool:
    """Abstract form matching C++ MatchCvg2WitnessScript (v1 / 32 / CVG2 prefix)."""
    if len(spk) < 2:
        return False
    if spk[0] != RIGOR_PUSH_1 or spk[1] != OP_PUSH32:
        return False
    prog = spk[2:]
    return len(prog) == 32 and prog[:4] == CVG2_TAG


# =============================================================================
# ABSTRACT SCRIPT CLASS / PCORB MINT-WITNESS SHAPE (not an interpreter)
# TASK-FV-VECTOR-SCRIPT-AND-MINT-WITNESS-NEG-001
# =============================================================================

RIGOR_PUSH_2 = 0x52
RIGOR_PUSH_16 = 0x60
RIGOR_RESERVED_LEGACY_DUP = 0x76
RIGOR_RESERVED_LEGACY_EQUAL = 0x87
RIGOR_RESERVED_LEGACY_EQUALVERIFY = 0x88
RIGOR_RESERVED_LEGACY_HASH160 = 0xA9
RIGOR_RESERVED_LEGACY_CHECKSIG = 0xAC
RIGOR_MARKER_RETURN = 0x6A
RIGOR_MARKER_RES_MINT = 0xBB
RIGOR_MARKER_CONVERGE = 0xBD
OP_PUSH20 = 0x14

PCORB_REQUIRED_SIZES = (32, 8, 32, 8, 4, 32)


def _cscriptnum_serialize(n: int) -> bytes:
    """C++ CScriptNum::serialize."""
    if n == 0:
        return b""
    neg = n < 0
    absvalue = -n if neg else n
    result = bytearray()
    while absvalue:
        result.append(absvalue & 0xFF)
        absvalue >>= 8
    if result[-1] & 0x80:
        result.append(0x80 if neg else 0)
    elif neg:
        result[-1] |= 0x80
    return bytes(result)


def _push_data(data: bytes) -> bytes:
    n = len(data)
    if n < 76:
        return bytes([n]) + data
    raise ValueError("push too large for mint/converge templates")


def get_res_mint_script(maturity_height: int, pubkey_hash: bytes) -> bytes:
    """C++ GetResMintScript: CScriptNum push || 20-byte hash || RIGOR_MARKER_RES_MINT."""
    if len(pubkey_hash) != 20:
        raise ValueError("pubkey_hash must be 20 bytes")
    return _push_data(_cscriptnum_serialize(maturity_height)) + _push_data(pubkey_hash) + bytes([RIGOR_MARKER_RES_MINT])


def is_res_mint_script(spk: bytes) -> bool:
    """C++ IsResMintScript / ParseResMintScript (exact GetResMintScript encoding)."""
    if not spk or spk[-1] != RIGOR_MARKER_RES_MINT or len(spk) < 22:
        return False
    if spk[-22] != OP_PUSH20:
        return False
    pkh = spk[-21:-1]
    prefix = spk[:-22]
    # Reconstruct maturity from the prefix as a single CScriptNum push and round-trip.
    if prefix == b"\x00":
        maturity = 0
    elif len(prefix) >= 1 and 1 <= prefix[0] <= 75 and len(prefix) == 1 + prefix[0]:
        data = prefix[1:]
        if not data:
            maturity = 0
        else:
            maturity = 0
            for i, b in enumerate(data):
                maturity |= b << (8 * i)
            if data[-1] & 0x80:
                return False  # negative rejected by ParseResMintScript
    else:
        return False
    return spk == get_res_mint_script(maturity, pkh)


def is_converge_script(spk: bytes) -> bool:
    """C++ ParseConvergeScript: OP_PUSHBYTES_20 || 20-byte hash || RIGOR_MARKER_CONVERGE."""
    return len(spk) == 22 and spk[0] == OP_PUSH20 and spk[21] == RIGOR_MARKER_CONVERGE


def witness_version_of(b: int) -> Optional[int]:
    if b == RIGOR_PUSH_0:
        return 0
    if RIGOR_PUSH_1 <= b <= RIGOR_PUSH_16:
        return b - 0x50
    return None


def parse_witness_program(spk: bytes) -> Optional[Tuple[int, bytes]]:
    """BIP141-shaped container. v1 is not BIP341."""
    n = len(spk)
    if n < 4 or n > 42:
        return None
    ver = witness_version_of(spk[0])
    if ver is None:
        return None
    plen = spk[1]
    if plen < 1 or plen + 2 != n:
        return None
    return ver, spk[2:]


def is_native_res_program(prog: bytes) -> bool:
    """C++ IsRigorResNativeWitnessProgram: 32 bytes, neither CVG2 nor RES2.

    Option B: the 32-byte program *is* the Ed25519 public key. This helper
    only classifies wire shape; it does not hash the key.
    """
    return len(prog) == 32 and prog[:4] != CVG2_TAG and prog[:4] != RES2_TAG


def is_allowed_script(spk: bytes) -> bool:
    """C++ IsAllowedScript / ClassifyRigorTxout admit-set.

    Witness v0/20, native v1/32 (not CVG2/RES2), RIGOR_MARKER_RES_MINT, RIGOR_MARKER_CONVERGE.
    RIGOR_MARKER_RETURN, legacy P2PKH, P2WSH, CVG2, RES2, and unknown programs are rejected.
    """
    if is_res_mint_script(spk) or is_converge_script(spk):
        return True
    wp = parse_witness_program(spk)
    if wp is None:
        return False
    ver, prog = wp
    if ver == 0:
        return len(prog) == 20
    if ver == 1:
        return is_native_res_program(prog)
    return False


def is_res_script(spk: bytes) -> bool:
    """C++ IsResScript: native v1 / 32, or RIGOR_MARKER_RES_MINT. CVG2/RES2 are not RES."""
    if is_res_mint_script(spk):
        return True
    wp = parse_witness_program(spk)
    if wp is None:
        return False
    ver, prog = wp
    return ver == 1 and is_native_res_program(prog)


def is_push_only(rest: bytes) -> bool:
    """C++ CScript::IsPushOnly: GetOp succeeds and every opcode <= RIGOR_PUSH_16.

    Includes RIGOR_RESERVED_INERT_50 (0x50). Does not include opcodes > 0x60.
    """
    i = 0
    n = len(rest)
    while i < n:
        op = rest[i]
        i += 1
        if op <= 75:
            if i + op > n:
                return False
            i += op
        elif op == 0x4C:
            if i >= n:
                return False
            ln = rest[i]
            i += 1
            if i + ln > n:
                return False
            i += ln
        elif op == 0x4D:
            if i + 2 > n:
                return False
            ln = int.from_bytes(rest[i : i + 2], "little")
            i += 2
            if i + ln > n:
                return False
            i += ln
        elif op == 0x4E:
            if i + 4 > n:
                return False
            ln = int.from_bytes(rest[i : i + 4], "little")
            i += 4
            if i + ln > n:
                return False
            i += ln
        elif op <= 0x60:
            continue
        else:
            return False
    return True


def is_op_return(spk: bytes) -> bool:
    """C++ NULL_DATA: leading RIGOR_MARKER_RETURN and the remainder is push-only."""
    if not spk or spk[0] != RIGOR_MARKER_RETURN:
        return False
    return is_push_only(spk[1:])


def asset_from_script(spk: bytes) -> str:
    """C++ AssetTypeFromScriptPubKey. Unrecognized → INVALID, not ORU/NONE."""
    wp = parse_witness_program(spk)
    if wp is not None:
        ver, prog = wp
        if ver == 0 and len(prog) == 20:
            return "ORU"
        if ver == 1 and is_native_res_program(prog):
            return "RES"
        return "INVALID"
    if is_res_mint_script(spk):
        return "RES"
    if is_converge_script(spk):
        return "ORU"
    if is_op_return(spk):
        return "NONE"
    return "INVALID"


def classify_script_reject(spk: bytes, claimed: Optional[str] = None) -> Optional[str]:
    """None means the abstract script form is admitted.

    claimed INVALID means "this script is not an admitted class" and is
    checked before the allowed-script gate so the branch is reachable.
    """
    if claimed == "INVALID":
        return None if asset_from_script(spk) == "INVALID" else "asset_tag_mismatch"
    if not is_allowed_script(spk):
        return "bad_rigor_script_type"
    if claimed is None:
        return None
    if claimed == "RES" and not is_res_script(spk):
        return "not_res_script"
    if claimed == "ORU":
        if is_res_script(spk):
            return "not_oru_script"
        if asset_from_script(spk) != "ORU":
            return "asset_tag_mismatch"
        return None
    if claimed == "NONE" and asset_from_script(spk) != "NONE":
        return "asset_tag_mismatch"
    return None


def classify_spend_cvg2(spk: bytes) -> Optional[str]:
    return "spend_cvg2" if is_cvg2_script(spk) else None


def parse_pcorb_shape(stack: List[bytes]) -> bool:
    """C++ ParseConvergeProof size prefix: ≥6 items, first six 32/8/32/8/4/32."""
    if len(stack) < 6:
        return False
    return all(len(stack[i]) == sz for i, sz in enumerate(PCORB_REQUIRED_SIZES))


def classify_pcorb_reject(stack: List[bytes]) -> Optional[str]:
    if len(stack) < 6:
        return "pcorb_too_few"
    if not parse_pcorb_shape(stack):
        return "pcorb_field_size"
    return None


def canonical_p2wpkh_script() -> bytes:
    return bytes([RIGOR_PUSH_0, OP_PUSH20]) + bytes(20)


def canonical_p2wsh_script() -> bytes:
    return bytes([RIGOR_PUSH_0, OP_PUSH32]) + bytes(32)


def canonical_res2_script() -> bytes:
    return bytes([RIGOR_PUSH_1, OP_PUSH32]) + RES2_TAG + bytes(28)


def canonical_p2pkh_script() -> bytes:
    return bytes([RIGOR_RESERVED_LEGACY_DUP, RIGOR_RESERVED_LEGACY_HASH160, OP_PUSH20]) + bytes(20) + bytes([RIGOR_RESERVED_LEGACY_EQUALVERIFY, RIGOR_RESERVED_LEGACY_CHECKSIG])


def canonical_p2sh_script() -> bytes:
    return bytes([RIGOR_RESERVED_LEGACY_HASH160, OP_PUSH20]) + bytes(20) + bytes([RIGOR_RESERVED_LEGACY_EQUAL])


def canonical_v1_len20_script() -> bytes:
    return bytes([RIGOR_PUSH_1, OP_PUSH20]) + bytes(20)


def canonical_v2_32_script() -> bytes:
    return bytes([RIGOR_PUSH_2, OP_PUSH32]) + bytes(32)


# RFC 8032 Ed25519 test vector 1 public key (C++ TestEd25519PubKey).
RFC8032_PK1 = bytes.fromhex(
    "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a"
)


def canonical_native_res_script() -> bytes:
    """Witness v1 / 32 / RFC8032 pk1. All-zero v1 is not C++ native RES (curve)."""
    return bytes([RIGOR_PUSH_1, OP_PUSH32]) + RFC8032_PK1


def canonical_converge_script() -> bytes:
    return bytes([OP_PUSH20]) + bytes(20) + bytes([RIGOR_MARKER_CONVERGE])


def eval_script_asset(inp: Dict[str, Any]) -> Dict[str, Any]:
    """Positive classifier table: asset / allowed / is_res. Script is the asset."""
    spk = bytes.fromhex(inp["script_hex"])
    return {
        "asset": asset_from_script(spk),
        "is_allowed": is_allowed_script(spk),
        "is_res": is_res_script(spk),
    }


def canonical_pcorb_stack() -> List[bytes]:
    return [bytes(sz) for sz in PCORB_REQUIRED_SIZES]


def eval_script_mint_neg(inp: Dict[str, Any]) -> Dict[str, Any]:
    """Evaluate a ScriptMintNeg vector. accepted=true is a generator/oracle failure."""
    kind = inp["kind"]
    base: Dict[str, Any] = {"accepted": False, "reject": "unknown"}

    if kind in ("script_class", "domain_cross"):
        spk = bytes.fromhex(inp["script_hex"])
        claimed = inp.get("claimed_asset")
        reason = classify_script_reject(spk, claimed)
        base["is_allowed"] = is_allowed_script(spk)
        base["is_res"] = is_res_script(spk)
        base["asset"] = asset_from_script(spk)
        if kind == "domain_cross":
            # L1 extras are frozen at the lottery-era W snapshot so this
            # invariant family stays byte-identical. Primary reject is script.
            base["W"] = SCRIPT_MINT_NEG_LOCKED_W
            base["dual_pool_reject"] = classify_dual_pool_reject_locked(
                inp["utxo"], inp["tx"], SCRIPT_MINT_NEG_LOCKED_W
            )
        if reason is None:
            if kind == "domain_cross" and base.get("dual_pool_reject"):
                base["reject"] = "asset_tag_mismatch"
                return base
            return {
                "accepted": True,
                "reject": None,
                "is_allowed": base["is_allowed"],
                "is_res": base["is_res"],
                "asset": base["asset"],
                **({"W": base["W"], "dual_pool_reject": None} if kind == "domain_cross" else {}),
            }
        base["reject"] = reason
        return base

    if kind == "spend_cvg2":
        spk = bytes.fromhex(inp["script_hex"])
        reason = classify_spend_cvg2(spk)
        if reason is None:
            return {"accepted": True, "reject": None}
        base["reject"] = reason
        return base

    if kind in ("pcorb_shape", "mint_witness_cross"):
        stack = [bytes.fromhex(h) for h in inp["stack_hex"]]
        reason = classify_pcorb_reject(stack)
        if kind == "mint_witness_cross":
            ins = gather_inputs(inp["utxo"], inp["tx"]["spends"])
            mint_ok = ins is not None and res_mint_shape_locked(
                inp["tx"], ins, SCRIPT_MINT_NEG_LOCKED_W
            )
            base["W"] = SCRIPT_MINT_NEG_LOCKED_W
            base["mint_shape_would_pass"] = mint_ok
        if reason is None:
            if kind == "mint_witness_cross" and not base.get("mint_shape_would_pass"):
                base["reject"] = "mint_shape"
                return base
            return {
                "accepted": True,
                "reject": None,
                **(
                    {"W": base["W"], "mint_shape_would_pass": base["mint_shape_would_pass"]}
                    if kind == "mint_witness_cross"
                    else {}
                ),
            }
        base["reject"] = reason
        return base

    raise ValueError(f"unknown ScriptMintNeg kind {kind}")


def cvg_out_is_cvg2_positive(out: Dict[str, Any]) -> bool:
    return int(out["value"]) > 0 and is_cvg2_script(bytes.fromhex(out["script_hex"]))


def classify_cvg_tx_reject(outs: List[Dict[str, Any]]) -> Optional[str]:
    """Reject reason for an abstract Convergence tx. None means the shape is valid."""
    if outs and all(not is_cvg2_script(bytes.fromhex(o["script_hex"])) for o in outs):
        return "not_cvg2_script"
    n = sum(1 for o in outs if cvg_out_is_cvg2_positive(o))
    if n != 1:
        if n == 0 and outs and all(int(o["value"]) == 0 for o in outs):
            return "amount_zero"
        return "cvg_count"
    o = next(o for o in outs if cvg_out_is_cvg2_positive(o))
    if o["asset"] != "ORU":
        return "wrong_asset"
    if int(o["value"]) < MIN_CONVERGENCE_AMOUNT:
        return "amount_below_min"
    return None


def eval_invalid_cvg(inp: Dict[str, Any]) -> Dict[str, Any]:
    """Evaluate an InvalidCvg vector. accepted=true is a generator/oracle failure:
    every row is a reject path of Convergence-tx shape or root well-formedness."""
    kind = inp["kind"]
    base: Dict[str, Any] = {"accepted": False, "reject": "unknown"}

    if kind == "cvg_tx":
        reason = classify_cvg_tx_reject(inp["outs"])
        if reason is None:
            return {"accepted": True, "reject": None}
        base["reject"] = reason
        return base

    if kind == "committed_c":
        txids = [bytes.fromhex(h) for h in inp["txids_hex"]]
        claimed = bytes.fromhex(inp["claimed_hex"])
        honest = compute_convergence_root_from_txids(txids)
        base["convergence_root"] = honest.hex()
        if claimed == honest and len(claimed) == 32:
            return {"accepted": True, "reject": None, "convergence_root": honest.hex()}
        if len(claimed) != 32:
            base["reject"] = "malformed_length"
            return base
        base["reject"] = "c_mismatch"
        return base

    if kind == "malformed_preimage":
        txids = [bytes.fromhex(h) for h in inp["txids_hex"]]
        payload = bytes.fromhex(inp["bad_payload_hex"])
        honest = compute_convergence_root_from_txids(txids)
        bad = sha256d(payload)
        base["convergence_root"] = honest.hex()
        base["bad_digest"] = bad.hex()
        if bad == honest:
            return {
                "accepted": True,
                "reject": None,
                "convergence_root": honest.hex(),
                "bad_digest": bad.hex(),
            }
        base["reject"] = inp["mode"]
        return base

    if kind == "malformed_length":
        txids = [bytes.fromhex(h) for h in inp["txids_hex"]]
        claimed = bytes.fromhex(inp["claimed_hex"])
        honest = compute_convergence_root_from_txids(txids)
        base["convergence_root"] = honest.hex()
        if len(claimed) == 32 and claimed == honest:
            return {"accepted": True, "reject": None, "convergence_root": honest.hex()}
        base["reject"] = "malformed_length"
        return base

    if kind == "dup_txid":
        txids = [bytes.fromhex(h) for h in inp["txids_hex"]]
        collapsed = [bytes.fromhex(h) for h in inp["collapsed_hex"]]
        c_dup = compute_convergence_root_from_txids(txids)
        c_one = compute_convergence_root_from_txids(collapsed)
        base["convergence_root"] = c_dup.hex()
        base["collapsed_root"] = c_one.hex()
        if c_dup == c_one:
            return {
                "accepted": True,
                "reject": None,
                "convergence_root": c_dup.hex(),
                "collapsed_root": c_one.hex(),
            }
        base["reject"] = "dup_not_collapsed"
        return base

    if kind == "nlt2_claim":
        txids = [bytes.fromhex(h) for h in inp["txids_hex"]]
        claimed = bytes.fromhex(inp["claimed_hex"])
        honest = compute_convergence_root_from_txids(txids)
        base["convergence_root"] = honest.hex()
        if claimed == honest:
            return {"accepted": True, "reject": None, "convergence_root": honest.hex()}
        base["reject"] = inp.get("mode", "nlt2_nonzero_claim")
        return base

    raise ValueError(f"unknown InvalidCvg kind {kind}")


def list_convergence_commitment_txids(txs: List[ConvergenceTx]) -> List[bytes]:
    """
    ``ListConvergenceRootCommitmentTxids`` predicate surface (block-entry view).

    Caller supplies ``is_valid`` (= CheckTxInputs ok). ``fee_positive`` is relay
    policy, not F membership (zero-fee burns that otherwise verify still enter F).
    """
    out: List[bytes] = []
    for tx in txs:
        if not tx.is_valid:
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


def encode_sigma_preimage(
    txid: bytes,
    prev_block_hash: bytes,
    height: int,
    convergence_root: bytes,
) -> bytes:
    """Canonical 100-byte σ preimage. Must match Coq [sigma_preimage_bytes] and C++ buf[]."""
    assert len(txid) == TXID_SIZE
    assert len(prev_block_hash) == TXID_SIZE
    assert len(convergence_root) == TXID_SIZE
    height_le = (height & 0xFFFFFFFF).to_bytes(4, "little")
    buf = txid + convergence_root + prev_block_hash + height_le
    assert len(buf) == SIGMA_INPUT_SIZE
    return buf


def exclude_already_paid(candidate_txids: list, already_paid: list) -> list:
    """A-9-C: drop any candidate whose txid a mint inside the window already paid.

    Mirrors [exclude_already_paid] in WindowExclusion.v and the paid-filter in
    FinalizeWindowedDrawSet. Order of survivors matches candidate order.
    """
    paid = set(already_paid)
    return [t for t in candidate_txids if t not in paid]


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

    ``H_i < T · f_i`` with ``T = res_probability_target_int`` (production: ``E = 246``,
    windowed; mint cost ``C = 1024`` ORU). Historical RFC-014 parameter used
    unwindowed ``2^240`` (vacuous / then unit-corrected). Do not treat ``2^240`` as
    the shipping RHS.
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
    informational digest of the inequality-eligible subset. Same sorted-concat-then-
    SHA256d shape as the convergence root for every nonempty list. Empty DCR hashes
    empty concat (not zeros); empty C is zeros. Coq: ``draw_candidate_root_from_flags``
    in ``DrawCandidateRoot.v``.
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
    txid_to_weight = {tx.txid: tx.convergence_amount for tx in txs if tx.is_valid}
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


# =============================================================================
# Dual-pool / mint-shape / cap (model predicates; not a C++ refinement)
#
# Mirrors ReplayReorgCore/StateTransition.v and ConnectBlockMint.v so negative
# golden vectors can be rejected by the Python oracle and the Coq harness on
# the same boolean predicates. Script / owner / PCORB / domain are out of scope.
# =============================================================================

ASSET_NONE = "NONE"


def txout_asset_value(kind: str, out: Dict[str, Any]) -> int:
    return int(out["value"]) if out["asset"] == kind else 0


def sum_asset(kind: str, outs: List[Dict[str, Any]]) -> int:
    return sum(txout_asset_value(kind, o) for o in outs)


def count_asset(kind: str, outs: List[Dict[str, Any]]) -> int:
    return sum(1 for o in outs if o["asset"] == kind)


def utxo_lookup(utxo: List[Dict[str, Any]], op: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    for entry in utxo:
        if entry["id"] == op["id"] and entry["vout"] == op["vout"]:
            return entry
    return None


def gather_inputs(
    utxo: List[Dict[str, Any]], spends: List[Dict[str, Any]]
) -> Optional[List[Dict[str, Any]]]:
    ins: List[Dict[str, Any]] = []
    for op in spends:
        found = utxo_lookup(utxo, op)
        if found is None:
            return None
        ins.append({"value": found["value"], "asset": found["asset"]})
    return ins


def no_res_to_oru(ins: List[Dict[str, Any]], outs: List[Dict[str, Any]]) -> bool:
    if sum_asset(ASSET_ORU, ins) == 0:
        return count_asset(ASSET_ORU, outs) == 0
    return True


def no_oru_to_res(ins: List[Dict[str, Any]], outs: List[Dict[str, Any]]) -> bool:
    if sum_asset(ASSET_RES, ins) == 0:
        return count_asset(ASSET_RES, outs) == 0
    return True


def valid_ordinary_dual_pool(ins: List[Dict[str, Any]], tx: Dict[str, Any]) -> bool:
    creates = tx["creates"]
    return (
        sum_asset(ASSET_RES, ins) >= sum_asset(ASSET_RES, creates)
        and no_res_to_oru(ins, creates)
        and no_oru_to_res(ins, creates)
    )


def res_mint_shape(tx: Dict[str, Any], ins: List[Dict[str, Any]], height: int) -> bool:
    """RFC-020: one RES output, no RES inputs, burn covers R · (2^25+h)^2 ≤ B << 40."""
    creates = tx["creates"]
    res_out = sum_asset(ASSET_RES, creates)
    burn = sum_asset(ASSET_ORU, ins) - sum_asset(ASSET_ORU, creates)
    return (
        res_out > 0
        and sum_asset(ASSET_RES, ins) == 0
        and burn > 0
        and count_asset(ASSET_RES, creates) == 1
        and check_res_mint_arith(res_out, burn, height)
    )


# ScriptMintNeg is an invariant family. SMW-010/011 carry lottery-era L1 extras
# (W = 40e6 mint-equality). Keep those extras pinned so regeneration does not
# flip the invariant digest. Live minting uses res_mint_shape above.
SCRIPT_MINT_NEG_LOCKED_W = 40_000_000


def res_mint_shape_locked(tx: Dict[str, Any], ins: List[Dict[str, Any]], reward: int) -> bool:
    creates = tx["creates"]
    return (
        reward > 0
        and sum_asset(ASSET_RES, ins) == 0
        and sum_asset(ASSET_ORU, ins) > 0
        and count_asset(ASSET_RES, creates) == 1
        and sum_asset(ASSET_RES, creates) == reward
    )


def classify_dual_pool_reject_locked(
    utxo: List[Dict[str, Any]], tx: Dict[str, Any], reward: int
) -> Optional[str]:
    ins = gather_inputs(utxo, tx["spends"])
    if ins is None:
        return "missing_input"
    if res_mint_shape_locked(tx, ins, reward) or valid_ordinary_dual_pool(ins, tx):
        return None
    creates = tx["creates"]
    res_in = sum_asset(ASSET_RES, ins)
    res_out = sum_asset(ASSET_RES, creates)
    oru_in = sum_asset(ASSET_ORU, ins)
    n_res_out = count_asset(ASSET_RES, creates)
    if res_in == 0 and oru_in > 0 and n_res_out > 0:
        if reward <= 0:
            return "unauthorized_mint"
        if n_res_out != 1:
            return "mint_wrong_count"
        if res_out != reward:
            return "mint_wrong_amount"
    if res_in == 0 and n_res_out > 0:
        return "oru_to_res"
    if oru_in == 0 and count_asset(ASSET_ORU, creates) > 0:
        return "res_to_oru"
    if res_in < res_out:
        return "ordinary_inflation"
    return "dual_pool"


def valid_dual_pool_tx(utxo: List[Dict[str, Any]], tx: Dict[str, Any], height: int) -> bool:
    ins = gather_inputs(utxo, tx["spends"])
    if ins is None:
        return False
    if res_mint_shape(tx, ins, height):
        return True
    return valid_ordinary_dual_pool(ins, tx)


def classify_dual_pool_reject(
    utxo: List[Dict[str, Any]], tx: Dict[str, Any], height: int
) -> Optional[str]:
    """None means the tx is accepted. Otherwise a stable reject tag."""
    ins = gather_inputs(utxo, tx["spends"])
    if ins is None:
        return "missing_input"
    if res_mint_shape(tx, ins, height) or valid_ordinary_dual_pool(ins, tx):
        return None
    creates = tx["creates"]
    res_in = sum_asset(ASSET_RES, ins)
    res_out = sum_asset(ASSET_RES, creates)
    oru_in = sum_asset(ASSET_ORU, ins)
    burn = oru_in - sum_asset(ASSET_ORU, creates)
    n_res_out = count_asset(ASSET_RES, creates)
    n_oru_out = count_asset(ASSET_ORU, creates)
    if res_in == 0 and oru_in > 0 and n_res_out > 0:
        if not res_mint_authorized(height):
            return "unauthorized_mint"
        if n_res_out != 1:
            return "mint_wrong_count"
        if not check_res_mint_arith(res_out, burn, height):
            return "burn_insufficient"
    if res_in == 0 and n_res_out > 0:
        return "oru_to_res"
    if oru_in == 0 and n_oru_out > 0:
        return "res_to_oru"
    if res_in < res_out:
        return "ordinary_inflation"
    return "dual_pool"


def valid_res_mint_obligation(height: int, draw_won: bool, mint_count: int) -> bool:
    """Draw-era H-1 pin, kept for EdgeCross window_obligation rows.

    RFC-019 production minting does not depend on a winner. These predicates
    only evaluate leftover golden vectors that still name draw_won.
    """
    if res_mint_authorized(height):
        return mint_count == 1 if draw_won else mint_count == 0
    return mint_count == 0


def winner_mismatch_path(height: int, draw_won: bool, mint_count: int) -> bool:
    return res_mint_authorized(height) and draw_won and mint_count != 1


def unauthorized_mint_path(height: int, mint_count: int) -> bool:
    return (not res_mint_authorized(height)) and mint_count != 0


def without_winner_mint_path(draw_won: bool, mint_count: int) -> bool:
    return (not draw_won) and mint_count != 0


def cb_res_attempted_amount(height: int, mint_count: int) -> int:
    return 0 if mint_count == 0 else mint_count


def valid_res_cap(res_supply: int, height: int, mint_count: int) -> bool:
    return res_supply + cb_res_attempted_amount(height, mint_count) <= RES_MAX_SUPPLY


def valid_header_connect(cm_height: int, hdr_height: int) -> bool:
    return hdr_height == cm_height + 1


def valid_chainwork_extend(cm_chainwork: int, hdr_chainwork: int) -> bool:
    return hdr_chainwork > cm_chainwork


def cb_expected_oru_subsidy(height: int) -> int:
    if height == 0:
        return 0
    shift = min(height // 210_000, 3)
    return 100_000_000_000 >> shift


def cb_oru_subsidy_ok(height: int, oru_subsidy: int) -> bool:
    return oru_subsidy <= cb_expected_oru_subsidy(height)


def valid_block_connect_model(state: Dict[str, Any], block: Dict[str, Any]) -> bool:
    height = int(block["height"])
    return (
        valid_header_connect(int(state["cm_height"]), height)
        and valid_chainwork_extend(int(state["cm_chainwork"]), int(block["chainwork"]))
        and cb_oru_subsidy_ok(height, int(block.get("oru_subsidy", 0)))
        and valid_res_mint_obligation(height, bool(block["draw_won"]), int(block["mint_count"]))
        and valid_res_cap(int(state["res_supply"]), height, int(block["mint_count"]))
    )


def apply_tx_utxo(
    utxo: List[Dict[str, Any]], tx: Dict[str, Any]
) -> Optional[List[Dict[str, Any]]]:
    for op in tx["spends"]:
        if utxo_lookup(utxo, op) is None:
            return None
    spent = {(op["id"], op["vout"]) for op in tx["spends"]}
    new_utxo = [e for e in utxo if (e["id"], e["vout"]) not in spent]
    for i, created in enumerate(tx["creates"]):
        new_utxo.append(
            {"id": tx["id"], "vout": i, "value": created["value"], "asset": created["asset"]}
        )
    return new_utxo


def _spk_bytes(o: Dict[str, Any]) -> bytes:
    """Script bytes on a model output / UTXO entry. Missing = empty (fixture)."""
    hx = o.get("script_hex")
    if hx:
        return bytes.fromhex(hx)
    spk = o.get("script")
    if not spk:
        return b""
    if isinstance(spk, (bytes, bytearray)):
        return bytes(spk)
    if isinstance(spk, str):
        return bytes.fromhex(spk)
    return bytes(spk)


def created_script_shape_ok(o: Dict[str, Any]) -> bool:
    """L1 mirror of Coq [created_script_shape_ok]. Empty script is INVALID."""
    spk = _spk_bytes(o)
    if is_allowed_script(spk):
        return True
    return bool(is_op_return(spk) and int(o.get("value", 0)) == 0)


def pcorb_shape_ok_tx(tx: Dict[str, Any]) -> bool:
    """L1 mirror of Coq [pcorb_shape_ok]. Absent / None skips the conjunct."""
    if "pcorb" not in tx and "stack_hex" not in tx:
        return True
    stack = tx.get("pcorb")
    if stack is None and "stack_hex" in tx:
        stack = [bytes.fromhex(h) for h in tx["stack_hex"]]
    if stack is None:
        return True
    items = [bytes.fromhex(x) if isinstance(x, str) else bytes(x) for x in stack]
    return parse_pcorb_shape(items)


def no_spend_cvg2_model(utxo: List[Dict[str, Any]], tx: Dict[str, Any]) -> bool:
    """Spend-CVG2 shape. Looks at the UTXO entry (gather_inputs strips script)."""
    for op in tx.get("spends") or []:
        found = utxo_lookup(utxo, op)
        if found is None:
            continue
        if is_cvg2_script(_spk_bytes(found)):
            return False
    return True


def valid_script_shape_tx(utxo: List[Dict[str, Any]], tx: Dict[str, Any]) -> bool:
    """L1 mirror of Coq [valid_script_shape_tx]. Not an interpreter."""
    creates = tx.get("creates") or []
    if not all(created_script_shape_ok(o) for o in creates):
        return False
    if not pcorb_shape_ok_tx(tx):
        return False
    return no_spend_cvg2_model(utxo, tx)


def apply_tx_model(
    utxo: List[Dict[str, Any]], tx: Dict[str, Any], height: int
) -> Optional[List[Dict[str, Any]]]:
    u2 = apply_tx_utxo(utxo, tx)
    if u2 is None:
        return None
    if not valid_dual_pool_tx(utxo, tx, height):
        return None
    if not valid_script_shape_tx(utxo, tx):
        return None
    return u2


def apply_block_model(state: Dict[str, Any], block: Dict[str, Any]) -> bool:
    if not valid_block_connect_model(state, block):
        return False
    height = int(state["cm_height"]) + 1
    utxo = list(state["utxo"])
    for tx in block.get("txs", []):
        nxt = apply_tx_model(utxo, tx, height)
        if nxt is None:
            return False
        utxo = nxt
    return True


def classify_obligation_reject(height: int, draw_won: bool, mint_count: int) -> Optional[str]:
    if unauthorized_mint_path(height, mint_count):
        return "unauthorized_mint"
    if without_winner_mint_path(draw_won, mint_count):
        return "without_winner"
    if winner_mismatch_path(height, draw_won, mint_count):
        return "winner_mismatch"
    if not valid_res_mint_obligation(height, draw_won, mint_count):
        return "obligation"
    return None


def eval_negative_core(inp: Dict[str, Any]) -> Dict[str, Any]:
    """Evaluate a NegativeCore vector. accepted=true is a generator/oracle failure
    for this family: every row is a reject path of the Coq monetary model."""
    kind = inp["kind"]
    height = int(inp["height"])
    w = get_res_mint_reward(height)
    base = {"W": w, "accepted": False, "reject": "unknown"}

    if kind == "dual_pool_tx":
        reason = classify_dual_pool_reject(inp["utxo"], inp["tx"], height)
        if reason is None:
            return {"W": w, "accepted": True, "reject": None}
        base["reject"] = reason
        return base

    if kind == "mint_shape":
        reason = classify_dual_pool_reject(inp["utxo"], inp["tx"], height)
        if reason is None:
            return {"W": w, "accepted": True, "reject": None}
        base["reject"] = reason
        return base

    if kind == "connect_cap":
        supply = int(inp["res_supply"])
        mint_count = int(inp["mint_count"])
        if valid_res_cap(supply, height, mint_count):
            return {"W": w, "accepted": True, "reject": None}
        base["reject"] = "cap_overshoot"
        return base

    if kind == "connect_obligation":
        reason = classify_obligation_reject(
            height, bool(inp["draw_won"]), int(inp["mint_count"])
        )
        if reason is None:
            return {"W": w, "accepted": True, "reject": None}
        base["reject"] = reason
        return base

    if kind == "apply_block":
        state = {
            "utxo": inp["utxo"],
            "cm_height": height - 1,
            "cm_chainwork": 0,
            "res_supply": int(inp.get("res_supply", 0)),
        }
        block = {
            "height": height,
            "chainwork": 1,
            "draw_won": bool(inp.get("draw_won", True)),
            "mint_count": int(inp.get("mint_count", 1)),
            "oru_subsidy": int(inp.get("oru_subsidy", 0)),
            "txs": inp["txs"],
        }
        if apply_block_model(state, block):
            return {"W": w, "accepted": True, "reject": None}
        base["reject"] = "apply_block_none"
        return base

    raise ValueError(f"unknown NegativeCore kind {kind}")


def eval_edge_cross(inp: Dict[str, Any]) -> Dict[str, Any]:
    """Evaluate an EdgeCross vector. accepted=true is a generator/oracle failure:
    every row is a cross-family reject or a window/DCR/selection disagreement."""
    kind = inp["kind"]
    base: Dict[str, Any] = {"accepted": False, "reject": "unknown"}

    if kind in ("script_dual_pool", "spend_cvg2_dual_pool"):
        height = int(inp["height"])
        w = get_res_mint_reward(height)
        spk = bytes.fromhex(inp["script_hex"])
        dual = classify_dual_pool_reject(inp["utxo"], inp["tx"], height)
        shape_ok = valid_script_shape_tx(inp["utxo"], inp["tx"])
        if kind == "spend_cvg2_dual_pool":
            script_reason = classify_spend_cvg2(spk)
        else:
            script_reason = classify_script_reject(spk, inp.get("claimed_asset"))
        base["W"] = w
        base["dual_pool_reject"] = dual
        base["shape_reject"] = None if shape_ok else "script_shape"
        if script_reason is None or dual is None:
            if script_reason is None and dual is None:
                return {
                    "accepted": True,
                    "reject": None,
                    "W": w,
                    "dual_pool_reject": None,
                    "shape_reject": base["shape_reject"],
                }
            missing = "script" if script_reason is None else "dual_pool"
            raise ValueError(f"{kind} missing {missing} reject")
        base["reject"] = script_reason
        return base

    if kind in ("root_mint", "root_cap"):
        height = int(inp["height"])
        w = get_res_mint_reward(height)
        root_inp = inp["root"]
        mint_inp = dict(inp["mint"])
        mint_inp.setdefault("height", height)
        root_got = eval_invalid_cvg(root_inp)
        mint_got = eval_negative_core(mint_inp)
        base["W"] = w
        base["root_reject"] = root_got.get("reject")
        base["mint_reject"] = mint_got.get("reject")
        for key in ("convergence_root", "bad_digest"):
            if key in root_got:
                base[key] = root_got[key]
        if root_got.get("accepted") or mint_got.get("accepted"):
            raise ValueError(f"{kind} one half unexpectedly accepted")
        if kind == "root_cap" and mint_got.get("reject") != "cap_overshoot":
            raise ValueError("root_cap mint half must be cap_overshoot")
        base["reject"] = f"{root_got['reject']}+{mint_got['reject']}"
        return base

    if kind in ("window_dcr_sel", "window_obligation"):
        txids = [bytes.fromhex(h) for h in inp["txids_hex"]]
        fees = list(inp["fees"])
        paid = [bytes.fromhex(h) for h in inp["already_paid_hex"]]
        prev = bytes.fromhex(inp["prev_block_hash_hex"])
        height = int(inp["height"])
        t_int = res_probability_target_from_bytes(bytes.fromhex(inp["T_hex"]))
        surv = exclude_already_paid(txids, paid)
        surv_fees = [fees[txids.index(x)] for x in surv]
        r = run_selection(surv, surv_fees, height, prev, t_int)
        r_full = run_selection(txids, fees, height, prev, t_int)
        dcr = convergence_draw_eligible_root(surv, r.eligible)
        dcr_full = convergence_draw_eligible_root(txids, r_full.eligible)
        win = -1 if r.winner_index is None or r.winner_index < 0 else txids.index(surv[r.winner_index])
        mode = inp["mode"]
        zeros = bytes(32)
        base.update(
            {
                "reject": mode,
                "surviving_hex": [x.hex() for x in surv],
                "dropped": len(txids) - len(surv),
                "convergence_root": r.convergence_root.hex(),
                "draw_candidate_root": dcr.hex(),
                "eligible": r.eligible,
                "winner_index": win,
                "full_convergence_root": r_full.convergence_root.hex(),
                "full_draw_candidate_root": dcr_full.hex(),
                "full_winner_index": r_full.winner_index,
                "c_is_zero": r.convergence_root == zeros,
                "dcr_neq_zero": dcr != zeros,
                "dcr_neq_c": dcr != r.convergence_root,
            }
        )
        if mode == "empty_after_pay":
            if surv or win != -1 or r.convergence_root != zeros or dcr == zeros:
                raise ValueError("empty_after_pay did not empty F / zero C / nonempty DCR / no winner")
        elif mode in ("singleton_nlt2", "singleton_hashes"):
            if len(surv) != 1 or r.convergence_root == zeros or dcr != r.convergence_root:
                raise ValueError("singleton_hashes C/DCR shape failed")
            if win < 0 or r_full.winner_index < 0:
                raise ValueError("singleton_nlt2 expected a survivor winner and a full-set winner")
        elif mode == "drop_winner_moves":
            if not paid or dcr == dcr_full or win == r_full.winner_index:
                raise ValueError("drop_winner_moves did not separate DCR/winner")
        else:
            raise ValueError(f"unknown window_dcr_sel mode {mode}")
        if kind == "window_dcr_sel":
            return base
        ob = eval_negative_core(
            {
                "kind": "connect_obligation",
                "height": height,
                "draw_won": bool(inp["draw_won"]),
                "mint_count": int(inp["mint_count"]),
            }
        )
        if ob.get("accepted"):
            raise ValueError("window_obligation mint-obligation half unexpectedly accepted")
        if mode == "empty_after_pay" and ob.get("reject") != "without_winner":
            raise ValueError("empty_after_pay obligation must be without_winner")
        if mode == "drop_winner_moves" and ob.get("reject") != "winner_mismatch":
            raise ValueError("drop_winner_moves obligation must be winner_mismatch")
        base["obligation_reject"] = ob["reject"]
        base["draw_won"] = bool(inp["draw_won"])
        base["mint_count"] = int(inp["mint_count"])
        base["reject"] = f"{mode}+{ob['reject']}"
        return base

    if kind == "pcorb_dual_pool":
        height = int(inp["height"])
        w = get_res_mint_reward(height)
        dual = classify_dual_pool_reject(inp["utxo"], inp["tx"], height)
        shape_ok = valid_script_shape_tx(inp["utxo"], inp["tx"])
        stack = [bytes.fromhex(h) for h in inp["tx"]["stack_hex"]]
        pcorb_reason = classify_pcorb_reject(stack)
        base["W"] = w
        base["dual_pool_reject"] = dual
        base["shape_reject"] = None if shape_ok else "script_shape"
        if pcorb_reason is None or dual is None or shape_ok:
            if pcorb_reason is None and dual is None:
                return {
                    "accepted": True,
                    "reject": None,
                    "W": w,
                    "dual_pool_reject": None,
                    "shape_reject": base["shape_reject"],
                }
            raise ValueError("pcorb_dual_pool must fail PCORB shape and dual-pool")
        base["reject"] = pcorb_reason
        return base

    if kind == "schedule_mint":
        height = int(inp["height"])
        w = get_res_mint_reward(height)
        mint_got = eval_negative_core(dict(inp["mint"], height=height))
        neighbour = inp.get("neighbour_height")
        base["W"] = w
        base["mint_reject"] = mint_got.get("reject")
        if neighbour is not None:
            wn = get_res_mint_reward(int(neighbour))
            base["W_neighbour"] = wn
            if wn == w:
                raise ValueError("schedule boundary W did not jump")
        if mint_got.get("accepted"):
            raise ValueError("schedule_mint mint half unexpectedly accepted")
        base["reject"] = mint_got["reject"]
        return base

    if kind == "schedule_cap":
        height = int(inp["height"])
        w = get_res_mint_reward(height)
        if w <= 0:
            raise ValueError("schedule_cap requires W(h)>0")
        shape_got = eval_negative_core(dict(inp["shape"], height=height))
        cap_got = eval_negative_core(dict(inp["cap"], height=height))
        if shape_got.get("accepted") or cap_got.get("accepted"):
            raise ValueError("schedule_cap one half unexpectedly accepted")
        if cap_got.get("reject") != "cap_overshoot":
            raise ValueError("schedule_cap cap half must be cap_overshoot")
        base["W"] = w
        base["shape_reject"] = shape_got["reject"]
        base["cap_reject"] = "cap_overshoot"
        base["reject"] = f"{shape_got['reject']}+cap_overshoot"
        return base

    raise ValueError(f"unknown EdgeCross kind {kind}")


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
