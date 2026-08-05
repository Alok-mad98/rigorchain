#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Fault injection harness — Layer-B full suite (FAULT-001 … FAULT-008).

Consensus-only path (FAULT-003 in isolation, no MiniWallet / mempool-compat noise):
    python3 test/functional/feature_fault_consensus_only.py --configfile test/config.ini

Layering
--------
* **Layer-A** — ``submitblock`` / convergence commitment / convergence invalid blocks: ``feature_fault_consensus_only.py``
  (implementation: ``feature_fault_injection_consensus.py`` + ``fault_injection_core.py``).
* **Layer-B (this file)** — FAULT-001/002 (mempool / MiniWallet witness hostility), FAULT-004…008
  (P2P / relay / SIGKILL / mempool order). Shares node setup via ``FaultInjectionShared``.

ORU native vs Bitcoin-compatible signing (harness documentation only; see ``interpreter.cpp``)
-----------------------------------------------------------------------------------------------
* ORU native witness spends use Ed25519 verification for the ``0x00 || ed25519_pk`` pubkey shape.
* Standard P2WPKH with compressed secp pubkeys uses witness-v0 **ECDSA** (compatibility path).
* ``MiniWallet`` is a **test helper** on the compatibility path only — not consensus-critical ORU identity.

STRICT RULES (Constitution)
---------------------------
* DO NOT modify consensus rules.
* DO NOT auto-correct failures.
* DO NOT suppress crashes or divergence.
* DO NOT optimize behavior.
* ONLY inject faults and capture behavior.

TASK coverage map (FAULT-001 … FAULT-008)
-----------------------------------------
* **FAULT-001** — malformed / policy-bad ORU-domain txs via ``testmempoolaccept`` /
  ``sendrawtransaction`` (truncated, garbage hex, oversize, dup inputs, max-unspendable,
  empty vouts, invalid ``createrawtransaction`` asset RPC).
* **FAULT-002** — malformed P2WPKH witness (truncated wire, oversize stack element,
  empty stack, bad ECDSA bytes, **extra witness stack item** as a stand-in for
  witness-shape hostility; full PCORB ``ConvergeProof`` / ``SelectionProof`` byte
  cases live in wallet/RPC mint tests — not duplicated here to avoid wallet scope creep).
* **FAULT-003** — invalid convergence / RIGOR coinbase path via ``submitblock`` on
  all three nodes + **duplicate convergence tx** in the same block body (merkle dup).
  **Also runnable standalone:** ``feature_fault_consensus_only.py``.
* **FAULT-004** — orphan / out-of-order ``submitblock`` on an isolated follower.
* **FAULT-005** — delayed relay, duplicate ``submitblock``, **staggered follower connect**.
* **FAULT-006** — partition, competing private tips, delayed follower reconnect.
* **FAULT-007** — SIGKILL mid heavy I/O + restart; optional ``savemempool`` persistence tick.
* **FAULT-008** — reversed local submission order + **explicit duplicate ``sendrawtransaction``**
  after sync (mempool may differ pre-block; chainstate must match post-block).

Artifacts (under tmpdir ``fault_injection/``)
---------------------------------------------
* ``fault_trace.tsv`` — TAB-separated rows, sorted at end of run (no timestamps).
* ``FAULT-xxx__<case>.json`` — per-subcase outcomes (canonical JSON).
* ``FAULT-SUITE__triplet_replay_fingerprints.json`` — final A/B/C ``state_hash`` + RPL2 digest.

