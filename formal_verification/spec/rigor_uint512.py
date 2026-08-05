#!/usr/bin/env python3
"""
uint512 helpers matching ``Consensus::uint512`` and ``rigor_math_detail::Mul256x256Full``
in ``core/src/consensus/rigor_math.h`` (RFC-000 §8, RFC-008 §7).
"""

from __future__ import annotations

TXID_SIZE = 32


def uint256_bytes_to_arith_int(b: bytes) -> int:
    """``UintToArith256`` / ``uint256`` memory order (32 bytes, little-endian limbs)."""
    if len(b) != TXID_SIZE:
        raise ValueError("uint256 must be 32 bytes")
    return int.from_bytes(b, "little")


def arith_int_to_uint256_bytes(n: int) -> bytes:
    return (n % (1 << 256)).to_bytes(TXID_SIZE, "little")


def mul256x256_full(a: int, b: int) -> int:
    """
    ``rigor_math_detail::Mul256x256Full`` — full 512-bit product, no truncation.
    ``a`` and ``b`` must each fit in 256 bits.
    """
    if a < 0 or b < 0:
        raise ValueError("unsigned only")
    if a >= (1 << 256) or b >= (1 << 256):
        raise ValueError("operands must fit in uint256")
    return a * b


def uint512_compare_u256_product(
    h_bytes: bytes, t_int: int, f_uint64: int
) -> bool:
    """
  ``ConvergenceDrawInequalityHolds`` / ``IsEligible``:
  ``uint512(H) < uint512(T) * uint512(f)`` with ``f`` in the low 64 bits.
    """
    if t_int == 0:
        return False
    if f_uint64 < 1:
        return False
    if f_uint64 > 0xFFFFFFFFFFFFFFFF:
        raise ValueError("weight must fit in uint64")
    h = uint256_bytes_to_arith_int(h_bytes)
    left = h
    right = mul256x256_full(t_int & ((1 << 256) - 1), f_uint64)
    return left < right


def winner_cross_multiply_less(
    h_i: bytes, f_i: int, h_j: bytes, f_j: int
) -> bool:
    """
    True iff candidate ``i`` is strictly better than ``j``:
    ``H_i * f_j < H_j * f_i`` in uint512 (RFC-000 §8).
    """
    if f_i < 1 or f_j < 1:
        raise ValueError("weights must be >= 1")
    hi = uint256_bytes_to_arith_int(h_i)
    hj = uint256_bytes_to_arith_int(h_j)
    lhs = mul256x256_full(hi, f_j)
    rhs = mul256x256_full(hj, f_i)
    return lhs < rhs


def winner_cross_multiply_equal(
    h_i: bytes, f_i: int, h_j: bytes, f_j: int
) -> bool:
    hi = uint256_bytes_to_arith_int(h_i)
    hj = uint256_bytes_to_arith_int(h_j)
    return mul256x256_full(hi, f_j) == mul256x256_full(hj, f_i)
