#!/usr/bin/env python3
"""
Byte-level parity tests against ``core/src/test/selection_golden_tests.cpp``.

SECURITY-001-A8: ``run_selection`` no longer takes ``target_prev``; the σ seed is the
parent block hash alone. Expected σ columns match the regenerated C++ golden tests.
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "spec"))

import rigor_consensus_spec as spec


def test_empty_set():
    r = spec.run_selection([], [], 1, bytes(32), 1)
    assert r.convergence_root == bytes(32)
    assert r.sigma_hashes == []
    assert r.eligible == []
    assert r.winner_index == -1


def test_sorting_order():
    _run_case(
        txid_hex=['ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff', '0000000000000000000000000000000000000000000000000000000000000000'],
        fees=[1, 1],
        height=10,
        prev_hex="1111111111111111111111111111111111111111111111111111111111111111",
        target_hex="ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        expected_c_hex="4fa4a869878bd4837e6192377293a0cc14f192b88711635ca3df0447dc1bea76",
        expected_sigma_hex=['9b8fec9a657ea2e6d27f3eead98c02cca2f8c941970ed60b3238d08f4dbd75b2', '6e0f00f65387c23518af80e3b215f895b552d33f3ed90c653a083307511e0cc1'],
        expected_eligible=[True, True],
        expected_winner=0,
    )


def test_single_tx_winner():
    _run_case(
        txid_hex=['aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'],
        fees=[1],
        height=1,
        prev_hex="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
        target_hex="ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        expected_c_hex=None,
        expected_sigma_hex=['30f0b752feab4e693dfabc7ec970f5fb7f2188585f14f4076a1cea0393406a1c'],
        expected_eligible=[True],
        expected_winner=0,
    )


def test_no_winner():
    _run_case(
        txid_hex=['aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb'],
        fees=[1, 1],
        height=5,
        prev_hex="cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
        target_hex="0000000000000000000000000000000000000000000000000000000000000001",
        expected_c_hex="499d0d3b39373fb9b7b0f399b7411f7af213d91c32624280e995ae0f8eb776fb",
        expected_sigma_hex=['c3f728239a89f1876eef97bb9e9238516e8e34f34c8b6408edc63b2bc20aaf88', '3957870e0c706c61cc7ca73350828ae3d56f3f082cdcf10eb6f8fcf7ae45332b'],
        expected_eligible=[False, False],
        expected_winner=-1,
    )


def test_weight_priority():
    _run_case(
        txid_hex=['aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb'],
        fees=[1, 1000],
        height=7,
        prev_hex="dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd",
        target_hex="ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        expected_c_hex="499d0d3b39373fb9b7b0f399b7411f7af213d91c32624280e995ae0f8eb776fb",
        expected_sigma_hex=['4f19b2a6c8b63b1170d8ab52268deda9e5cd171e85549460beabf164b29192c7', 'c8d3d18fc2f5f86630d734ef796bb4d048b846d2ddaa1b37284136ac54919061'],
        expected_eligible=[True, True],
        expected_winner=1,
    )


def test_tie_break():
    _run_case(
        txid_hex=['1111111111111111111111111111111111111111111111111111111111111111', '2222222222222222222222222222222222222222222222222222222222222222'],
        fees=[10, 10],
        height=9,
        prev_hex="eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee",
        target_hex="ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        expected_c_hex="1140b574afee3cb89a4db3dc8037acfa856f5112e68a954e3ca0a908082c98ba",
        expected_sigma_hex=['fb47e7af1a7f9ed73f5e4a5aed5344e9f1bf7fabbef31d2839e2766e8322dbee', '717ef2a6325670be2f6714de2687d9b0399c9062c1b683ea1f0150b99859a852'],
        expected_eligible=[True, True],
        expected_winner=1,
    )


def test_big_int():
    _run_case(
        txid_hex=['ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff', 'eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee'],
        fees=[18446744073709551615, 1],
        height=100,
        prev_hex="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        target_hex="ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        expected_c_hex="490de05febe1b32d530db9f4587a5eae06cbbe2dd0aa8582c0ab7755ebcb9d05",
        expected_sigma_hex=['1d3365ed0ea0262ad8d9ab85682124ee6e7c514723de97a00222e9942ac115b2', 'a7b17ad1704130182b2c71db746dc769a338c6e508248ffa0dc84719a0266193'],
        expected_eligible=[True, True],
        expected_winner=0,
    )


def _run_case(
    txid_hex,
    fees,
    height,
    prev_hex,
    target_hex,
    expected_c_hex,
    expected_sigma_hex,
    expected_eligible,
    expected_winner,
):
    txids = [spec.parse_hex256_sethex(h) for h in txid_hex]
    prev = spec.parse_hex256_sethex(prev_hex)
    t_int = spec.res_probability_target_from_bytes(spec.parse_hex256_sethex(target_hex))
    r = spec.run_selection(txids, fees, height, prev, t_int)

    if expected_c_hex is None:
        assert r.convergence_root == bytes(32)
    else:
        assert r.convergence_root == spec.hash_hex_to_uint256_bytes(expected_c_hex)

    assert len(r.sigma_hashes) == len(expected_sigma_hex)
    for i, sh in enumerate(expected_sigma_hex):
        assert r.sigma_hashes[i] == spec.hash_hex_to_uint256_bytes(sh)

    assert r.eligible == expected_eligible
    assert r.winner_index == expected_winner


if __name__ == "__main__":
    test_empty_set()
    test_sorting_order()
    test_single_tx_winner()
    test_no_winner()
    test_weight_priority()
    test_tie_break()
    test_big_int()
    print("All C++ selection golden tests passed (Python spec).")
