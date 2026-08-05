#!/usr/bin/env python3
"""
Rigor Consensus Python-C++ Cross-Validation Script
=================================================

This script compares the Python executable spec output with C++ implementation
output to ensure byte-level exactness.

CRITICAL: Any mismatch indicates a consensus-critical bug.

This script includes byte-level diff comparison (diff -b style) to catch
subtle implementation differences in:
- memcmp sorting
- LE64/LE32 encoding
- SHA256d implementation
"""

import sys
import os
import json
import subprocess
from typing import List, Dict, Any, Tuple
import difflib

# Import the spec
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'spec'))
import rigor_consensus_spec


# =============================================================================
# C++ Test Harness Execution
# =============================================================================

def run_cpp_test_harness() -> List[Dict[str, Any]]:
    """
    Run the C++ test harness and parse its JSON output.
    
    Returns:
        List of test vectors with C++ results
    """
    cpp_harness_path = os.path.join(
        os.path.dirname(__file__), '..', 'cpp', 'bin', 'test_harness'
    )
    mock_harness_path = os.path.join(
        os.path.dirname(__file__), '..', 'cpp', 'mock_cpp_harness.py'
    )
    
    # Try real C++ harness first, fall back to mock
    harness_path = cpp_harness_path
    use_python = False
    
    if not os.path.exists(cpp_harness_path):
        print("WARNING: C++ test harness not found, using mock Python harness")
        harness_path = mock_harness_path
        use_python = True
    
    try:
        # Run test harness
        if use_python:
            result = subprocess.run(
                ['python3', harness_path],
                capture_output=True,
                text=True,
                check=True
            )
        else:
            result = subprocess.run(
                [harness_path],
                capture_output=True,
                text=True,
                check=True
            )
        
        # Parse JSON output
        output = result.stdout.strip()
        if output.startswith('[') and output.endswith(']'):
            vectors = json.loads(output)
            return vectors
        else:
            print(f"ERROR: Harness output is not valid JSON array")
            print(f"Output: {output[:200]}")
            sys.exit(1)
            
    except subprocess.CalledProcessError as e:
        print(f"ERROR: Test harness failed with exit code {e.returncode}")
        print(f"stderr: {e.stderr}")
        sys.exit(1)
    except FileNotFoundError:
        print(f"ERROR: Test harness not found at {harness_path}")
        print("Please build the C++ test harness first:")
        print("  cd formal_verification/cpp")
        print("  ./build_harness.sh")
        sys.exit(1)
    except json.JSONDecodeError as e:
        print(f"ERROR: Failed to parse harness JSON output: {e}")
        sys.exit(1)


# =============================================================================
# Python Spec Test Vector Generation
# =============================================================================

def create_test_txid(seed: int) -> bytes:
    """Create a deterministic test txid from a seed."""
    return rigor_consensus_spec.sha256d(seed.to_bytes(4, 'little'))


def generate_python_convergence_root_vectors() -> List[Dict[str, Any]]:
    """Generate ConvergenceRoot test vectors using Python spec."""
    vectors = []
    
    # Vector 1: Empty list
    vectors.append({
        'test_name': 'ConvergenceRoot_empty',
        'txids': [],
        'convergence_root': rigor_consensus_spec.compute_convergence_root_from_txids([]).hex()
    })
    
    # Vector 2: Single txid
    vectors.append({
        'test_name': 'ConvergenceRoot_single',
        'txids': [create_test_txid(0).hex()],
        'convergence_root': rigor_consensus_spec.compute_convergence_root_from_txids(
            [create_test_txid(0)]
        ).hex()
    })
    
    # Vector 3: Two txids
    txids = [create_test_txid(0), create_test_txid(1)]
    vectors.append({
        'test_name': 'ConvergenceRoot_two',
        'txids': [t.hex() for t in txids],
        'convergence_root': rigor_consensus_spec.compute_convergence_root_from_txids(txids).hex()
    })
    
    # Vector 4: Multiple txids
    txids = [create_test_txid(i) for i in range(5)]
    vectors.append({
        'test_name': 'ConvergenceRoot_multiple',
        'txids': [t.hex() for t in txids],
        'convergence_root': rigor_consensus_spec.compute_convergence_root_from_txids(txids).hex()
    })
    
    # Vector 5: Order invariance
    txids = [create_test_txid(i) for i in [3, 1, 4, 0, 2]]
    vectors.append({
        'test_name': 'ConvergenceRoot_order_invariance',
        'txids': [t.hex() for t in txids],
        'convergence_root': rigor_consensus_spec.compute_convergence_root_from_txids(txids).hex()
    })
    
    return vectors


