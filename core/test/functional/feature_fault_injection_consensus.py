#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Layer-A consensus fault harness: FAULT-003 only (``submitblock`` / convergence commitment / convergence).

See ``fault_injection_core.py`` module docstring for ORU Ed25519 vs witness-v0 ECDSA
compatibility routing and Layer-A vs Layer-B split.

This module intentionally contains **no** MiniWallet, mempool-order tests, or
``generatetodescriptor`` orchestration — those live in ``feature_fault_injection.py``.
"""

from __future__ import annotations

import os

import test_framework.fault_injection_expect as exp
import test_framework.fault_injection_harness as fh
from fault_injection_core import FaultInjectionShared, SYNC_TIMEOUT, assemble_attack_block
from res_security.cvge_helpers import (  # noqa: I252
    REJECT_BAD_CONVERGENCE_CUMULATIVE,
    REJECT_BAD_CONVERGENCE_MISSING,
    conv_corrupt_cumulative,
    submit_block_hex,
)
from test_framework.opus_pow import solve_block_opus
from test_framework.test_framework import BitcoinTestFramework


def run_fault_003_invalid_mint(test, na, nb, nc):
    """FAULT-003: invalid convergence / CVGE coinbase via ``submitblock`` (Layer-A)."""
    test.log.info("FAULT-003: invalid mint / convergence block injections (submitblock)")
    test._setup_rigor_attack_chain(na)
    test.sync_blocks([na, nb, nc], timeout=SYNC_TIMEOUT)
    fh.drain(na, nb, nc)
    base = test._fault_dir()

    # CVGE cumulative corruption is checked before ConvergenceDrawCandidates root validation inside
    # VerifyCoinbaseConvergenceAndResLayout(); therefore submitblock is expected to fail
    # with bad-convergence-cumulative.
    #
    # CVGE ConvergenceDrawCandidates-root tamper: ConnectBlock enforces winner vs body RES mint coupling
    # (validation.cpp bad-rigor-res-winner-mismatch) before CVGE committed vs recomputed
    # ConvergenceDrawCandidates root comparison (bad-convergence-draw-candidate-root).
    cases = [
        ("invalid_conv_cumulative_commitment", REJECT_BAD_CONVERGENCE_CUMULATIVE, lambda tpl, ra, rb: conv_corrupt_cumulative(tpl["default_convergence_commitment"]), {}),
        ("invalid_sigma_conv_missing", REJECT_BAD_CONVERGENCE_MISSING, lambda _tpl, _ra, _rb: bytes.fromhex("6a0100"), {}),
    ]

    mining_addr = na.get_deterministic_witness_mining_address()

    for case_id, expect_reject, mut_fn, extra_kw in cases:
        tpl, res_a, res_b = test._fresh_convergence_pair_and_template(na)
        snap_before = fh.consensus_fingerprint(na)
        bad_conv = mut_fn(tpl, res_a, res_b)
        blk = assemble_attack_block(na, tpl, res_a, res_b, cvge_script_pubkey=bad_conv, **extra_kw)
        solve_block_opus(blk)
        test.disconnect_nodes(1, 0)
        test.disconnect_nodes(2, 0)
        test.wait_until(
            lambda: len(nb.getpeerinfo()) == 0 and len(nc.getpeerinfo()) == 0,
            timeout=30,
        )
        reasons = []
        for lab, n in (("A", na), ("B", nb), ("C", nc)):
            reasons.append((lab, submit_block_hex(n, blk)))
        for lab, r in reasons:
            if r != expect_reject:
                raise AssertionError(
                    f"{case_id} node {lab} reject mismatch: got {r!r}, expected {expect_reject!r}"
                )
        test.connect_nodes(1, 0)
        test.connect_nodes(2, 0)
        test.sync_blocks([na, nb, nc], timeout=SYNC_TIMEOUT)
        fh.drain(na, nb, nc)
        fh.write_fault_artifact(
            base,
            "FAULT-003",
            case_id,
            {"expected": expect_reject, "per_node": dict(reasons)},
        )
        test._trace_row("FAULT-003", case_id, "reject_reason", expect_reject)
        snap_after = fh.consensus_fingerprint(na)
        if snap_before["state_hash"] != snap_after["state_hash"]:
            raise AssertionError(f"{case_id} state must not mutate on reject")
        test.generatetoaddress(na, 1, mining_addr)
        test.sync_blocks([na, nb, nc], timeout=SYNC_TIMEOUT)

    tpl_d, res_da, _res_db = test._fresh_convergence_pair_and_template(na)
    snap_dup_b = fh.consensus_fingerprint(na)
    blk_dup = assemble_attack_block(na, tpl_d, res_da, res_da)
    solve_block_opus(blk_dup)
    test.disconnect_nodes(1, 0)
    test.disconnect_nodes(2, 0)
    test.wait_until(
        lambda: len(nb.getpeerinfo()) == 0 and len(nc.getpeerinfo()) == 0,
        timeout=30,
    )
    dup_submit = [(lab, submit_block_hex(n, blk_dup)) for lab, n in (("A", na), ("B", nb), ("C", nc))]
    for lab, r in dup_submit:
        if r != exp.SUBMITBLOCK_BAD_TXNS_INPUTS_MISSINGORSPENT:
            raise AssertionError(
                f"duplicate_convergence_in_block node {lab} got {r!r}, expected {exp.SUBMITBLOCK_BAD_TXNS_INPUTS_MISSINGORSPENT!r}"
            )
    test.connect_nodes(1, 0)
    test.connect_nodes(2, 0)
    test.sync_blocks([na, nb, nc], timeout=SYNC_TIMEOUT)
    fh.drain(na, nb, nc)
    fh.write_fault_artifact(
        base,
        "FAULT-003",
        "duplicate_convergence_tx_in_block",
        {"expected": exp.SUBMITBLOCK_BAD_TXNS_INPUTS_MISSINGORSPENT, "per_node": dict(dup_submit)},
    )
    test._trace_row("FAULT-003", "duplicate_convergence_tx_in_block", "reject_reason", exp.SUBMITBLOCK_BAD_TXNS_INPUTS_MISSINGORSPENT)
    if snap_dup_b["state_hash"] != fh.consensus_fingerprint(na)["state_hash"]:
        raise AssertionError("FAULT-003 duplicate-convergence submit must not advance tip")
    test.generatetoaddress(na, 1, mining_addr)
    test.sync_blocks([na, nb, nc], timeout=SYNC_TIMEOUT)

    test._assert_triplet(na, nb, nc, "FAULT-003 final")
    return True


class FaultConsensusInjectionTest(FaultInjectionShared, BitcoinTestFramework):
    """Standalone Layer-A runner: FAULT-003 only."""

    def set_test_params(self):
        FaultInjectionShared.set_test_params(self)

    def run_test(self):
        os.makedirs(self._fault_dir(), exist_ok=True)
        trace = self._trace_path()
        if os.path.isfile(trace):
            os.remove(trace)

        na, nb, nc = self.nodes
        run_fault_003_invalid_mint(self, na, nb, nc)

        fh.finalize_sorted_trace(
            trace,
            [
                "# RIGOR fault injection trace - consensus layer (FAULT-003 only)",
                "# columns: fault_id, case_id, field, value",
            ],
        )
        fh.write_triplet_snapshot(
            os.path.join(self._fault_dir(), "FAULT-SUITE__triplet_replay_fingerprints.json"),
            {
                "fingerprints": fh.triplet_replay_fingerprints(self.nodes, ("A", "B", "C")),
                "identical_triplet": True,
                "layer": "consensus_only",
            },
        )
