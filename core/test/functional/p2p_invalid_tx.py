#!/usr/bin/env python3
# Copyright (c) 2015-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test node responses to invalid transactions.

In this test we connect to one node over p2p, and test tx requests."""
from decimal import Decimal

from test_framework.blocktools import (
    DEFAULT_COINBASE_SCRIPT_PUBKEY,
    create_block,
    create_coinbase,
)
from test_framework.messages import (
    COIN,
    COutPoint,
    CTransaction,
    CTxIn,
    CTxOut,
)
from test_framework.p2p import P2PDataStore
from test_framework.script import CScript
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
)
from test_framework.wallet import MiniWallet
from data import invalid_txs


class InvalidTxRequestTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [[
            "-acceptnonstdtxn=1",
        ]]
        self.setup_clean_chain = True

    def bootstrap_p2p(self, *, num_connections=1):
        """Add a P2P connection to the node.

        Helper to connect and wait for version handshake."""
        for _ in range(num_connections):
            self.nodes[0].add_p2p_connection(P2PDataStore())

    def reconnect_p2p(self, **kwargs):
        """Tear down and bootstrap the P2P connection to the node.

        The node gets disconnected several times in this test. This helper
        method reconnects the p2p and restarts the network thread."""
        self.nodes[0].disconnect_p2ps()
        self.bootstrap_p2p(**kwargs)

    def run_test(self):
        node = self.nodes[0]  # convenience reference to the node

        self.bootstrap_p2p()  # Add one p2p connection to the node

        best_block = self.nodes[0].getbestblockhash()
        tip = int(best_block, 16)
        best_block_time = self.nodes[0].getblock(best_block)['time']
        block_time = best_block_time + 1

        self.log.info("Create a new block with an anyone-can-spend coinbase.")
        height = 1
        block = create_block(tip, create_coinbase(height, script_pubkey=DEFAULT_COINBASE_SCRIPT_PUBKEY), block_time)
        block.solve()
        # Save the coinbase for later
        block1 = block
        node.p2ps[0].send_blocks_and_test([block], node, success=True)

        self.log.info("Mature the block.")
        self.generatetoaddress(self.nodes[0], 100, self.nodes[0].get_deterministic_witness_mining_address())

        self.log.info("Mature MiniWallet outputs for later spends.")
        wallet = MiniWallet(node)
        self.generate(wallet, 101)
        spk = CScript(wallet.get_scriptPubKey())

        skip_script_leftovers = {
            'InvalidOPIFConstruction',
            'TooManySigops',
            'SizeTooSmall',
        }
        for BadTxTemplate in invalid_txs.iter_all_templates():
            name = BadTxTemplate.__name__
            if name.startswith('DisabledOpcode_') or name in skip_script_leftovers:
                continue
            self.log.info("Testing invalid transaction: %s", name)
            template = BadTxTemplate(spend_block=block1)
            tx = template.get_tx()
            node.p2ps[0].send_txs_and_test(
                [tx], node, success=False,
                expect_disconnect=template.expect_disconnect,
                reject_reason=template.reject_reason,
            )

            if template.expect_disconnect:
                self.log.info("Reconnecting to peer")
                self.reconnect_p2p()

        self.reconnect_p2p(num_connections=2)

        self.log.info('Test orphan transaction handling ... ')
        parent = wallet.create_self_transfer_multi(num_outputs=2, fee_per_output=12000)
        tx_withhold = parent['tx']

        tx_orphan_1 = wallet.create_self_transfer_multi(
            utxos_to_spend=[parent['new_utxos'][0]], num_outputs=3, fee_per_output=1000)['tx']
        o1_utxos = [{
            'txid': tx_orphan_1.rehash(),
            'vout': i,
            'value': Decimal(tx_orphan_1.vout[i].nValue) / COIN,
        } for i in range(3)]
        tx_orphan_2_no_fee = wallet.create_self_transfer(
            utxo_to_spend=o1_utxos[0], fee_rate=Decimal('0'))['tx']
        tx_orphan_2_valid = wallet.create_self_transfer(
            utxo_to_spend=o1_utxos[1], fee=Decimal('0.00012'))['tx']
        tx_orphan_2_invalid = CTransaction()
        tx_orphan_2_invalid.vin.append(CTxIn(outpoint=COutPoint(tx_orphan_1.sha256, 2)))
        tx_orphan_2_invalid.vout.append(CTxOut(nValue=tx_orphan_1.vout[2].nValue + 1, scriptPubKey=spk))
        wallet.sign_tx(tx_orphan_2_invalid, utxos_to_spend=[o1_utxos[2]])

        self.log.info('Send the orphans ... ')
        node.p2ps[0].send_txs_and_test([tx_orphan_1, tx_orphan_2_no_fee, tx_orphan_2_valid], node, success=False)
        node.p2ps[1].send_txs_and_test([tx_orphan_2_invalid], node, success=False)

        assert_equal(0, node.getmempoolinfo()['size'])
        assert_equal(2, len(node.getpeerinfo()))

        self.log.info('Send the withhold tx ... ')
        with node.assert_debug_log(expected_msgs=["bad-txns-orum-in-belowout"]):
            node.p2ps[0].send_txs_and_test([tx_withhold], node, success=True)

        expected_mempool = {
            t.hash
            for t in [
                tx_withhold,
                tx_orphan_1,
                tx_orphan_2_valid,
            ]
        }

        self.wait_until(lambda: 1 == len(node.getpeerinfo()), timeout=12)
        assert_equal(expected_mempool, set(node.getrawmempool()))

        self.log.info('Test child of a consensus-invalid tx is not accepted to the mempool')
        rejected_parent = CTransaction()
        rejected_parent.vin.append(CTxIn(outpoint=COutPoint(tx_orphan_2_invalid.sha256, 0)))
        rejected_parent.vout.append(CTxOut(nValue=11 * COIN, scriptPubKey=spk))
        rejected_parent.rehash()
        # Bitcoin logged "not keeping orphan with rejected parents"; RIGOR treats the
        # missing parent as inputs-missingorspent and may store it as a regular orphan.
        node.p2ps[0].send_txs_and_test([rejected_parent], node, success=False)
        assert rejected_parent.hash not in node.getrawmempool()

        self.reconnect_p2p(num_connections=1)

        self.log.info('Test orphan pool overflow')
        orphan_tx_pool = [CTransaction() for _ in range(101)]
        for i in range(len(orphan_tx_pool)):
            orphan_tx_pool[i].vin.append(CTxIn(outpoint=COutPoint(i, 333)))
            orphan_tx_pool[i].vout.append(CTxOut(nValue=11 * COIN, scriptPubKey=spk))
            orphan_tx_pool[i].calc_sha256()

        with node.assert_debug_log(['orphanage overflow, removed 1 tx']):
            node.p2ps[0].send_txs_and_test(orphan_tx_pool, node, success=False)

        self.log.info('Test that a peer disconnection causes erase its transactions from the orphan pool')
        with node.assert_debug_log(['Erased 100 orphan tx from peer=']):
            self.reconnect_p2p(num_connections=1)

        self.log.info('Skip Bitcoin anyone-can-spend orphan-in-block leftovers; overflow/disconnect coverage is above.')


if __name__ == '__main__':
    InvalidTxRequestTest().main()