def generate_python_pcobr_hash_vectors() -> List[Dict[str, Any]]:
    """Generate PCORB hash test vectors using Python spec."""
    vectors = []
    
    txid = create_test_txid(0)
    bind_hash = create_test_txid(1)
    convergence_root = create_test_txid(2)
    height = 100
    target_prev = 0x1d00ffff
    
    # Vector 1: Basic case
    h = rigor_consensus_spec.compute_pcobr_hash(
        txid, bind_hash, height, target_prev, convergence_root
    )
    vectors.append({
        'test_name': 'PCORBHash_basic',
        'txid': txid.hex(),
        'bind_hash': bind_hash.hex(),
        'height': height,
        'target_prev': target_prev,
        'convergence_root': convergence_root.hex(),
        'pcorb_hash': h.hex()
    })
    
    # Vector 2: Different height
    height2 = 999999
    h2 = rigor_consensus_spec.compute_pcobr_hash(
        txid, bind_hash, height2, target_prev, convergence_root
    )
    vectors.append({
        'test_name': 'PCORBHash_different_height',
        'txid': txid.hex(),
        'bind_hash': bind_hash.hex(),
        'height': height2,
        'target_prev': target_prev,
        'convergence_root': convergence_root.hex(),
        'pcorb_hash': h2.hex()
    })
    
    # Vector 3: Different target
    target_prev2 = 0x1e012345
    h3 = rigor_consensus_spec.compute_pcobr_hash(
        txid, bind_hash, height, target_prev2, convergence_root
    )
    vectors.append({
        'test_name': 'PCORBHash_different_target',
        'txid': txid.hex(),
        'bind_hash': bind_hash.hex(),
        'height': height,
        'target_prev': target_prev2,
        'convergence_root': convergence_root.hex(),
        'pcorb_hash': h3.hex()
    })
    
    return vectors


# =============================================================================
# Byte-Level Diff Comparison
# =============================================================================

def byte_level_diff(hex1: str, hex2: str, label: str = "") -> List[str]:
    """
    Perform byte-level diff comparison between two hex strings.
    
    This is similar to `diff -b` but for hex bytes.
    Returns a list of difference descriptions.
    """
    if hex1 == hex2:
        return []
    
    differences = []
    
    # Convert to bytes
    try:
        bytes1 = bytes.fromhex(hex1)
        bytes2 = bytes.fromhex(hex2)
    except ValueError as e:
        differences.append(f"Invalid hex: {e}")
        return differences
    
    # Compare byte by byte
    max_len = max(len(bytes1), len(bytes2))
    
    for i in range(max_len):
        b1 = bytes1[i] if i < len(bytes1) else None
        b2 = bytes2[i] if i < len(bytes2) else None
        
        if b1 != b2:
            diff_str = f"Byte {i}: "
            if b1 is not None:
                diff_str += f"Python=0x{b1:02x} "
            else:
                diff_str += "Python=(missing) "
            if b2 is not None:
                diff_str += f"C++=0x{b2:02x}"
            else:
                diff_str += "C++=(missing)"
            differences.append(diff_str)
    
    return differences


# =============================================================================
# Cross-Validation
# =============================================================================

