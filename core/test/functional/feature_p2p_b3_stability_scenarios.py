#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""TASK-B5-002 PHASE-B3: compressed restart / reorg / P2P stability scenarios (CI).

Complements ``scripts/p2p_b3_longrun_node_stability.sh`` (72h+ ops harness).
Stdout on success: ``P2P-B3-STABILITY: PASS``
"""

from __future__ import annotations

import os
import signal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.p2p_propagation_util import assert_tips_equal
from test_framework.util import assert_equal


class P2PB3StabilityScenariosTest(BitcoinTestFramework):
    def set_test_params(self):
        self.chain = "devtest"
        self.setup_clean_chain = True
        self.num_nodes = 3
        self.extra_args = [[], [], []]

    def _det_hash(self, node):
        return node.getdeterministicstatehash()["state_hash"]

    def _assert_triple_determinism(self, label: str):
        self.nodes[0].syncwithvalidationinterfacequeue()
        self.nodes[1].syncwithvalidationinterfacequeue()
        self.nodes[2].syncwithvalidationinterfacequeue()
        hashes = [self._det_hash(n) for n in self.nodes]
        if hashes[0] != hashes[1]:
            raise AssertionError(f"{label} state_hash n0 vs n1: {hashes[0]} != {hashes[1]}")
        if hashes[1] != hashes[2]:
            raise AssertionError(f"{label} state_hash n1 vs n2: {hashes[1]} != {hashes[2]}")
        assert_tips_equal(self.nodes, label)

    def _mine_bootstrap(self, nblocks=120):
        addr = self.nodes[0].get_deterministic_witness_mining_address()
        self.generatetoaddress(self.nodes[0], nblocks, addr)
        self.sync_all()

    def scenario_clean_restart(self):
        self.log.info("=== clean shutdown + restart node1 ===")
        pre = self._det_hash(self.nodes[1])
        self.restart_node(1)
        self.connect_nodes(1, 0)
        self.connect_nodes(2, 1)
        self.sync_all()
        post = self._det_hash(self.nodes[1])
        assert_equal(pre, post)
        self._assert_triple_determinism("clean_restart")

    def _kill_node(self, idx: int, sig: int) -> None:
        n = self.nodes[idx]
        os.kill(n.process.pid, sig)
        if sig == signal.SIGKILL:
            n.process.wait(timeout=30)
            n.running = False
        else:
            n.wait_until_stopped(timeout=60)

    def scenario_sigterm(self):
        self.log.info("=== SIGTERM node1 ===")
        n1 = self.nodes[1]
        pre = self._det_hash(n1)
        self._kill_node(1, signal.SIGTERM)
        self.start_node(1)
        self.connect_nodes(0, 1)
        self.connect_nodes(1, 2)
        self.sync_all()
        assert_equal(pre, self._det_hash(n1))
        self._assert_triple_determinism("sigterm")

    def scenario_sigkill(self):
        self.log.info("=== SIGKILL node1 ===")
        n1 = self.nodes[1]
        pre = self._det_hash(n1)
        self._kill_node(1, signal.SIGKILL)
        self.start_node(1)
        self.connect_nodes(0, 1)
        self.connect_nodes(1, 2)
        self.sync_all()
        assert_equal(pre, self._det_hash(n1))
        self._assert_triple_determinism("sigkill")

    def scenario_mempool_load_restart(self):
        self.log.info("=== restart during gettxoutsetinfo flush ===")
        n1 = self.nodes[1]
        pre = self._det_hash(n1)
        n1.gettxoutsetinfo()
        self._kill_node(1, signal.SIGTERM)
        self.start_node(1)
        self.connect_nodes(0, 1)
        self.connect_nodes(1, 2)
        self.sync_all()
        assert_equal(pre, self._det_hash(n1))
        self._assert_triple_determinism("mempool_load_restart")

    def scenario_reorg_1_and_10(self):
        self.log.info("=== 1-block reorg ===")
        n0, n1, n2 = self.nodes
        a0 = n0.get_deterministic_witness_mining_address()
        a1 = n1.get_deterministic_witness_mining_address()
        self.disconnect_nodes(0, 1)
        self.disconnect_nodes(1, 2)
        self.generatetoaddress(n0, 1, a0, sync_fun=lambda: None)
        self.generatetoaddress(n1, 2, a1, sync_fun=lambda: None)
        self.connect_nodes(0, 1)
        self.connect_nodes(1, 2)
        self.sync_all()
        self._assert_triple_determinism("reorg_1")

        self.log.info("=== 10-block reorg ===")
        self.disconnect_nodes(0, 1)
        self.disconnect_nodes(1, 2)
        self.generatetoaddress(n0, 10, a0, sync_fun=lambda: None)
        self.generatetoaddress(n1, 20, a1, sync_fun=lambda: None)
        self.connect_nodes(0, 1)
        self.connect_nodes(1, 2)
        self.sync_all()
        self._assert_triple_determinism("reorg_10")

    def scenario_delayed_and_partition(self):
        self.log.info("=== delayed propagation + partition reconnect ===")
        n0, n1, n2 = self.nodes
        a0 = n0.get_deterministic_witness_mining_address()
        self.disconnect_nodes(1, 2)
        self.wait_until(lambda: len(n2.getpeerinfo()) == 0, timeout=30)
        hashes = self.generatetoaddress(n0, 5, a0, sync_fun=lambda: None)
        self.sync_blocks([n0, n1])
        h2_before = n2.getblockcount()
        self.connect_nodes(1, 2)
        self.sync_all()
        assert n2.getblockcount() > h2_before
        assert_equal(n2.getbestblockhash(), hashes[-1])
        self._assert_triple_determinism("delayed_propagation")

    def scenario_slow_peer_catchup(self):
        self.log.info("=== slow peer catch-up ===")
        n0, n1, n2 = self.nodes
        a0 = n0.get_deterministic_witness_mining_address()
        self.disconnect_nodes(1, 2)
        self.wait_until(lambda: len(n2.getpeerinfo()) == 0, timeout=30)
        self.generatetoaddress(n0, 15, a0, sync_fun=lambda: None)
        self.sync_blocks([n0, n1])
        self.connect_nodes(1, 2)
        self.sync_all()
        self._assert_triple_determinism("slow_peer")

    def scenario_intermittent_disconnect(self):
        self.log.info("=== intermittent disconnect ===")
        self.disconnect_nodes(1, 0)
        self.disconnect_nodes(0, 1)
        self.connect_nodes(1, 0)
        self.sync_all()
        self._assert_triple_determinism("intermittent_disconnect")

    def run_test(self):
        self._mine_bootstrap(120)
        self._assert_triple_determinism("bootstrap")

        self.scenario_clean_restart()
        self.scenario_sigterm()
        self.scenario_mempool_load_restart()
        self.scenario_reorg_1_and_10()
        self.scenario_delayed_and_partition()
        self.scenario_slow_peer_catchup()
        self.scenario_intermittent_disconnect()
        self.scenario_sigkill()

        self.log.info("P2P-B3-STABILITY: PASS")


if __name__ == "__main__":
    P2PB3StabilityScenariosTest().main()
