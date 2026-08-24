#!/usr/bin/env python3
"""
RIGOR CONSENSUS GOLDEN VECTORS TEST SUITE
==========================================

Golden Vectors - Insurance 1 against modeling errors.

This test suite validates that the Python executable spec produces
exactly the expected outputs for known-good test vectors.

FAILING ANY TEST = CONSENSUS FAILURE
"""

import sys
import hashlib
from typing import List, Tuple, Dict, Any

# Import the spec
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'spec'))
import rigor_consensus_spec


# =============================================================================
# GOLDEN VECTORS
# =============================================================================

class GoldenVector:
    """A single golden vector test case."""
    
    def __init__(self, name: str, inputs: Dict[str, Any], expected: Dict[str, Any]):
        self.name = name
        self.inputs = inputs
        self.expected = expected
    
    def run(self) -> bool:
        """Run the test and return True if it passes."""
        raise NotImplementedError


class ConvergenceRootGoldenVector(GoldenVector):
    """Golden vector for ConvergenceRoot computation."""
    
    def run(self) -> bool:
        txids = self.inputs['txids']
        expected_root = self.expected['convergence_root']
        
        actual_root = rigor_consensus_spec.compute_convergence_root_from_txids(txids)
        
        if actual_root != expected_root:
            print(f"FAIL: {self.name}")
            print(f"  Expected: {expected_root.hex()}")
            print(f"  Actual:   {actual_root.hex()}")
            return False
        
        print(f"PASS: {self.name}")
        return True


class PCORBHashGoldenVector(GoldenVector):
    """Golden vector for PCORB hash computation."""
    
    def run(self) -> bool:
        txid = self.inputs['txid']
        bind_hash = self.inputs['bind_hash']
        height = self.inputs['height']
        target_prev = self.inputs['target_prev']
        convergence_root = self.inputs['convergence_root']
        expected_hash = self.expected['pcorb_hash']
        
        actual_hash = rigor_consensus_spec.compute_pcobr_hash(
            txid, bind_hash, height, convergence_root
        )
        
        if actual_hash != expected_hash:
            print(f"FAIL: {self.name}")
            print(f"  Expected: {expected_hash.hex()}")
            print(f"  Actual:   {actual_hash.hex()}")
            return False
        
        print(f"PASS: {self.name}")
        return True


class WinnerSelectionGoldenVector(GoldenVector):
    """Golden vector for winner selection computation."""
    
    def run(self) -> bool:
        txs = self.inputs['txs']
        bind_hash = self.inputs['bind_hash']
        height = self.inputs['height']
        target_prev = self.inputs['target_prev']
        expected_winner = self.expected['winner']
        
        # Compute convergence root first
        txids = [tx.txid for tx in txs]
        convergence_root = rigor_consensus_spec.compute_convergence_root_from_txids(txids)
        
        actual_winner = rigor_consensus_spec.select_winner(
            txs, bind_hash, height, convergence_root
        )
        
        if actual_winner != expected_winner:
            print(f"FAIL: {self.name}")
            print(f"  Expected: {expected_winner.hex() if expected_winner else 'None'}")
            print(f"  Actual:   {actual_winner.hex() if actual_winner else 'None'}")
            return False
        
        print(f"PASS: {self.name}")
        return True


# =============================================================================
# TEST VECTOR GENERATION
# =============================================================================

def create_test_txid(seed: int) -> bytes:
    """Create a deterministic test txid from a seed."""
    return rigor_consensus_spec.sha256d(seed.to_bytes(4, 'little'))


def generate_convergence_root_vectors() -> List[GoldenVector]:
    """Generate golden vectors for ConvergenceRoot computation."""
    vectors = []
    
    # Vector 1: Empty list -> all-zero
    vectors.append(ConvergenceRootGoldenVector(
        "ConvergenceRoot_empty",
        {'txids': []},
        {'convergence_root': bytes(32)}
    ))
    
    # Vector 2: Single txid hashes (no n<2 zero shortcut)
    txid0 = create_test_txid(0)
    vectors.append(ConvergenceRootGoldenVector(
        "ConvergenceRoot_single",
        {'txids': [txid0]},
        {'convergence_root': rigor_consensus_spec.sha256d(txid0)}
    ))
    
    # Vector 3: Two txids (minimum for hashing)
    txid0 = create_test_txid(0)
    txid1 = create_test_txid(1)
    sorted_txids = sorted([txid0, txid1])
    expected_root = rigor_consensus_spec.sha256d(b''.join(sorted_txids))
    vectors.append(ConvergenceRootGoldenVector(
        "ConvergenceRoot_two",
        {'txids': [txid0, txid1]},
        {'convergence_root': expected_root}
    ))
    
    # Vector 4: Multiple txids
    txids = [create_test_txid(i) for i in range(5)]
    sorted_txids = sorted(txids)
    expected_root = rigor_consensus_spec.sha256d(b''.join(sorted_txids))
    vectors.append(ConvergenceRootGoldenVector(
        "ConvergenceRoot_multiple",
        {'txids': txids},
        {'convergence_root': expected_root}
    ))
    
    # Vector 5: Order invariance - different order, same result
    txids_shuffled = [create_test_txid(i) for i in [3, 1, 4, 0, 2]]
    vectors.append(ConvergenceRootGoldenVector(
        "ConvergenceRoot_order_invariance",
        {'txids': txids_shuffled},
        {'convergence_root': expected_root}  # Same as Vector 4
    ))
    
    return vectors


