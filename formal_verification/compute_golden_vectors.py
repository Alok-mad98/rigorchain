#!/usr/bin/env python3
"""
Golden Test Vector Computation
RFC-000-FINAL Compliant
"""

import hashlib
import json
import struct

def sha256(data: bytes) -> bytes:
    """Single SHA256"""
    return hashlib.sha256(data).digest()

def hash256(data: bytes) -> bytes:
    """
    RFC-000 §4: Hash256(x) = SHA256(SHA256(x))
    Bitcoin canonical double SHA
    """
    return sha256(sha256(data))

def uint32_le(value: int) -> bytes:
    """
    RFC-000 §3: uint32 → 4 bytes (little-endian)
    """
    return struct.pack('<I', value)

def compute_convergence_root(txids: list) -> bytes:
    """
    RFC-000 §6: CONVERGENCE ROOT
    C = Hash256(concat(sort(txid_i)))
    
    Sorting Rules (FULLY LOCKED):
    Sort using unsigned byte-wise lexicographical order
    Compare: uint8_t per byte
    
    Boundary Conditions:
    IF N < 2: C = 32 zero bytes. RFC-015 allows N=1 selection while preserving
    this root boundary.
    """
    if len(txids) < 2:
        return b'\x00' * 32
    
    # Sort using unsigned byte-wise lexicographical order
    sorted_txids = sorted(txids)
    
    # Concatenate (no delimiters, no length prefixes)
    concatenated = b''.join(sorted_txids)
    
    # Hash256
    return hash256(concatenated)

def compute_sigma_hash(txid: bytes, C: bytes, prev_block_hash: bytes, height: int) -> bytes:
    """
    RFC-000 §4 (Sigma-PoW-01 / SECURITY-001-A8): CANONICAL HASHING
    σ_i Input (FULLY LOCKED):
    [32] txid_i
    [32] ConvergenceRoot (C)
    [32] B_{h-1} (parent block hash, verbatim)
    [4 ] height (LE) — the payout height
    TOTAL = 100 bytes EXACT

    Rules (MUST):
    len(input) MUST == 100
    Otherwise → REJECT (bad-hash-length)

    `target_prev` is deliberately absent: B_{h-1} is a hash of the parent header,
    which already contains nBits.
    """
    buf = bytearray(100)
    buf[0:32]   = txid
    buf[32:64]  = C
    buf[64:96]  = prev_block_hash
    buf[96:100] = uint32_le(height)
    return hash256(bytes(buf))

def is_eligible(H: bytes, T: bytes, f: int) -> bool:
    """
    RFC-000 §2: SINGLE INEQUALITY LAW
    H_i < T · f_i
    
    Implementation (MUST):
    H_i * f_j < H_j * f_i
    
    FORBIDDEN:
    ❌ division
    ❌ floating point
    ❌ normalization
    ❌ approximation
    
    Constraints:
    f_i MUST be integer
    f_i MUST >= 1
    """
    if f == 0:
        return False  # f_i MUST >= 1
    
    # Convert to integers for comparison
    H_int = int.from_bytes(H, 'big')
    T_int = int.from_bytes(T, 'big')
    
    # Compute T * f using arbitrary precision
    T_times_f = T_int * f
    
    # Check H < T * f
    return H_int < T_times_f

def select_winner(txids: list, sigma_hashes: list, fees: list, T: bytes, C: bytes) -> int:
    """
    RFC-000 §1: CORE PRINCIPLE
    Consensus = Deterministic Selection
    
    winner = argmin_i (H_i / f_i)
    
    RFC-000 §9: TIE-BREAK RULE
    IF:
    H_i * f_j == H_j * f_i
    THEN:
    txid_i < txid_j → winner
    """
    if len(txids) == 0:
        return None
    
    winner_index = None
    best_ratio = None
    
    for i, (txid, sigma, fee) in enumerate(zip(txids, sigma_hashes, fees)):
        if not is_eligible(sigma, T, fee):
            continue
        
        H_int = int.from_bytes(sigma, 'big')
        f_int = fee
        
        if winner_index is None:
            winner_index = i
            best_ratio = H_int / f_int
        else:
            # Compare using cross-multiplication to avoid division
            # We want argmin(H_i / f_i)
            # Equivalent to: (H_i * f_best) < (H_best * f_i)
            
            H_best = int.from_bytes(sigma_hashes[winner_index], 'big')
            f_best = fees[winner_index]
            
            lhs = H_int * f_best
            rhs = H_best * f_int
            
            if lhs < rhs:
                winner_index = i
                best_ratio = H_int / f_int
            elif lhs == rhs:
                # RFC-000 §9: TIE-BREAK RULE
                # IF: H_i * f_j == H_j * f_i
                # THEN: txid_i < txid_j → winner
                if txid < txids[winner_index]:
                    winner_index = i
                    best_ratio = H_int / f_int
    
    return winner_index