Replay / divergence capture reuses STEP-0202 when a consensus fingerprint
mismatch is detected (non-suppressing: test still fails after capture).
"""

from __future__ import annotations

import os
import time
from copy import deepcopy
from decimal import Decimal

from fault_injection_core import FaultInjectionShared, SYNC_TIMEOUT
from feature_fault_injection_consensus import run_fault_003_invalid_mint

from test_framework.blocktools import COINBASE_MATURITY
from test_framework.messages import (
    COIN,
    COutPoint,
    CTransaction,
    CTxIn,
    CTxOut,
)
from test_framework.script import CScript, OP_RETURN
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal
from test_framework.wallet import MiniWallet

import test_framework.fault_injection_expect as exp
import test_framework.fault_injection_harness as fh


class FaultInjectionTest(FaultInjectionShared, BitcoinTestFramework):
    def set_test_params(self):
        FaultInjectionShared.set_test_params(self)

    def fault_001_malformed_transaction(self, na, nb, nc):
        self.log.info("FAULT-001: malformed transaction injections")
        self._mine_baseline(na, nb, nc, 15)
        wallet = MiniWallet(na)
        wallet.generate(COINBASE_MATURITY + 8)
        self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)
        fh.drain(na, nb, nc)

        st = wallet.create_self_transfer(fee_rate=Decimal("0.00001"))
        good_hex = st["hex"]
        base = self._fault_dir()

        def record(case_id: str, obj):
            fh.write_fault_artifact(base, "FAULT-001", case_id, obj)

        # --- truncated serialization (decode error path) ---
        bad_trunc = good_hex[:-10]
        errs = []
        for lab, n in (("A", na), ("B", nb), ("C", nc)):
            ok, _r, exc = fh.try_rpc(
                lambda _n=n: _n.testmempoolaccept(rawtxs=[bad_trunc], maxfeerate=Decimal("0"))
            )
            assert not ok and exc is not None, (lab, ok)
            errs.append(exc)
        fh.assert_identical_rpc_errors(
            errs, ["A", "B", "C"], "FAULT-001 truncated",
            message_must_contain=exp.MEMPOOL_RPC_DECODE_FRAGMENT,
        )
        record("truncated_tx", {"rpc_error": [fh.rpc_error_tuple(e) for e in errs]})
        self._trace_row("FAULT-001", "truncated_tx", "normalized_message", fh.normalize_decode_error_message(errs[0].error["message"]))

        # --- invalid hex (not minimal hex / wrong length) ---
        bad_garbage = "ff" * 4
        errs2 = []
        for n in (na, nb, nc):
            ok, _r, exc = fh.try_rpc(
                lambda _n=n: _n.testmempoolaccept(rawtxs=[bad_garbage], maxfeerate=Decimal("0"))
            )
            assert not ok and exc is not None
            errs2.append(exc)
        fh.assert_identical_rpc_errors(
            errs2, ["A", "B", "C"], "FAULT-001 garbage",
            message_must_contain=exp.MEMPOOL_RPC_DECODE_FRAGMENT,
        )
        record("invalid_serialization", {"rpc_error": [fh.rpc_error_tuple(e) for e in errs2]})

        # --- oversized (standardness / weight) ---
        # Pad toward ``MAX_STANDARD_TX_WEIGHT`` (400000) with ``_bulk_tx`` and a large ORUM fee
        # so relay policy does not hit ``min relay fee not met (orum)`` first.  Depending on
        # local weight accounting vs witness serialization, the triplet-stable mempool verdict
        # may be ``tx-size`` or a BIP147-style script message on the same malformed witness path.
        st_big = wallet.create_self_transfer(
            fee=Decimal("2"),
            fee_rate=Decimal("0"),
            target_weight=1_200_000,
        )
        big_hex = st_big["hex"]
        wt = int(na.decoderawtransaction(big_hex, True)["weight"])
        fh.assert_identical_mempool_verdict(
            (na, nb, nc),
            ("A", "B", "C"),
            big_hex,
            ctx="FAULT-001 oversized",
            expect_reject_one_of=(
                exp.MEMPOOL_TX_SIZE_NON_STANDARD,
                "non-mandatory-script-verify-flag (Signature must be zero for failed CHECK(MULTI)SIG operation)",
            ),
        )
        record(
            "oversized_tx",
            {
                "decoderawtransaction_weight": wt,
                "verdict": fh.testmempoolaccept_result(na, big_hex, maxfeerate=Decimal("0")),
            },
        )

        # --- duplicate inputs ---
        tx = deepcopy(st["tx"])
        tx.vin.append(deepcopy(tx.vin[0]))
        tx.rehash()
        dup_hex = tx.serialize_with_witness().hex()
        fh.assert_identical_mempool_verdict(
            (na, nb, nc),
            ("A", "B", "C"),
            dup_hex,
            ctx="FAULT-001 dup inputs",
            expect_reject_exact=exp.MEMPOOL_BAD_TXNS_INPUTS_DUPLICATE,
        )
        record("duplicate_inputs", {"verdict": fh.testmempoolaccept_result(na, dup_hex, maxfeerate=Decimal("0"))})

        # --- empty outputs (CheckTransaction → bad-txns-vout-empty) ---
        tx_empty = deepcopy(st["tx"])
        tx_empty.vout = []
        tx_empty.rehash()
        empty_hex = tx_empty.serialize_with_witness().hex()
        fh.assert_identical_mempool_verdict(
            (na, nb, nc),
            ("A", "B", "C"),
            empty_hex,
            ctx="FAULT-001 empty_outputs",
            expect_reject_exact=exp.MEMPOOL_BAD_TXNS_VOUT_EMPTY,
        )
        record("empty_outputs", {"verdict": fh.testmempoolaccept_result(na, empty_hex, maxfeerate=Decimal("0"))})

        # --- invalid asset domain at RPC layer (not a serialized tx; identical JSON-RPC error) ---
        utxo2 = wallet.get_utxo()
        out_addr = na.getrawchangeaddress("bech32")
        rpc_errs = []
        for n in (na, nb, nc):
            ok, _r, exc = fh.try_rpc(
                lambda _n=n: _n.createrawtransaction(
                    [{"txid": utxo2["txid"], "vout": int(utxo2["vout"])}],
                    [{out_addr: {"amount": "0.01", "asset": "ORB"}}],
                )
            )
            assert not ok and exc is not None
            rpc_errs.append(exc)
        fh.assert_identical_rpc_errors(
            rpc_errs,
            ["A", "B", "C"],
            "FAULT-001 invalid_asset_domain",
            message_must_contain="asset",
        )
        record(
            "invalid_asset_domain",
            {"rpc_error": [fh.rpc_error_tuple(e) for e in rpc_errs]},
        )

        # --- max unspendable exceeded (sendrawtransaction policy; identical error tuple) ---
        tx_burn = CTransaction()
        utxo = wallet.get_utxo()
        tx_burn.vin.append(
            CTxIn(COutPoint(int(utxo["txid"], 16), utxo["vout"]), nSequence=0xFFFFFFFF)
        )
        burn_value = int(Decimal(utxo["value"]) * COIN) - 1000
        assert burn_value > 0
        tx_burn.vout = [
            CTxOut(burn_value, CScript([OP_RETURN, b"fault001"])),
        ]
        wallet.sign_tx(tx_burn, utxos_to_spend=[utxo])
        burn_hex = tx_burn.serialize_with_witness().hex()
        burn_errs = []
        for n in (na, nb, nc):
            ok, _r, exc = fh.try_rpc(
                lambda _n=n: _n.sendrawtransaction(burn_hex, 0, 0)
            )
            assert not ok and exc is not None
            burn_errs.append(exc)
        fh.assert_identical_rpc_errors(
            burn_errs,
            ["A", "B", "C"],
            "FAULT-001 max-unspendable",
            message_must_contain=exp.SENDRAW_MAX_UNSPENDABLE_FRAGMENT,
        )
        record("max_unspendable_policy", {"rpc_error": [fh.rpc_error_tuple(e) for e in burn_errs]})

        self._assert_triplet(na, nb, nc, "FAULT-001 post-injection")
        return True

    def fault_002_malformed_witness(self, na, nb, nc):
        self.log.info("FAULT-002: malformed witness injections")
        self._mine_baseline(na, nb, nc, 15)
        wallet = MiniWallet(na)
        wallet.generate(COINBASE_MATURITY + 8)
        self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)
        fh.drain(na, nb, nc)

        st = wallet.create_self_transfer(fee=Decimal("0.5"), fee_rate=Decimal("0"))
        good_wit_hex = st["hex"]
        base = self._fault_dir()
        _script_fail_prefixes = (
            exp.MEMPOOL_SCRIPT_VERIFY_FAILED_PREFIX,
            exp.MEMPOOL_NON_MANDATORY_SCRIPT_PREFIX,
        )

        # Truncated witness serialization (either RPC decode failure or mempool reject)
        full = bytes.fromhex(good_wit_hex)
        bad_wit = full[:-5].hex()
        ok0, r0, exc0 = fh.try_rpc(
            lambda: na.testmempoolaccept(rawtxs=[bad_wit], maxfeerate=Decimal("0"))
        )
        if not ok0:
            errs = []
            for n in (na, nb, nc):
                ok, _r, exc = fh.try_rpc(
                    lambda _n=n: _n.testmempoolaccept(rawtxs=[bad_wit], maxfeerate=Decimal("0"))
                )
                assert not ok and exc is not None
                errs.append(exc)
            fh.assert_identical_rpc_errors(
                errs,
                ["A", "B", "C"],
                "FAULT-002 truncated_witness",
                message_must_contain=exp.MEMPOOL_RPC_DECODE_FRAGMENT,
            )
            fh.write_fault_artifact(
                base, "FAULT-002", "truncated_witness", {"path": "rpc_decode", "rpc_error": [fh.rpc_error_tuple(e) for e in errs]}
            )
        else:
            assert r0 and len(r0) == 1 and r0[0].get("allowed") is False
            fh.assert_identical_mempool_verdict(
                (na, nb, nc),
                ("A", "B", "C"),
                bad_wit,
                ctx="FAULT-002 truncated_witness",
                expect_reject_starts_with_any=_script_fail_prefixes,
            )
            fh.write_fault_artifact(
                base,
                "FAULT-002",
                "truncated_witness",
                {"path": "mempool_reject", "verdict": fh.testmempoolaccept_result(na, bad_wit, maxfeerate=Decimal("0"))},
            )

        # Oversized witness stack element (still parses; policy / script fail)
        tx = deepcopy(st["tx"])
        tx.wit.vtxinwit[0].scriptWitness.stack = [b"\xab" * 5000, b"\xcc" * 10]
        tx.rehash()
        bigw = tx.serialize_with_witness().hex()
        fh.assert_identical_mempool_verdict(
            (na, nb, nc),
            ("A", "B", "C"),
            bigw,
            ctx="FAULT-002 oversized_witness",
            expect_reject_starts_with_any=_script_fail_prefixes,
        )
        fh.write_fault_artifact(
            base,
            "FAULT-002",
            "oversized_witness",
            {"verdict": fh.testmempoolaccept_result(na, bigw, maxfeerate=Decimal("0"))},
        )

        # Invalid P2WPKH stack size (empty witness)
        tx2 = deepcopy(st["tx"])
        tx2.wit = type(tx2.wit)()
        tx2.rehash()
        bad_stack = tx2.serialize_with_witness().hex()
        fh.assert_identical_mempool_verdict(
            (na, nb, nc),
            ("A", "B", "C"),
            bad_stack,
            ctx="FAULT-002 invalid_stack",
            expect_reject_starts_with_any=_script_fail_prefixes,
        )
        fh.write_fault_artifact(
            base,
            "FAULT-002",
            "invalid_stack_size",
            {"verdict": fh.testmempoolaccept_result(na, bad_stack, maxfeerate=Decimal("0"))},
        )

        # Malformed signature bytes (still two stack items; ECDSA parse / verify fail)
        tx3 = deepcopy(st["tx"])
        tx3.wit.vtxinwit[0].scriptWitness.stack = [b"\x00" * 71, tx3.wit.vtxinwit[0].scriptWitness.stack[1]]
        tx3.rehash()
        bad_sig = tx3.serialize_with_witness().hex()
        fh.assert_identical_mempool_verdict(
            (na, nb, nc),
            ("A", "B", "C"),
            bad_sig,
            ctx="FAULT-002 bad_sig",
            expect_reject_starts_with_any=_script_fail_prefixes,
        )
        fh.write_fault_artifact(
            base,
            "FAULT-002",
            "malformed_signature",
            {"verdict": fh.testmempoolaccept_result(na, bad_sig, maxfeerate=Decimal("0"))},
        )

        # Extra witness stack item (P2WPKH expects exactly two pushes; hostile padding).
        tx4 = deepcopy(st["tx"])
        st0 = tx4.wit.vtxinwit[0].scriptWitness.stack
        tx4.wit.vtxinwit[0].scriptWitness.stack = [st0[0], st0[1], b"\xde\xad"]
        tx4.rehash()
        extra_wit = tx4.serialize_with_witness().hex()
        fh.assert_identical_mempool_verdict(
            (na, nb, nc),
            ("A", "B", "C"),
            extra_wit,
            ctx="FAULT-002 extra_witness_stack_item",
            expect_reject_starts_with_any=_script_fail_prefixes,
        )
        fh.write_fault_artifact(
            base,
            "FAULT-002",
            "extra_witness_stack_item",
            {"verdict": fh.testmempoolaccept_result(na, extra_wit, maxfeerate=Decimal("0"))},
        )

        self._assert_triplet(na, nb, nc, "FAULT-002 post-injection")
        return True

    def fault_003_invalid_mint(self, na, nb, nc):
        """FAULT-003 (Layer-A): delegated to ``feature_fault_injection_consensus.run_fault_003_invalid_mint``."""
        return run_fault_003_invalid_mint(self, na, nb, nc)

    def fault_004_orphan_storm(self, na, nb, nc):
        self.log.info("FAULT-004: orphan / out-of-order block delivery")
        self._mine_baseline(na, nb, nc, 10)
        self.disconnect_nodes(1, 0)
        self.disconnect_nodes(2, 0)
        self.wait_until(lambda: len(nb.getpeerinfo()) == 0 and len(nc.getpeerinfo()) == 0, timeout=30)

        addr = na.get_deterministic_witness_mining_address()
        tip0 = na.getbestblockhash()
        self.generatetoaddress(na, 5, addr, sync_fun=lambda: None)
        hashes = []
        h = na.getblockcount()
        for height in range(h - 4, h + 1):
            hashes.append(na.getblockhash(height))
        hex_by_hash = {bh: na.getblock(bh, 0) for bh in hashes}

        # Out-of-order submit to B: tip first (orphan), then fill parents
        order_b = [hashes[-1], hashes[-3], hashes[-4], hashes[-2], hashes[-1]]
        for bh in order_b:
            nb.submitblock(hexdata=hex_by_hash[bh])
        fh.drain(na, nb, nc)

        self.connect_nodes(1, 0)
        self.connect_nodes(2, 0)
        self.sync_blocks([na, nb, nc], timeout=SYNC_TIMEOUT)
        fh.write_fault_artifact(
            self._fault_dir(),
            "FAULT-004",
            "submit_order",
            {"tip0": tip0, "final_tip": na.getbestblockhash(), "hashes": hashes},
        )
        self._assert_triplet(na, nb, nc, "FAULT-004 final")
        return True

    def fault_005_partial_block_propagation(self, na, nb, nc):
        self.log.info("FAULT-005: delayed / duplicate / staggered block relay")
        self._mine_baseline(na, nb, nc, 8)
        self.disconnect_nodes(1, 0)
        self.disconnect_nodes(2, 0)
        self.wait_until(lambda: len(nb.getpeerinfo()) == 0 and len(nc.getpeerinfo()) == 0, timeout=30)

        addr = na.get_deterministic_witness_mining_address()
        self.generatetoaddress(na, 4, addr, sync_fun=lambda: None)
        tip = na.getbestblockhash()
        raw = na.getblock(tip, 0)

        self.connect_nodes(1, 0)
        self.sync_blocks([na, nb], timeout=SYNC_TIMEOUT)
        assert_equal(nb.getblockcount(), na.getblockcount())
        d0 = nb.submitblock(hexdata=raw)
        d1 = nb.submitblock(hexdata=raw)
        self.log.info("FAULT-005 duplicate submit results: %s, %s", d0, d1)
        time.sleep(0.15)
        self.connect_nodes(2, 0)
        self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)
        fh.write_fault_artifact(
            self._fault_dir(),
            "FAULT-005",
            "relay_notes",
            {"duplicate_submit_result_node_b": [d0, d1]},
        )
        self._assert_triplet(na, nb, nc, "FAULT-005 final")
        return True

    def fault_005b_staggered_follower(self, na, nb, nc):
        """FAULT-005 extension: B syncs first; C connects later (staggered delivery)."""
        self.log.info("FAULT-005b: staggered follower delivery")
        self._mine_baseline(na, nb, nc, 6)
        self.disconnect_nodes(1, 0)
        self.disconnect_nodes(2, 0)
        self.wait_until(lambda: len(nb.getpeerinfo()) == 0 and len(nc.getpeerinfo()) == 0, timeout=30)
        addr = na.get_deterministic_witness_mining_address()
        self.generatetoaddress(na, 3, addr, sync_fun=lambda: None)
        tip_hex = na.getblock(na.getbestblockhash(), 0)

        self.connect_nodes(1, 0)
        self.sync_blocks([na, nb], timeout=SYNC_TIMEOUT)
        fh.drain(na, nb)
        fp_b_mid = fh.consensus_fingerprint(nb)

        time.sleep(0.2)
        self.connect_nodes(2, 0)
        self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)
        fh.write_fault_artifact(
            self._fault_dir(),
            "FAULT-005",
            "staggered_follower",
            {
                "node_b_mid_sync_state_hash": fp_b_mid["state_hash"],
                "tip_block_hex_byte_len": len(tip_hex) // 2,
                "final_triplet": fh.triplet_replay_fingerprints((na, nb, nc), ("A", "B", "C")),
            },
        )
        self._assert_triplet(na, nb, nc, "FAULT-005b final")
        return True

    def fault_006_disconnect_during_reorg(self, na, nb, nc):
        self.log.info("FAULT-006: partition + longer fork + delayed follower")
        self._mine_baseline(na, nb, nc, 25)
        self.disconnect_nodes(1, 0)
        self.disconnect_nodes(2, 0)
        self.wait_until(lambda: len(nb.getpeerinfo()) == 0 and len(nc.getpeerinfo()) == 0, timeout=30)

        addr_a = na.get_deterministic_witness_mining_address()
        addr_b = nb.get_deterministic_witness_mining_address()
        self.generatetoaddress(nb, 5, addr_b, sync_fun=lambda: None)

        self.connect_nodes(1, 0)
        self.sync_blocks([na, nb], timeout=SYNC_TIMEOUT)

        self.connect_nodes(2, 0)
        self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)
        fh.write_fault_artifact(
            self._fault_dir(),
            "FAULT-006",
            "reorg_churn_fingerprints",
            {
                "after_fork_sync": fh.triplet_replay_fingerprints((na, nb, nc), ("A", "B", "C")),
            },
        )
        self._assert_triplet(na, nb, nc, "FAULT-006 after fork sync")

        self.disconnect_nodes(1, 0)
        self.wait_until(lambda: len(nb.getpeerinfo()) == 0, timeout=30)
        self.generatetoaddress(na, 1, addr_a, sync_fun=lambda: None)
        self.connect_nodes(1, 0)
        self.sync_blocks([na, nb, nc], timeout=SYNC_TIMEOUT)
        fh.write_fault_artifact(
            self._fault_dir(),
            "FAULT-006",
            "after_reconnect_churn",
            {"fingerprints": fh.triplet_replay_fingerprints((na, nb, nc), ("A", "B", "C"))},
        )
        self._assert_triplet(na, nb, nc, "FAULT-006 after reconnect churn")
        return True

    def fault_007_restart_during_flush(self, na, nb, nc):
        self.log.info("FAULT-007: SIGKILL + restart on follower (replay recovery)")
        self._mine_baseline(na, nb, nc, 40)
        nb_extra = ["-dbcache=4", "-dbbatchsize=4096", "-debug=coindb"]
        self.stop_node(1)
        self.start_node(1, extra_args=self.extra_args[1] + nb_extra)

        # Drop C↔A so connect_nodes(2,0) matches from_num_peers (C already had A as peer).
        self.disconnect_nodes(2, 0)
        self.wait_until(lambda: len(nc.getpeerinfo()) == 0, timeout=30)

        self.connect_nodes(1, 0)
        self.connect_nodes(2, 0)
        self.sync_blocks([na, nb, nc], timeout=SYNC_TIMEOUT)

        addr_b = nb.get_deterministic_witness_mining_address()
        for _ in range(5):
            self.generatetoaddress(
                nb,
                5,
                addr_b,
                sync_fun=lambda: self.sync_blocks([na, nb, nc], timeout=SYNC_TIMEOUT),
            )
        nb.syncwithvalidationinterfacequeue()
        log_start = nb.debug_log_bytes()
        nb.gettxoutsetinfo()
        nb.syncwithvalidationinterfacequeue()
        pre = fh.consensus_fingerprint(nb)
        ok_save, _r_save, exc_save = fh.try_rpc(lambda: nb.savemempool())
        if not ok_save:
            self.log.debug("savemempool before SIGKILL (non-fatal): %s", exc_save)

        nb.process.kill()
        nb.process.wait(timeout=30)
        nb.running = False
        nb.process = None

        self.start_node(1, extra_args=self.extra_args[1] + nb_extra)
        self.connect_nodes(1, 0)
        self.sync_blocks([na, nb, nc], timeout=SYNC_TIMEOUT)

        post = fh.consensus_fingerprint(nb)
        fh.write_fault_artifact(
            self._fault_dir(),
            "FAULT-007",
            "sigkill_replay",
            {"pre": pre, "post": post, "debug_log_start_byte": log_start},
        )
        if pre["state_hash"] != post["state_hash"]:
            raise AssertionError("FAULT-007 NODE-B state_hash after SIGKILL restart")
        self._assert_triplet(na, nb, nc, "FAULT-007 triplet")
        return True

    def fault_008_mempool_divergence(self, na, nb, nc):
        self.log.info("FAULT-008: mempool order / relay divergence must not change consensus")
        self._mine_baseline(na, nb, nc, 20)
        wallet = MiniWallet(na)
        self.generatetodescriptor(
            na,
            COINBASE_MATURITY + 3,
            wallet.get_descriptor(),
            sync_fun=lambda: self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT),
        )
        wallet.rescan_utxos()
        fh.drain(na, nb, nc)

        a1 = wallet.create_self_transfer(fee_rate=Decimal("0.00002"))
        wallet.rescan_utxos()
        a2 = wallet.create_self_transfer(fee_rate=Decimal("0.00002"))

        self.disconnect_nodes(1, 0)
        self.disconnect_nodes(2, 0)
        self.wait_until(lambda: len(nb.getpeerinfo()) == 0 and len(nc.getpeerinfo()) == 0, timeout=30)

        na.sendrawtransaction(a1["hex"], 0)
        na.sendrawtransaction(a2["hex"], 0)
        nb.sendrawtransaction(a2["hex"], 0)
        nb.sendrawtransaction(a1["hex"], 0)
        nc.sendrawtransaction(a1["hex"], 0)
        nc.sendrawtransaction(a2["hex"], 0)

        snap_order = {
            "sorted_txids": fh.snapshot_mempools((na, nb, nc), ("A", "B", "C")),
            "submission_order": {
                "A": [a1["txid"], a2["txid"]],
                "B": [a2["txid"], a1["txid"]],
                "C": [a1["txid"], a2["txid"]],
            },
            "raw_mempool_order": {
                "A": na.getrawmempool(verbose=False),
                "B": nb.getrawmempool(verbose=False),
                "C": nc.getrawmempool(verbose=False),
            },
        }
        fh.write_fault_artifact(self._fault_dir(), "FAULT-008", "mempool_order_isolated", snap_order)

        self.connect_nodes(1, 0)
        self.connect_nodes(2, 0)
        self.sync_mempools(self.nodes, timeout=SYNC_TIMEOUT)

        addr = na.get_deterministic_witness_mining_address()
        self.generatetoaddress(
            na,
            1,
            addr,
            sync_fun=lambda: self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT),
        )
        fh.write_fault_artifact(self._fault_dir(), "FAULT-008", "mempools_after_block", fh.snapshot_mempools((na, nb, nc), ("A", "B", "C")))

        # Post-confirmation rebroadcast: identical failure tuples across nodes (must not alter chainstate).
        rb_errs = []
        for n in (na, nb, nc):
            ok, _r, exc = fh.try_rpc(lambda _n=n: _n.sendrawtransaction(a1["hex"], 0))
            assert not ok and exc is not None
            rb_errs.append(exc)
        fh.assert_identical_rpc_errors(rb_errs, ["A", "B", "C"], "FAULT-008 rebroadcast spent tx")
        fh.write_fault_artifact(
            self._fault_dir(),
            "FAULT-008",
            "rebroadcast_spent_tx_rpc",
            {"rpc_error": [fh.rpc_error_tuple(e) for e in rb_errs]},
        )

        self._assert_triplet(na, nb, nc, "FAULT-008 post-block")
        return True

    def run_test(self):
        os.makedirs(self._fault_dir(), exist_ok=True)
        trace = self._trace_path()
        if os.path.isfile(trace):
            os.remove(trace)

        na, nb, nc = self.nodes
        fault_tests = [
            self.fault_001_malformed_transaction,
            self.fault_002_malformed_witness,
            self.fault_003_invalid_mint,
            self.fault_004_orphan_storm,
            self.fault_005_partial_block_propagation,
            self.fault_005b_staggered_follower,
            self.fault_006_disconnect_during_reorg,
            self.fault_007_restart_during_flush,
            self.fault_008_mempool_divergence,
        ]
        results = {}
        for fn in fault_tests:
            name = fn.__name__
            try:
                results[name] = bool(fn(na, nb, nc))
            except Exception as e:
                self.log.error("%s failed: %s", name, e)
                fh.write_fault_artifact(
                    self._fault_dir(),
                    "HARNESS",
                    name,
                    {"error": f"{type(e).__name__}: {e}"},
                )
                raise

        fh.finalize_sorted_trace(
            trace,
            [
                "# RIGOR fault injection trace - sorted body rows",
                "# columns: fault_id, case_id, field, value",
            ],
        )
        fh.write_triplet_snapshot(
            os.path.join(self._fault_dir(), "FAULT-SUITE__triplet_replay_fingerprints.json"),
            {
                "fingerprints": fh.triplet_replay_fingerprints(self.nodes, ("A", "B", "C")),
                "identical_triplet": True,
            },
        )
        passed = sum(1 for v in results.values() if v)
        self.log.info("FAULT suite finished: %d/%d", passed, len(fault_tests))
        assert passed == len(fault_tests)


if __name__ == "__main__":
    FaultInjectionTest().main()
