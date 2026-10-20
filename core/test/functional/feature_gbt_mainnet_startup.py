#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Exercise the production getblocktemplate startup policy on chain=main.

Regtest sets IsTestChain and skips EvaluateGbtStartup. This test starts real
mainnet chain parameters (pinned genesis, network id "main"). It does not hash
at mainnet difficulty and does not submit a block.

The mainnet operations baseline rejects -mocktime and -minimumchainwork, and
this test does not weaken that. Before the genesis template window, a request
that passes the production gate then fails in CreateNewBlock with time-too-new.
That failure is after the gate: -9 and -10 are returned earlier. A call that
omits the segwit rule reaches the segwit parameter check only after the gate
allows the request.

RPC -8 for a wrong network id or a genesis mismatch cannot be produced by a
consistent binary: only chain "main" leaves the test-chain bypass, and startup
asserts the genesis hash. Those decisions are covered by gbt_startup_policy_tests.
A missing active tip is likewise unreachable once the node has finished loading
genesis; the same unit test covers it, including with -solomining. A live
genesis-only node is not in IBD while its tip work equals the default minimum
and the tip is not 24 hours old, so the production -10 branch is not forced here.
"""
from test_framework.authproxy import JSONRPCException
from test_framework.messages import msg_rigorcap
from test_framework.p2p import P2PInterface
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
    p2p_port,
    rpc_port,
)

MAINNET_GENESIS = "e2ce92cfef740f92e8cc84a8541d8e760c776c668afd9b61740b3a4315241468"
GBT_RULES = {"rules": ["segwit"]}


class NoVerackPeer(P2PInterface):
    """Complete version exchange but never send verack."""

    def on_version(self, message):
        self.send_message(msg_rigorcap())
        self.nServices = message.nServices


class GbtMainnetStartupTest(BitcoinTestFramework):
    def set_test_params(self):
        self.chain = "main"
        self.setup_clean_chain = True
        self.num_nodes = 4
        self.extra_args = [
            [],
            [],
            [],
            ["-solomining=1"],
        ]

    def setup_network(self):
        self.setup_nodes()

    def setup_nodes(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        seen = set()
        for n in self.nodes:
            endpoint = (p2p_port(n.index), rpc_port(n.index))
            if endpoint[0] in seen or endpoint[1] in seen:
                raise AssertionError(f"node {n.index} reused an endpoint {endpoint}")
            seen.update(endpoint)
            self.log.info(
                f"Starting node {n.index} p2p=127.0.0.1:{endpoint[0]} rpc=127.0.0.1:{endpoint[1]}"
            )
            try:
                n.start()
                n.wait_for_rpc_connection()
            except Exception as exc:
                n.running = False
                detail = ""
                stderr = getattr(n, "stderr", None)
                if stderr is not None:
                    stderr.seek(0)
                    detail = stderr.read().decode("utf-8", errors="replace").strip()
                raise AssertionError(
                    f"node {n.index} failed to start "
                    f"(p2p 127.0.0.1:{endpoint[0]}, rpc 127.0.0.1:{endpoint[1]}): {exc}\n{detail}"
                ) from exc
            if f"127.0.0.1:{endpoint[1]}" not in n.url:
                raise AssertionError(
                    f"node {n.index} RPC url {n.url} does not target its own rpc port"
                )

    def run_test(self):
        node_a, node_b, node_c, node_solo = self.nodes
        self.log.info("Identity: chain main and the pinned genesis, not in IBD")
        for n in self.nodes:
            info = n.getblockchaininfo()
            assert_equal(info["chain"], "main")
            assert_equal(info["blocks"], 0)
            assert_equal(n.getblockhash(0), MAINNET_GENESIS)
            assert_equal(info["initialblockdownload"], False)
            assert_equal(int(info["chainwork"], 16), 0x1FFFFFFFF)
            self.log.info(
                f"node{n.index} chain={info['chain']} height={info['blocks']} "
                f"hash={info['bestblockhash']} ibd={info['initialblockdownload']} "
                f"chainwork={info['chainwork']}"
            )

        self.log.info("No eligible peer: production policy returns -9")
        assert_equal(len(node_a.getpeerinfo()), 0)
        assert_raises_rpc_error(-9, "not connected", node_a.getblocktemplate, GBT_RULES)

        self.log.info("Unfinished handshake is visible but not eligible")
        node_a.add_p2p_connection(NoVerackPeer(), wait_for_verack=False)
        self.wait_until(lambda: len(node_a.getpeerinfo()) == 1, timeout=10)
        peer = node_a.getpeerinfo()[0]
        assert peer.get("version", 0) > 0
        assert "verack" not in peer.get("bytesrecv_per_msg", {})
        assert_raises_rpc_error(-9, "not connected", node_a.getblocktemplate, GBT_RULES)
        node_a.p2ps[0].peer_disconnect()
        self.wait_until(lambda: len(node_a.getpeerinfo()) == 0, timeout=10)
        assert_raises_rpc_error(-9, "not connected", node_a.getblocktemplate, GBT_RULES)

        self.log.info("Handshake-complete peer passes the production gate")
        self.connect_nodes(1, 0)
        self.wait_for_received_verack(node_a, "testnode1")
        self.wait_for_received_verack(node_b, "testnode0")
        eligible = self.eligible_peer(node_a, "testnode1")
        assert eligible["connection_type"] not in ("feeler", "addr-fetch")
        # The segwit parameter check is after the production gate. Reaching it
        # means network identity, genesis identity, peer eligibility, and IBD
        # all allowed the request.
        assert_raises_rpc_error(-8, "segwit rule", node_a.getblocktemplate)
        outcome = self.gate_passed_then_clock(node_a)
        self.log.info(f"node A after verack: {outcome}")
        outcome_b = self.gate_passed_then_clock(node_b)
        self.log.info(f"node B after verack: {outcome_b}")

        self.log.info("Disconnecting the peer withdraws eligibility")
        self.disconnect_nodes(1, 0)
        self.wait_until(lambda: len(node_a.getpeerinfo()) == 0, timeout=10)
        assert_raises_rpc_error(-9, "not connected", node_a.getblocktemplate, GBT_RULES)
        assert_raises_rpc_error(-9, "not connected", node_b.getblocktemplate, GBT_RULES)

        self.log.info("Third node joins without an endpoint collision")
        self.connect_nodes(2, 0)
        self.connect_nodes(2, 1)
        self.wait_for_received_verack(node_a, "testnode2")
        self.wait_for_received_verack(node_c, "testnode0")
        self.wait_for_received_verack(node_b, "testnode2")
        outcome_c = self.gate_passed_then_clock(node_c)
        self.log.info(f"node C after verack: {outcome_c}")

        self.log.info("solomining skips the zero-peer refusal; identity still runs first")
        assert_equal(len(node_solo.getpeerinfo()), 0)
        assert_equal(node_solo.getblockchaininfo()["initialblockdownload"], False)
        assert_raises_rpc_error(-8, "segwit rule", node_solo.getblocktemplate)
        outcome_solo = self.gate_passed_then_clock(node_solo)
        self.log.info(f"node solo with zero peers: {outcome_solo}")

    def gate_passed_then_clock(self, node):
        """A request the gate allows. Before the template window this is time-too-new."""
        try:
            tmpl = node.getblocktemplate(GBT_RULES)
        except JSONRPCException as err:
            code = err.error["code"]
            message = err.error["message"]
            assert code not in (-8, -9, -10), message
            assert "time-too-new" in message, message
            assert "not connected" not in message
            assert "initial sync" not in message
            assert "genesis identity" not in message
            assert "requires chain main" not in message
            return f"error {code}: {message}"
        assert_equal(tmpl["height"], 1)
        assert_equal(tmpl["previousblockhash"], MAINNET_GENESIS)
        assert_equal(tmpl["rigor_coinbase_immutable"], True)
        assert "segwit" in tmpl["rules"]
        return f"template height={tmpl['height']} prev={tmpl['previousblockhash']}"

    def wait_for_received_verack(self, node, subver_tag):
        def ready():
            return self.eligible_peer(node, subver_tag) is not None
        self.wait_until(ready, timeout=30)

    def eligible_peer(self, node, subver_tag):
        for peer in node.getpeerinfo():
            if subver_tag not in peer.get("subver", ""):
                continue
            if "verack" not in peer.get("bytesrecv_per_msg", {}):
                continue
            return peer
        return None


if __name__ == "__main__":
    GbtMainnetStartupTest().main()