def bytes_to_hex(b: bytes) -> str:
    """Convert bytes to hex string"""
    return b.hex()

def hex_to_bytes(hex_str: str) -> bytes:
    """Convert hex string to bytes"""
    return bytes.fromhex(hex_str)

# Historical Sigma-CR-01 golden `target_prev` (parent header nBits). Under Sigma-PoW-01
# this value is no longer folded into σ; it remains only for PCORB / witness fixtures
# that still carry the field.
GOLDEN_TARGET_PREV = 0x1d00ffff

def compute_case_002():
    """
    CASE-002: single_tx_winner
    """
    txids = [hex_to_bytes("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")]
    fees = [1]
    height = 1
    prev_hash = hex_to_bytes("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")
    target = hex_to_bytes("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
    
    # Compute C
    C = compute_convergence_root(txids)
    
    # Compute sigma
    sigma = compute_sigma_hash(txids[0], C, prev_hash, height)

    # Check eligibility
    eligible = is_eligible(sigma, target, fees[0])

    # Select winner
    winner_index = select_winner(txids, [sigma], fees, target, C)
    
    result = {
            "name": "single_tx_winner",
        "input": {
            "txids": [bytes_to_hex(txids[0])],
            "fees": fees,
            "height": height,
            "prev_hash": bytes_to_hex(prev_hash),
            "target": bytes_to_hex(target)
        },
        "expected": {
            "C": bytes_to_hex(C),
            "sigma": [bytes_to_hex(sigma)],
            "eligible": [eligible],
            "winner_index": winner_index
        }
    }
    
    return result

def compute_case_003():
    """
    CASE 003 — ordering affects C
    """
    txids = [
        hex_to_bytes("ff" * 32),
        hex_to_bytes("00" * 32)
    ]
    fees = [1, 1]
    height = 10
    prev_hash = hex_to_bytes("11" * 32)
    target = hex_to_bytes("ff" * 32)
    
    # Compute C
    C = compute_convergence_root(txids)
    
    # Compute sigma for each txid
    sigmas = []
    for txid in txids:
        sigma = compute_sigma_hash(txid, C, prev_hash, height)
        sigmas.append(sigma)

    # Check eligibility
    eligible = []
    for sigma, fee in zip(sigmas, fees):
        eligible.append(is_eligible(sigma, target, fee))

    # Select winner
    winner_index = select_winner(txids, sigmas, fees, target, C)

    result = {
        "name": "sorting_order",
        "input": {
            "txids": [bytes_to_hex(txids[0]), bytes_to_hex(txids[1])],
            "fees": fees,
            "height": height,
            "prev_hash": bytes_to_hex(prev_hash),
            "target": bytes_to_hex(target)
        },
        "expected": {
            "C": bytes_to_hex(C),
            "sigma": [bytes_to_hex(s) for s in sigmas],
            "eligible": eligible,
            "winner_index": winner_index
        }
    }
    
    return result

def compute_case_004():
    """
    CASE 004 — no winner
    """
    txids = [
        hex_to_bytes("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
        hex_to_bytes("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")
    ]
    fees = [1, 1]
    height = 5
    prev_hash = hex_to_bytes("cc" * 32)
    target = hex_to_bytes("0000000000000000000000000000000000000000000000000000000000000001")
    
    # Compute C
    C = compute_convergence_root(txids)
    
    # Compute sigma for each txid
    sigmas = []
    for txid in txids:
        sigma = compute_sigma_hash(txid, C, prev_hash, height)
        sigmas.append(sigma)

    # Check eligibility
    eligible = []
    for sigma, fee in zip(sigmas, fees):
        eligible.append(is_eligible(sigma, target, fee))

    # Select winner
    winner_index = select_winner(txids, sigmas, fees, target, C)

    result = {
        "name": "no_winner",
        "input": {
            "txids": [bytes_to_hex(txids[0]), bytes_to_hex(txids[1])],
            "fees": fees,
            "height": height,
            "prev_hash": bytes_to_hex(prev_hash),
            "target": bytes_to_hex(target)
        },
        "expected": {
            "C": bytes_to_hex(C),
            "sigma": [bytes_to_hex(s) for s in sigmas],
            "eligible": eligible,
            "winner_index": winner_index
        }
    }
    
    return result

def compute_case_005():
    """
    CASE 005 — weight influence
    """
    txids = [
        hex_to_bytes("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
        hex_to_bytes("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")
    ]
    fees = [1, 1000]
    height = 7
    prev_hash = hex_to_bytes("dd" * 32)
    target = hex_to_bytes("ff" * 32)
    
    # Compute C
    C = compute_convergence_root(txids)
    
    # Compute sigma for each txid
    sigmas = []
    for txid in txids:
        sigma = compute_sigma_hash(txid, C, prev_hash, height)
        sigmas.append(sigma)

    # Check eligibility
    eligible = []
    for sigma, fee in zip(sigmas, fees):
        eligible.append(is_eligible(sigma, target, fee))

    # Select winner
    winner_index = select_winner(txids, sigmas, fees, target, C)

    result = {
        "name": "weight_priority",
        "input": {
            "txids": [bytes_to_hex(txids[0]), bytes_to_hex(txids[1])],
            "fees": fees,
            "height": height,
            "prev_hash": bytes_to_hex(prev_hash),
            "target": bytes_to_hex(target)
        },
        "expected": {
            "C": bytes_to_hex(C),
            "sigma": [bytes_to_hex(s) for s in sigmas],
            "eligible": eligible,
            "winner_index": winner_index
        }
    }
    
    return result

def compute_case_006():
    """
    CASE 006 — Tie-break
    """
    txids = [
        hex_to_bytes("1111111111111111111111111111111111111111111111111111111111111111"),
        hex_to_bytes("2222222222222222222222222222222222222222222222222222222222222222")
    ]
    fees = [10, 10]
    height = 9
    prev_hash = hex_to_bytes("ee" * 32)
    target = hex_to_bytes("ff" * 32)
    
    # Compute C
    C = compute_convergence_root(txids)
    
    # Compute sigma for each txid
    sigmas = []
    for txid in txids:
        sigma = compute_sigma_hash(txid, C, prev_hash, height)
        sigmas.append(sigma)

    # Check eligibility
    eligible = []
    for sigma, fee in zip(sigmas, fees):
        eligible.append(is_eligible(sigma, target, fee))

    # Select winner
    winner_index = select_winner(txids, sigmas, fees, target, C)

    result = {
        "name": "tie_break",
        "input": {
            "txids": [bytes_to_hex(txids[0]), bytes_to_hex(txids[1])],
            "fees": fees,
            "height": height,
            "prev_hash": bytes_to_hex(prev_hash),
            "target": bytes_to_hex(target)
        },
        "expected": {
            "C": bytes_to_hex(C),
            "sigma": [bytes_to_hex(s) for s in sigmas],
            "eligible": eligible,
            "winner_index": winner_index
        }
    }
    
    return result

def compute_case_007():
    """
    CASE 007 — large-number test (MUST)
    """
    txids = [
        hex_to_bytes("ff" * 32),
        hex_to_bytes("ee" * 32)
    ]
    fees = [18446744073709551615, 1]  # UINT64_MAX, 1
    height = 100
    prev_hash = hex_to_bytes("aa" * 32)
    target = hex_to_bytes("ff" * 32)
    
    # Compute C
    C = compute_convergence_root(txids)
    
    # Compute sigma for each txid
    sigmas = []
    for txid in txids:
        sigma = compute_sigma_hash(txid, C, prev_hash, height)
        sigmas.append(sigma)

    # Check eligibility
    eligible = []
    for sigma, fee in zip(sigmas, fees):
        eligible.append(is_eligible(sigma, target, fee))

    # Select winner
    winner_index = select_winner(txids, sigmas, fees, target, C)

    result = {
        "name": "big_int",
        "input": {
            "txids": [bytes_to_hex(txids[0]), bytes_to_hex(txids[1])],
            "fees": fees,
            "height": height,
            "prev_hash": bytes_to_hex(prev_hash),
            "target": bytes_to_hex(target)
        },
        "expected": {
            "C": bytes_to_hex(C),
            "sigma": [bytes_to_hex(s) for s in sigmas],
            "eligible": eligible,
            "winner_index": winner_index
        }
    }
    
    return result

if __name__ == "__main__":
    print("Computing Golden Test Vectors...")
    print("=" * 80)
    
    # Compute all cases
    cases = [
        compute_case_002(),
        compute_case_003(),
        compute_case_004(),
        compute_case_005(),
        compute_case_006(),
        compute_case_007()
    ]
    
    # Output results
    for case in cases:
        print(f"\n{case['name']}:")
        print(json.dumps(case, indent=2))
        print("-" * 80)
    
    # Save to JSON file
    output_file = "./formal_verification/rigor_golden_vectors_computed.json"
    with open(output_file, 'w') as f:
        json.dump(cases, f, indent=2)
    
    print(f"\nResults saved to: {output_file}")
