#!/usr/bin/env python3
"""
Rigor Golden Vector Test Runner

This script executes golden vector tests and verifies bit-exact results.

Usage:
    python3 test_runner.py --vectors rigor_golden_vectors.json
    python3 test_runner.py --update  # Update golden vectors with computed values
"""

import json
import sys
import argparse
from pathlib import Path
from typing import Dict, Any, List, Tuple

# Add spec to path
sys.path.insert(0, str(Path(__file__).parent / "spec"))

from rigor_consensus_spec import (
    compute_convergence_root_from_txids,
    compute_pcobr_hash,
    check_inequality,
    select_convergence_winner,
    validate_orb_proof,
    determine_res_mode,
    parse_hex256_sethex,
    hash_hex_to_uint256_bytes,
    RES_PROBABILITY_TARGET_INT,
    sha256d,
    TXID_SIZE,
)


class GoldenVectorTestRunner:
    """Test runner for golden vector validation."""
    
    def __init__(self, vectors_file: str):
        self.vectors_file = vectors_file
        self.vectors = self._load_vectors()
        self.results = []
    
    def _load_vectors(self) -> Dict[str, Any]:
        """Load golden vectors from JSON file."""
        with open(self.vectors_file, 'r') as f:
            return json.load(f)
    
    def _hex_to_bytes(self, hex_str: str) -> bytes:
        """Convert hex string to bytes."""
        return bytes.fromhex(hex_str)
    
    def _bytes_to_hex(self, data: bytes) -> str:
        """Convert bytes to hex string."""
        return data.hex()
    
    def run_test(self, vector: Dict[str, Any]) -> Tuple[bool, str, Dict[str, Any]]:
        """Run a single golden vector test."""
        test_id = vector['id']
        test_name = vector['name']
        module = vector['module']
        input_data = vector['input']
        expected = vector['expected']
        
        try:
            if module == "ConvergenceRoot":
                return self._test_convergence_root(test_id, test_name, input_data, expected)
            elif module == "PCORBHash":
                return self._test_pcobr_hash(test_id, test_name, input_data, expected)
            elif module == "Inequality":
                return self._test_inequality(test_id, test_name, input_data, expected)
            elif module == "WinnerSelection":
                return self._test_winner_selection(test_id, test_name, input_data, expected)
            elif module == "ORBProof":
                return self._test_orb_proof(test_id, test_name, input_data, expected)
            elif module == "DualSemantics":
                return self._test_dual_semantics(test_id, test_name, input_data, expected)
            else:
                return False, f"Unknown module: {module}", {}
        except Exception as e:
            return False, f"Exception: {str(e)}", {}
    
    def _test_convergence_root(self, test_id: str, test_name: str, input_data: Dict, expected: Dict) -> Tuple[bool, str, Dict]:
        """Test ConvergenceRoot computation."""
        txids = [self._hex_to_bytes(txid) for txid in input_data['txids']]
        
        # Compute actual result
        actual_root = compute_convergence_root_from_txids(txids)
        actual_root_hex = self._bytes_to_hex(actual_root)
        
        # Verify
        expected_root = expected['Convergeroot']
        if actual_root_hex != expected_root:
            return False, f"ConvergenceRoot mismatch: expected {expected_root}, got {actual_root_hex}", {
                'actual_Convergeroot': actual_root_hex
            }
        
        # Check sorted_txids if provided
        if 'sorted_txids' in expected:
            sorted_txids = sorted(txids)
            expected_sorted = [self._hex_to_bytes(txid) for txid in expected['sorted_txids']]
            if sorted_txids != expected_sorted:
                return False, "Sorted txids mismatch", {}
        
        return True, "PASS", {}
    
    def _test_pcobr_hash(self, test_id: str, test_name: str, input_data: Dict, expected: Dict) -> Tuple[bool, str, Dict]:
        """Test PCORB hash computation."""
        if 'hash' in expected:
            # Full hash test
            txid = self._hex_to_bytes(input_data['txid'])
            # A-8: bind_hash field is B_{h-1} verbatim (also accepted as prev_block_hash).
            prev = self._hex_to_bytes(input_data.get('prev_block_hash', input_data['bind_hash']))
            height = input_data['height']
            convergence_root = self._hex_to_bytes(input_data['Converge_root'])
            
            actual_hash = compute_pcobr_hash(txid, prev, height, convergence_root)
            actual_hash_hex = self._bytes_to_hex(actual_hash)
            
            expected_hash = expected['hash']
            if actual_hash_hex != expected_hash:
                return False, f"Hash mismatch: expected {expected_hash}, got {actual_hash_hex}", {
                    'actual_hash': actual_hash_hex
                }
        elif 'serialized_height' in expected:
            # Endianness test
            height = input_data['height']
            target_prev = input_data['target_prev']
            
            actual_height = height.to_bytes(8, 'little').hex()
            actual_target = target_prev.to_bytes(4, 'little').hex()
            
            expected_height = expected['serialized_height']
            expected_target = expected['serialized_target']
            
            if actual_height != expected_height or actual_target != expected_target:
                return False, f"Endianness mismatch: height={actual_height} (exp {expected_height}), target={actual_target} (exp {expected_target})", {
                    'actual_serialized_height': actual_height,
                    'actual_serialized_target': actual_target
                }
        
        return True, "PASS", {}
    
    def _test_inequality(self, test_id: str, test_name: str, input_data: Dict, expected: Dict) -> Tuple[bool, str, Dict]:
        """Test inequality check (RFC-014: ``H < T·f`` with explicit integer ``T``)."""
        h = hash_hex_to_uint256_bytes(input_data['h'])
        T = input_data.get('T_int', input_data.get('T', RES_PROBABILITY_TARGET_INT))
        weight = input_data.get('f', input_data.get('W', input_data.get('convergence_amount')))

        actual_result = check_inequality(h, T, weight)
        expected_result = expected['result']
        
        if actual_result != expected_result:
            return False, f"Inequality mismatch: expected {expected_result}, got {actual_result}", {
                'actual_result': actual_result
            }
        
        return True, "PASS", {}
    
    def _test_winner_selection(self, test_id: str, test_name: str, input_data: Dict, expected: Dict) -> Tuple[bool, str, Dict]:
        """Test winner selection."""
        candidates = input_data['candidates']
        cr_hex = input_data['Converge_root']
        convergence_root = bytes(TXID_SIZE) if cr_hex == '0' * 64 else hash_hex_to_uint256_bytes(cr_hex)
        
        # Convert candidates to format expected by select_convergence_winner
        candidate_list = []
        T_int = input_data.get('res_probability_target_int', RES_PROBABILITY_TARGET_INT)
        for cand in candidates:
            prev = cand.get('prev_block_hash', cand['bind_hash'])
            candidate_list.append((
                parse_hex256_sethex(cand['txid']),
                parse_hex256_sethex(prev),
                cand['height'],
                cand['convergence_amount'],
            ))

        actual_winner = select_convergence_winner(
            candidate_list, convergence_root, T_int
        )
        expected_winner = expected['winner']
        if expected_winner is None:
            if actual_winner is not None:
                return False, f"Winner mismatch: expected None, got {actual_winner.hex()}", {}
        elif actual_winner is None or actual_winner != parse_hex256_sethex(expected_winner):
            got = None if actual_winner is None else actual_winner.hex()
            return False, f"Winner mismatch: expected {expected_winner}, got {got}", {
                'actual_winner': got
            }

        if 'sigma' in expected:
            for i, cand in enumerate(candidates):
                exp_sig = expected['sigma'][i]
                prev = cand.get('prev_block_hash', cand['bind_hash'])
                got_sig = compute_pcobr_hash(
                    parse_hex256_sethex(cand['txid']),
                    parse_hex256_sethex(prev),
                    cand['height'],
                    convergence_root,
                ).hex()
                if got_sig != exp_sig:
                    return False, f"sigma[{i}] mismatch: expected {exp_sig}, got {got_sig}", {}

        return True, "PASS", {}
    
    def _test_orb_proof(self, test_id: str, test_name: str, input_data: Dict, expected: Dict) -> Tuple[bool, str, Dict]:
        """Test ORB proof validation."""
        # Handle ORB-003 which has different input format
        if 'proof_root' in input_data and 'block_root' in input_data:
            # This is a root mismatch test
            proof_root = self._hex_to_bytes(input_data['proof_root'])
            block_root = self._hex_to_bytes(input_data['block_root'])
            
            # For this test, we just check that different roots would fail
            if proof_root == block_root:
                return False, "Test expects mismatched roots but they are the same", {}
            
            # Simulate validation with mismatched roots
            convergence_txid = self._hex_to_bytes(input_data.get('Converge_txid', 'aa' + '00' * 31))
            in_F = input_data.get('in_F', True)
            inequality = input_data.get('inequality', True)
            
            # Mismatched root should fail - pass block_root as expected
            actual_valid, actual_error = validate_orb_proof(
                convergence_txid, proof_root, in_F, inequality, block_root
            )
            
            expected_valid = expected['valid']
            if actual_valid != expected_valid:
                return False, f"Validity mismatch: expected {expected_valid}, got {actual_valid}", {
                    'actual_valid': actual_valid
                }
            
            return True, "PASS", {}
        
        # Standard ORB proof test
        convergence_txid = self._hex_to_bytes(input_data['Converge_txid'])
        convergence_root = self._hex_to_bytes(input_data['Converge_root'])
        in_F = input_data['in_F']
        inequality = input_data['inequality']
        
        actual_valid, actual_error = validate_orb_proof(
            convergence_txid, convergence_root, in_F, inequality
        )
        
        expected_valid = expected['valid']
        
        if actual_valid != expected_valid:
            return False, f"Validity mismatch: expected {expected_valid}, got {actual_valid} (error: {actual_error})", {
                'actual_valid': actual_valid,
                'actual_error': actual_error
            }
        
        if 'error' in expected and actual_error != expected['error']:
            return False, f"Error mismatch: expected {expected['error']}, got {actual_error}", {
                'actual_error': actual_error
            }
        
        return True, "PASS", {}
    
    def _test_dual_semantics(self, test_id: str, test_name: str, input_data: Dict, expected: Dict) -> Tuple[bool, str, Dict]:
        """Test dual semantics (mint/claim mode)."""
        input_assets = input_data['input_asset']
        has_proof = input_data.get('has_proof', False)
        
        actual_mode, actual_counted, actual_error = determine_res_mode(input_assets, has_proof)
        
        expected_mode = expected.get('mode')
        expected_counted = expected.get('counted')
        expected_valid = expected.get('valid', True)
        expected_error = expected.get('error')
        
        if not expected_valid:
            if actual_error != expected_error:
                return False, f"Error mismatch: expected {expected_error}, got {actual_error}", {
                    'actual_error': actual_error
                }
            return True, "PASS", {}
        
        if actual_mode != expected_mode or actual_counted != expected_counted:
            return False, f"Mode/counted mismatch: expected ({expected_mode}, {expected_counted}), got ({actual_mode}, {actual_counted})", {
                'actual_mode': actual_mode,
                'actual_counted': actual_counted
            }
        
        return True, "PASS", {}
    
    def run_all(self) -> Dict[str, Any]:
        """Run all golden vector tests."""
        passed = 0
        failed = 0
        results = []
        
        for vector in self.vectors['vectors']:
            test_id = vector['id']
            test_name = vector['name']
            
            success, message, computed = self.run_test(vector)
            
            result = {
                'id': test_id,
                'name': test_name,
                'status': 'PASS' if success else 'FAIL',
                'message': message,
                'computed': computed
            }
            results.append(result)
            
            if success:
                passed += 1
            else:
                failed += 1
                print(f"FAIL: {test_id} {test_name}")
                print(f"  {message}")
        
        return {
            'total': len(self.vectors['vectors']),
            'passed': passed,
            'failed': failed,
            'results': results
        }
    
    def update_vectors(self) -> None:
        """Update golden vectors with computed values."""
        updated_vectors = self.vectors.copy()
        
        for vector in updated_vectors['vectors']:
            test_id = vector['id']
            module = vector['module']
            input_data = vector['input']
            
            try:
                if module == "ConvergenceRoot":
                    txids = [self._hex_to_bytes(txid) for txid in input_data['txids']]
                    actual_root = compute_convergence_root_from_txids(txids)
                    vector['expected']['Convergeroot'] = self._bytes_to_hex(actual_root)
                    
                    # Also update sorted_txids
                    sorted_txids = sorted(txids)
                    vector['expected']['sorted_txids'] = [self._bytes_to_hex(txid) for txid in sorted_txids]
                    
                elif module == "PCORBHash" and 'hash' in vector['expected']:
                    txid = self._hex_to_bytes(input_data['txid'])
                    prev = self._hex_to_bytes(input_data.get('prev_block_hash', input_data['bind_hash']))
                    height = input_data['height']
                    convergence_root = self._hex_to_bytes(input_data['Converge_root'])
                    
                    actual_hash = compute_pcobr_hash(txid, prev, height, convergence_root)
                    vector['expected']['hash'] = self._bytes_to_hex(actual_hash)
                
                print(f"Updated {test_id}")
            except Exception as e:
                print(f"Failed to update {test_id}: {e}")
        
        # Write updated vectors
        with open(self.vectors_file, 'w') as f:
            json.dump(updated_vectors, f, indent=2)
        
        print(f"Updated golden vectors written to {self.vectors_file}")