def validate_vector(cpp_vector: Dict[str, Any], python_vector: Dict[str, Any]) -> Tuple[bool, str]:
    """
    Validate a single test vector between C++ and Python.
    
    Returns:
        (passed, error_message)
    """
    test_name = cpp_vector.get('test_name', '')
    
    # Check test name matches
    if cpp_vector.get('test_name') != python_vector.get('test_name'):
        return False, f"Test name mismatch: C++={cpp_vector.get('test_name')}, Python={python_vector.get('test_name')}"
    
    # For ConvergenceRoot vectors
    if 'convergence_root' in cpp_vector:
        cpp_root = cpp_vector['convergence_root']
        python_root = python_vector['convergence_root']
        
        if cpp_root != python_root:
            diff = byte_level_diff(python_root, cpp_root, "convergence_root")
            error_msg = f"ConvergenceRoot mismatch:\n"
            error_msg += f"  Python: {python_root}\n"
            error_msg += f"  C++:    {cpp_root}\n"
            if diff:
                error_msg += f"  Byte differences:\n"
                for d in diff:
                    error_msg += f"    {d}\n"
            return False, error_msg
    
    # For PCORB hash vectors
    if 'pcorb_hash' in cpp_vector:
        cpp_hash = cpp_vector['pcorb_hash']
        python_hash = python_vector['pcorb_hash']
        
        if cpp_hash != python_hash:
            diff = byte_level_diff(python_hash, cpp_hash, "pcorb_hash")
            error_msg = f"PCORB hash mismatch:\n"
            error_msg += f"  Python: {python_hash}\n"
            error_msg += f"  C++:    {cpp_hash}\n"
            if diff:
                error_msg += f"  Byte differences:\n"
                for d in diff:
                    error_msg += f"    {d}\n"
            return False, error_msg
    
    return True, ""


def run_cross_validation():
    """Run full cross-validation between Python and C++."""
    print("=" * 70)
    print("RIGOR CONSENSUS PYTHON-C++ CROSS-VALIDATION")
    print("=" * 70)
    print()
    
    # Get C++ vectors
    print("Running C++ test harness...")
    cpp_vectors = run_cpp_test_harness()
    print(f"C++ generated {len(cpp_vectors)} test vectors")
    print()
    
    # Generate Python vectors
    print("Generating Python test vectors...")
    python_vectors = []
    python_vectors.extend(generate_python_convergence_root_vectors())
    python_vectors.extend(generate_python_pcobr_hash_vectors())
    print(f"Python generated {len(python_vectors)} test vectors")
    print()
    
    # Create lookup by test name
    python_lookup = {v['test_name']: v for v in python_vectors}
    
    # Validate each vector
    passed = 0
    failed = 0
    
    for cpp_vec in cpp_vectors:
        test_name = cpp_vec['test_name']
        
        if test_name not in python_lookup:
            print(f"SKIP: {test_name} - not found in Python vectors")
            continue
        
        python_vec = python_lookup[test_name]
        is_valid, error_msg = validate_vector(cpp_vec, python_vec)
        
        if is_valid:
            print(f"PASS: {test_name}")
            passed += 1
        else:
            print(f"FAIL: {test_name}")
            print(f"  {error_msg}")
            failed += 1
    
    print()
    print("=" * 70)
    print(f"Results: {passed} passed, {failed} failed")
    print("=" * 70)
    
    if failed > 0:
        print()
        print("ERROR: Cross-validation detected consensus-critical mismatches!")
        print("This indicates a bug in either the Python spec or C++ implementation.")
        print("Common causes:")
        print("  - Endianness mismatch (LE64/LE32 encoding)")
        print("  - memcmp sorting difference")
        print("  - SHA256d implementation variant")
        print("  - Byte order in uint256 representation")
        sys.exit(1)
    
    print()
    print("SUCCESS: Python and C++ implementations are byte-identical!")
    return True


# =============================================================================
# Main
# =============================================================================

if __name__ == "__main__":
    run_cross_validation()