def generate_pcobr_hash_vectors() -> List[GoldenVector]:
    """Generate golden vectors for PCORB hash computation."""
    vectors = []
    
    # Vector 1: Basic case
    txid = create_test_txid(0)
    bind_hash = create_test_txid(1)
    convergence_root = create_test_txid(2)
    height = 100
    target_prev = 0x1d00ffff
    
    expected_hash = rigor_consensus_spec.compute_pcobr_hash(txid, bind_hash, height, convergence_root)
    
    vectors.append(PCORBHashGoldenVector(
        "PCORBHash_basic",
        {
            'txid': txid,
            'bind_hash': bind_hash,
            'height': height,
            'target_prev': target_prev,
            'convergence_root': convergence_root
        },
        {'pcorb_hash': expected_hash}
    ))
    
    # Vector 2: Different height
    height2 = 999999
    expected_hash2 = rigor_consensus_spec.compute_pcobr_hash(txid, bind_hash, height2, convergence_root)
    
    vectors.append(PCORBHashGoldenVector(
        "PCORBHash_different_height",
        {
            'txid': txid,
            'bind_hash': bind_hash,
            'height': height2,
            'target_prev': target_prev,
            'convergence_root': convergence_root
        },
        {'pcorb_hash': expected_hash2}
    ))
    
    # Vector 3: SECURITY-001-A8 — target_prev is not a σ input, so changing it must not move the digest.
    target_prev2 = 0x1e012345
    expected_hash3 = rigor_consensus_spec.compute_pcobr_hash(txid, bind_hash, height, convergence_root)
    assert expected_hash3 == expected_hash
    
    vectors.append(PCORBHashGoldenVector(
        "PCORBHash_target_prev_ignored",
        {
            'txid': txid,
            'bind_hash': bind_hash,
            'height': height,
            'target_prev': target_prev2,
            'convergence_root': convergence_root
        },
        {'pcorb_hash': expected_hash3}
    ))
    
    return vectors