def main():
    parser = argparse.ArgumentParser(description='Rigor Golden Vector Test Runner')
    parser.add_argument('--vectors', default='formal_verification/rigor_golden_vectors.json',
                        help='Path to golden vectors JSON file')
    parser.add_argument('--update', action='store_true',
                        help='Update golden vectors with computed values')
    parser.add_argument('--bit-exact', action='store_true', default=True,
                        help='Require bit-exact matching (default: True)')
    
    args = parser.parse_args()
    
    runner = GoldenVectorTestRunner(args.vectors)
    
    if args.update:
        print("Updating golden vectors with computed values...")
        runner.update_vectors()
        return
    
    print("=" * 70)
    print("RIGOR CONSENSUS GOLDEN VECTOR TESTS")
    print("=" * 70)
    print(f"Vectors file: {args.vectors}")
    print(f"Bit-exact required: {args.bit_exact}")
    print()
    
    results = runner.run_all()
    
    print()
    print("=" * 70)
    print(f"Results: {results['passed']}/{results['total']} passed, {results['failed']} failed")
    print("=" * 70)
    
    if results['failed'] > 0:
        print()
        print("CRITICAL: Golden vector tests failed!")
        print("This indicates a consensus-critical bug.")
        print("DO NOT MERGE until all tests pass.")
        sys.exit(1)
    else:
        print()
        print("SUCCESS: All golden vector tests passed!")
        print("Consensus-critical correctness verified.")
        sys.exit(0)


if __name__ == "__main__":
    main()
