#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Three-node regtest propagation, reorg, and timestamp boundaries.

Regtest deliberately skips the mainnet getblocktemplate production policy.
This file checks networking, block acceptance, most-work selection, and the
existing time-too-old / time-too-new rules. It does not change those rules
and it does not prove the production policy (see feature_gbt_mainnet_startup.py).
"""
from decimal import Decimal

from test_framework.address import ADDRESS_BCRT1_UNSPENDABLE
from test_framework.blocktools import (
    DEFAULT_COINBASE_SCRIPT_PUBKEY,
    MAX_FUTURE_BLOCK_TIME,
    create_block,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class GbtRegtestMeshTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 3

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def setup_network(self):
        self.setup_nodes()
        self.connect_nodes(1, 0)
        self.connect_nodes(2, 0)
        self.connect_nodes(2, 1)
        self.sync_all()

    def run_test(self):
        nodes = self.nodes
        self.log.info("Genesis agreement")
        genesis = [n.getblockhash(0) for n in nodes]
        assert_equal(genesis[0], genesis[1])
        assert_equal(genesis[0], genesis[2])
        self.snapshot("genesis")

        self.log.info("Height 1 propagates on the connected mesh")
        self.generatetoaddress(nodes[0], 1, ADDRESS_BCRT1_UNSPENDABLE, sync_fun=self.no_op)
        self.sync_blocks()
        self.snapshot("after-height-1")
        assert_equal(nodes[0].getblockcount(), 1)
        tip1 = nodes[0].getbestblockhash()
        for n in nodes:
            assert_equal(n.getblockcount(), 1)
            assert_equal(n.getbestblockhash(), tip1)

        self.log.info("A second block propagates")
        self.generatetoaddress(nodes[0], 1, ADDRESS_BCRT1_UNSPENDABLE, sync_fun=self.no_op)
        self.sync_blocks()
        self.snapshot("after-height-2")
        tip2 = nodes[0].getbestblockhash()
        for n in nodes:
            assert_equal(n.getblockcount(), 2)
            assert_equal(n.getbestblockhash(), tip2)

        self.log.info("Disconnect node 2, extend the connected pair, then reconnect")
        self.disconnect_nodes(2, 0)
        self.disconnect_nodes(2, 1)
        self.generatetoaddress(nodes[0], 1, ADDRESS_BCRT1_UNSPENDABLE, sync_fun=self.no_op)
        self.sync_blocks([nodes[0], nodes[1]])
        self.snapshot("node2-isolated")
        isolated_tip = nodes[0].getbestblockhash()
        assert_equal(nodes[0].getblockcount(), 3)
        assert_equal(nodes[1].getblockcount(), 3)
        assert_equal(nodes[1].getbestblockhash(), isolated_tip)
        assert_equal(nodes[2].getblockcount(), 2)
        assert_equal(nodes[2].getbestblockhash(), tip2)
        self.connect_nodes(2, 0)
        self.sync_blocks()
        self.snapshot("node2-rejoined")
        for n in nodes:
            assert_equal(n.getblockcount(), 3)
            assert_equal(n.getbestblockhash(), isolated_tip)

        self.log.info("Two valid branches from the same parent; more work wins")
        self.disconnect_nodes(2, 0)
        parent = nodes[0].getbestblockhash()
        assert_equal(nodes[2].getbestblockhash(), parent)
        # Distinct payout scripts, so the stale coinbase txid cannot also be
        # the height-4 coinbase of the heavier branch.
        short_addr = nodes[0].get_deterministic_witness_mining_address()
        long_addr = nodes[2].get_deterministic_witness_mining_address()
        short_hashes = self.generatetoaddress(nodes[0], 1, short_addr, sync_fun=self.no_op)
        long_hashes = self.generatetoaddress(nodes[2], 2, long_addr, sync_fun=self.no_op)
        self.sync_blocks([nodes[0], nodes[1]])
        self.snapshot("competing-before-reconnect")
        assert_equal(nodes[0].getblockcount(), 4)
        assert_equal(nodes[1].getbestblockhash(), short_hashes[-1])
        assert_equal(nodes[2].getblockcount(), 5)
        assert_equal(nodes[2].getbestblockhash(), long_hashes[-1])
        assert nodes[0].getbestblockhash() != nodes[2].getbestblockhash()
        stale = nodes[0].getblock(short_hashes[-1], 2)
        winner = nodes[2].getblock(long_hashes[-1], 2)
        stale_txid = stale["tx"][0]["txid"]
        winner_txid = winner["tx"][0]["txid"]
        assert_equal(stale["tx"][0]["vout"][0]["value"], Decimal("128"))
        assert_equal(winner["tx"][0]["vout"][0]["value"], Decimal("128"))
        self.connect_nodes(2, 0)
        self.sync_blocks()
        self.snapshot("competing-after-reconnect")
        for n in nodes:
            assert_equal(n.getblockcount(), 5)
            assert_equal(n.getbestblockhash(), long_hashes[-1])
        assert_equal(nodes[0].gettxout(stale_txid, 0), None)
        spent = nodes[0].gettxout(winner_txid, 0)
        assert spent is not None
        assert_equal(spent["value"], Decimal("128"))

        if self.is_wallet_compiled():
            nodes[0].createwallet("probe")
            text = nodes[0].help("destroyoru")
            assert "destroyoru" in text

        self.time_boundaries(nodes[0])

    def time_boundaries(self, node):
        self.log.info("Timestamp boundaries via proposal and submitblock")
        tip = node.getblockheader(node.getbestblockhash())
        mtp = tip["mediantime"]
        now = mtp + 3 * MAX_FUTURE_BLOCK_TIME
        node.setmocktime(now)
        height = node.getblockcount() + 1
        prev = int(tip["hash"], 16)

        def make(ntime):
            block = create_block(
                hashprev=prev,
                ntime=ntime,
                coinbase_script_pubkey=DEFAULT_COINBASE_SCRIPT_PUBKEY,
                tmpl={"height": height},
            )
            block.solve()
            return block

        def propose(block):
            return node.getblocktemplate({
                "data": block.serialize().hex(),
                "mode": "proposal",
                "rules": ["segwit"],
            })

        too_old = make(mtp)
        assert_equal(propose(too_old), "time-too-old")
        assert_equal(node.submitblock(too_old.serialize().hex()), "time-too-old")

        lower = make(mtp + 1)
        assert_equal(propose(lower), None)

        too_new = make(now + MAX_FUTURE_BLOCK_TIME + 1)
        assert_equal(propose(too_new), "time-too-new")
        assert_equal(node.submitblock(too_new.serialize().hex()), "time-too-new")

        inside = make(now)
        assert_equal(propose(inside), None)
        assert_equal(node.submitblock(inside.serialize().hex()), None)
        assert_equal(node.getbestblockhash(), inside.hash)

    def snapshot(self, label):
        for n in self.nodes:
            info = n.getblockchaininfo()
            self.log.info(
                f"{label} node{n.index} height={info['blocks']} hash={info['bestblockhash']}"
            )


if __name__ == "__main__":
    GbtRegtestMeshTest().main()