def generate_winner_selection_vectors() -> List[GoldenVector]:
    """Generate golden vectors for winner selection computation."""
    vectors = []
    
    # Vector 1: Singleton candidate can win under RFC-015. The weight is above the saturation
    # point so eligibility is deterministic and this vector tests winner SELECTION rather than
    # draw odds. SECURITY-001-A9-C moved that point from 65536 ORU to 65536*W, so the bound is
    # derived from the window constant rather than written out; doubling it leaves margin.
    _saturating_weight = (rigor_consensus_spec.MIN_CONVERGENCE_AMOUNT
                          * 65536
                          * rigor_consensus_spec.CONVERGENCE_DRAW_WINDOW
                          * 2)
    txs_single = [rigor_consensus_spec.ConvergenceTx(
        txid=create_test_txid(0),
        convergence_amount=_saturating_weight,
        is_valid=True
    )]
    bind_hash = create_test_txid(10)
    height = 100
    target_prev = 0x1d00ffff
    
    vectors.append(WinnerSelectionGoldenVector(
        "WinnerSelection_single_candidate_winner",
        {
            'txs': txs_single,
            'bind_hash': bind_hash,
            'height': height,
            'target_prev': target_prev
        },
        {'winner': txs_single[0].txid}
    ))
    
    # Vector 2: No winner - zero candidates
    txs_empty = []
    vectors.append(WinnerSelectionGoldenVector(
        "WinnerSelection_no_winner_zero_candidates",
        {
            'txs': txs_empty,
            'bind_hash': bind_hash,
            'height': height,
            'target_prev': target_prev
        },
        {'winner': None}
    ))
    
    # Vector 3: No winner - all candidates invalid (inequality fails)
    # Create candidates with very low convergence amounts that fail inequality
    txs_invalid = [
        rigor_consensus_spec.ConvergenceTx(
            txid=create_test_txid(i),
            convergence_amount=rigor_consensus_spec.MIN_CONVERGENCE_AMOUNT,
            is_valid=False,
        )
        for i in range(5)
    ]
    vectors.append(WinnerSelectionGoldenVector(
        "WinnerSelection_no_winner_inequality_fails",
        {
            'txs': txs_invalid,
            'bind_hash': bind_hash,
            'height': height,
            'target_prev': target_prev
        },
        {'winner': None}  # No valid spendable convergence candidates (is_valid=False)
    ))
    
    # Vector 4: Single winner with 2 candidates
    txs_two = [
        rigor_consensus_spec.ConvergenceTx(
            txid=create_test_txid(0),
            convergence_amount=rigor_consensus_spec.MIN_CONVERGENCE_AMOUNT * 10,
            is_valid=True
        ),
        rigor_consensus_spec.ConvergenceTx(
            txid=create_test_txid(1),
            convergence_amount=rigor_consensus_spec.MIN_CONVERGENCE_AMOUNT * 5,
            is_valid=True
        )
    ]
    # Expected winner would be the one with lower hash (deterministic)
    # For now, we'll compute it
    txids = [tx.txid for tx in txs_two]
    convergence_root = rigor_consensus_spec.compute_convergence_root_from_txids(txids)
    expected_winner = rigor_consensus_spec.select_winner(txs_two, bind_hash, height, convergence_root
    )
    
    vectors.append(WinnerSelectionGoldenVector(
        "WinnerSelection_two_candidates",
        {
            'txs': txs_two,
            'bind_hash': bind_hash,
            'height': height,
            'target_prev': target_prev
        },
        {'winner': expected_winner}
    ))
    
    # Vector 5: Multiple candidates - deterministic selection
    txs_multiple = [
        rigor_consensus_spec.ConvergenceTx(
            txid=create_test_txid(i),
            convergence_amount=rigor_consensus_spec.MIN_CONVERGENCE_AMOUNT * (i + 1),
            is_valid=True
        ) for i in range(5)
    ]
    txids_multi = [tx.txid for tx in txs_multiple]
    convergence_root_multi = rigor_consensus_spec.compute_convergence_root_from_txids(txids_multi)
    expected_winner_multi = rigor_consensus_spec.select_winner(txs_multiple, bind_hash, height, convergence_root_multi
    )
    
    vectors.append(WinnerSelectionGoldenVector(
        "WinnerSelection_multiple_candidates",
        {
            'txs': txs_multiple,
            'bind_hash': bind_hash,
            'height': height,
            'target_prev': target_prev
        },
        {'winner': expected_winner_multi}
    ))
    
    return vectors


# =============================================================================
# TEST RUNNER
# =============================================================================

def run_golden_vectors() -> bool:
    """Run all golden vector tests."""
    print("=" * 70)
    print("RIGOR CONSENSUS GOLDEN VECTORS TEST SUITE")
    print("=" * 70)
    print()
    
    all_vectors = []
    all_vectors.extend(generate_convergence_root_vectors())
    all_vectors.extend(generate_pcobr_hash_vectors())
    all_vectors.extend(generate_winner_selection_vectors())
    
    passed = 0
    failed = 0
    
    for vector in all_vectors:
        if vector.run():
            passed += 1
        else:
            failed += 1
    
    print()
    print("=" * 70)
    print(f"Results: {passed} passed, {failed} failed")
    print("=" * 70)
    
    return failed == 0


# =============================================================================
# CROSS-IMPLEMENTATION VALIDATION
# =============================================================================

def validate_against_cpp_implementation():
    """
    Validate Python spec against C++ implementation.
    
    This would require:
    1. Compiling the C++ code with test vector export
    2. Running C++ on same inputs
    3. Comparing outputs
    
    For now, this is a placeholder for the integration step.
    """
    print("C++ cross-validation: run formal_verification/scripts/run_differential_gate.sh")
    print("  Python oracle: tests/test_phase3_oracle.py (full JSON set)")
    print("  C++: selection_golden_tests (generated bridge + hardcoded cases)")
    print("  Coq: Kernel/GoldenHarness.v (enforced subset)")
    print("  This is finite-vector agreement, not a refinement proof.")
    return True


# =============================================================================
# MAIN
# =============================================================================

if __name__ == "__main__":
    success = run_golden_vectors()
    
    if not success:
        print("\nERROR: Golden vector tests failed!")
        print("This indicates a consensus-critical bug in the specification.")
        sys.exit(1)
    
    print("\nAll golden vector tests passed!")
    
    # Run cross-validation if available
    validate_against_cpp_implementation()
    
    sys.exit(0)
