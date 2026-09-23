#!/usr/bin/env python3
"""
Mock C++ Test Harness for Cross-Validation Testing
==================================================

This script simulates C++ test harness output using Python's hashlib.
Used to validate the cross-validation script structure before real C++ integration.

In production, this would be replaced by the actual C++ test_harness executable.
"""

import sys

print("SKIP: lottery mock C++ σ/root harness retired with Batch A")
raise SystemExit(0)

import os
import json

# Import the spec to generate matching outputs
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'spec'))
import rigor_consensus_spec


def create_test_txid(seed: int) -> bytes:
    """Create a deterministic test txid from a seed."""
    return rigor_consensus_spec.sha256d(seed.to_bytes(4, 'little'))


def generate_convergence_root_vectors():
    """Generate ConvergenceRoot test vectors."""
    # Vector 1: Empty list
    txids = []
    root = rigor_consensus_spec.compute_convergence_root_from_txids(txids)
    print(json.dumps({
        'test_name': 'ConvergenceRoot_empty',
        'txids': [],
        'convergence_root': root.hex()
    }))
    
    # Vector 2: Single txid
    txids = [create_test_txid(0)]
    root = rigor_consensus_spec.compute_convergence_root_from_txids(txids)
    print(json.dumps({
        'test_name': 'ConvergenceRoot_single',
        'txids': [t.hex() for t in txids],
        'convergence_root': root.hex()
    }))
    
    # Vector 3: Two txids
    txids = [create_test_txid(0), create_test_txid(1)]
    root = rigor_consensus_spec.compute_convergence_root_from_txids(txids)
    print(json.dumps({
        'test_name': 'ConvergenceRoot_two',
        'txids': [t.hex() for t in txids],
        'convergence_root': root.hex()
    }))
    
    # Vector 4: Multiple txids
    txids = [create_test_txid(i) for i in range(5)]
    root = rigor_consensus_spec.compute_convergence_root_from_txids(txids)
    print(json.dumps({
        'test_name': 'ConvergenceRoot_multiple',
        'txids': [t.hex() for t in txids],
        'convergence_root': root.hex()
    }))
    
    # Vector 5: Order invariance
    txids = [create_test_txid(i) for i in [3, 1, 4, 0, 2]]
    root = rigor_consensus_spec.compute_convergence_root_from_txids(txids)
    print(json.dumps({
        'test_name': 'ConvergenceRoot_order_invariance',
        'txids': [t.hex() for t in txids],
        'convergence_root': root.hex()
    }))


def generate_pcobr_hash_vectors():
    """Generate PCORB hash test vectors."""
    txid = create_test_txid(0)
    bind_hash = create_test_txid(1)
    convergence_root = create_test_txid(2)
    height = 100
    target_prev = 0x1d00ffff
    
    # Vector 1: Basic case
    h = rigor_consensus_spec.compute_pcobr_hash(
        txid, bind_hash, height, target_prev, convergence_root
    )
    print(json.dumps({
        'test_name': 'PCORBHash_basic',
        'txid': txid.hex(),
        'bind_hash': bind_hash.hex(),
        'height': height,
        'target_prev': target_prev,
        'convergence_root': convergence_root.hex(),
        'pcorb_hash': h.hex()
    }))
    
    # Vector 2: Different height
    height2 = 999999
    h2 = rigor_consensus_spec.compute_pcobr_hash(
        txid, bind_hash, height2, target_prev, convergence_root
    )
    print(json.dumps({
        'test_name': 'PCORBHash_different_height',
        'txid': txid.hex(),
        'bind_hash': bind_hash.hex(),
        'height': height2,
        'target_prev': target_prev,
        'convergence_root': convergence_root.hex(),
        'pcorb_hash': h2.hex()
    }))
    
    # Vector 3: Different target
    target_prev2 = 0x1e012345
    h3 = rigor_consensus_spec.compute_pcobr_hash(
        txid, bind_hash, height, target_prev2, convergence_root
    )
    print(json.dumps({
        'test_name': 'PCORBHash_different_target',
        'txid': txid.hex(),
        'bind_hash': bind_hash.hex(),
        'height': height,
        'target_prev': target_prev2,
        'convergence_root': convergence_root.hex(),
        'pcorb_hash': h3.hex()
    }))


if __name__ == "__main__":
    vectors = []
    
    # Collect ConvergenceRoot vectors
    txids = []
    root = rigor_consensus_spec.compute_convergence_root_from_txids(txids)
    vectors.append({
        'test_name': 'ConvergenceRoot_empty',
        'txids': [],
        'convergence_root': root.hex()
    })
    
    txids = [create_test_txid(0)]
    root = rigor_consensus_spec.compute_convergence_root_from_txids(txids)
    vectors.append({
        'test_name': 'ConvergenceRoot_single',
        'txids': [t.hex() for t in txids],
        'convergence_root': root.hex()
    })
    
    txids = [create_test_txid(0), create_test_txid(1)]
    root = rigor_consensus_spec.compute_convergence_root_from_txids(txids)
    vectors.append({
        'test_name': 'ConvergenceRoot_two',
        'txids': [t.hex() for t in txids],
        'convergence_root': root.hex()
    })
    
    txids = [create_test_txid(i) for i in range(5)]
    root = rigor_consensus_spec.compute_convergence_root_from_txids(txids)
    vectors.append({
        'test_name': 'ConvergenceRoot_multiple',
        'txids': [t.hex() for t in txids],
        'convergence_root': root.hex()
    })
    
    txids = [create_test_txid(i) for i in [3, 1, 4, 0, 2]]
    root = rigor_consensus_spec.compute_convergence_root_from_txids(txids)
    vectors.append({
        'test_name': 'ConvergenceRoot_order_invariance',
        'txids': [t.hex() for t in txids],
        'convergence_root': root.hex()
    })
    
    # Collect PCORB hash vectors
    # SECURITY-001-A8: target_prev is not a sigma input.
    txid = create_test_txid(0)
    prev_block_hash = create_test_txid(1)
    convergence_root = create_test_txid(2)
    height = 100
    
    h = rigor_consensus_spec.compute_pcobr_hash(
        txid, prev_block_hash, height, convergence_root
    )
    vectors.append({
        'test_name': 'PCORBHash_basic',
        'txid': txid.hex(),
        'prev_block_hash': prev_block_hash.hex(),
        'height': height,
        'convergence_root': convergence_root.hex(),
        'pcorb_hash': h.hex()
    })
    
    height2 = 999999
    h2 = rigor_consensus_spec.compute_pcobr_hash(
        txid, prev_block_hash, height2, convergence_root
    )
    vectors.append({
        'test_name': 'PCORBHash_different_height',
        'txid': txid.hex(),
        'prev_block_hash': prev_block_hash.hex(),
        'height': height2,
        'convergence_root': convergence_root.hex(),
        'pcorb_hash': h2.hex()
    })
    
    convergence_root2 = create_test_txid(3)
    h3 = rigor_consensus_spec.compute_pcobr_hash(
        txid, prev_block_hash, height, convergence_root2
    )
    vectors.append({
        'test_name': 'PCORBHash_different_convergence_root',
        'txid': txid.hex(),
        'prev_block_hash': prev_block_hash.hex(),
        'height': height,
        'convergence_root': convergence_root2.hex(),
        'pcorb_hash': h3.hex()
    })
    
    # Output as JSON array
    print(json.dumps(vectors, indent=2))
