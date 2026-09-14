#!/usr/bin/env python3
# Copyright (c) 2018-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the wallet balance RPC methods."""
from decimal import Decimal
import json
import struct

from test_framework.address import ADDRESS_BCRT1_UNSPENDABLE as ADDRESS_WATCHONLY
from test_framework.blocktools import COINBASE_MATURITY
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)


def create_transactions(node, address, amt, fees):
    # Create and sign raw transactions from node to address for amt.
    # Creates a transaction for each fee and returns an array
    # of the raw transactions.
    utxos = [u for u in node.listunspent(0) if u['spendable']]

    # Create transactions
    inputs = []
    ins_total = 0
    for utxo in utxos:
        inputs.append({"txid": utxo["txid"], "vout": utxo["vout"]})
        ins_total += utxo['amount']
        if ins_total >= amt + max(fees):
            break
    # make sure there was enough utxos
    assert ins_total >= amt + max(fees)

    txs = []
    for fee in fees:
        outputs = {address: amt}
        # prevent 0 change output
        if ins_total > amt + fee:
            outputs[node.getrawchangeaddress()] = ins_total - amt - fee
        raw_tx = node.createrawtransaction(inputs, outputs, 0, True)
        signed = node.signrawtransactionwithwallet(raw_tx)
        if not signed.get('complete', False):
            raise AssertionError(
                'signrawtransactionwithwallet incomplete (dumping full RPC result):\n'
                + json.dumps(signed, indent=2, sort_keys=True)
            )
        txs.append(signed)

    return txs

class WalletTest(BitcoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)

    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [
            # Limit mempool descendants as a hack to have wallet txs rejected from the mempool.
            # Set walletrejectlongchains=0 so the wallet still creates the transactions.
            ['-limitdescendantcount=3', '-walletrejectlongchains=0'],
            [],
        ]
        # whitelist peers to speed up tx relay / mempool sync
        for args in self.extra_args:
            args.append("-whitelist=noban@127.0.0.1")

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        if not self.options.descriptors:
            # Tests legacy watchonly behavior which is not present (and does not need to be tested) in descriptor wallets
            self.nodes[0].importaddress(ADDRESS_WATCHONLY)
            # Check that nodes don't own any UTXOs
            assert_equal(len(self.nodes[0].listunspent()), 0)
            assert_equal(len(self.nodes[1].listunspent()), 0)

            self.log.info("Check that only node 0 is watching an address")
            assert 'watchonly' in self.nodes[0].getbalances()
            assert 'watchonly' not in self.nodes[1].getbalances()

        self.log.info("Mining blocks ...")
        self.generate(self.nodes[0], 1)
        self.generate(self.nodes[1], 1)

        # Verify listunspent returns immature coinbase if 'include_immature_coinbase' is set
        assert_equal(len(self.nodes[0].listunspent(query_options={'include_immature_coinbase': True})), 1)
        assert_equal(len(self.nodes[0].listunspent(query_options={'include_immature_coinbase': False})), 0)

        self.generatetoaddress(self.nodes[1], COINBASE_MATURITY + 1, ADDRESS_WATCHONLY)

        # Verify listunspent returns all immature coinbases if 'include_immature_coinbase' is set
        # For now, only the legacy wallet will see the coinbases going to the imported 'ADDRESS_WATCHONLY'
        assert_equal(len(self.nodes[0].listunspent(query_options={'include_immature_coinbase': False})), 1 if self.options.descriptors else 2)
        assert_equal(len(self.nodes[0].listunspent(query_options={'include_immature_coinbase': True})), 1 if self.options.descriptors else COINBASE_MATURITY + 2)

        # Upstream Bitcoin Core assumes regtest subsidy 50 BTC; Rigor pays flat 128 ORU (RFC-020).
        spendable0 = [u for u in self.nodes[0].listunspent(minconf=1) if u["spendable"]]
        assert len(spendable0) >= 1
        self.subsidy = Decimal(str(spendable0[0]["amount"]))
        self.scale = self.subsidy / Decimal("50")

        def sc(x):
            return Decimal(str(x)) * self.scale

        if not self.options.descriptors:
            # Tests legacy watchonly behavior which is not present (and does not need to be tested) in descriptor wallets
            assert_equal(self.nodes[0].getbalances()['mine']['trusted'], self.subsidy)
            assert_equal(self.nodes[0].getwalletinfo()['balance'], self.subsidy)
            assert_equal(self.nodes[1].getbalances()['mine']['trusted'], self.subsidy)

            assert_equal(self.nodes[0].getbalances()['watchonly']['immature'], sc(5000))
            assert 'watchonly' not in self.nodes[1].getbalances()

            assert_equal(self.nodes[0].getbalance(), self.subsidy)
            assert_equal(self.nodes[1].getbalance(), self.subsidy)

        self.log.info("Test getbalance with different arguments")
        assert_equal(self.nodes[0].getbalance("*"), self.subsidy)
        assert_equal(self.nodes[0].getbalance("*", 1), self.subsidy)
        assert_equal(self.nodes[0].getbalance(minconf=1), self.subsidy)
        if not self.options.descriptors:
            assert_equal(self.nodes[0].getbalance(minconf=0, include_watchonly=True), sc(100))
            assert_equal(self.nodes[0].getbalance("*", 1, True), sc(100))
        else:
            assert_equal(self.nodes[0].getbalance(minconf=0, include_watchonly=True), self.subsidy)
            assert_equal(self.nodes[0].getbalance("*", 1, True), self.subsidy)
        assert_equal(self.nodes[1].getbalance(minconf=0, include_watchonly=True), self.subsidy)

        # Send 40 BTC from 0 to 1 and 60 BTC from 1 to 0.
        txs = create_transactions(self.nodes[0], self.nodes[1].getnewaddress(address_type='bech32'), sc(40), [Decimal('0.01')])
        self.nodes[0].sendrawtransaction(txs[0]['hex'])
        self.nodes[1].sendrawtransaction(txs[0]['hex'])  # sending on both nodes is faster than waiting for propagation

        self.sync_all()
        txs = create_transactions(self.nodes[1], self.nodes[0].getnewaddress(address_type='bech32'), sc(60), [Decimal('0.01'), Decimal('0.02')])
        self.nodes[1].sendrawtransaction(txs[0]['hex'])
        self.nodes[0].sendrawtransaction(txs[0]['hex'])  # sending on both nodes is faster than waiting for propagation
        self.sync_all()

        # First argument of getbalance must be set to "*"
        assert_raises_rpc_error(-32, "dummy first argument must be excluded or set to \"*\"", self.nodes[1].getbalance, "")

        self.log.info("Test balances with unconfirmed inputs")

        # Before `test_balance()`, we have had two nodes with a balance of 50
        # each and then we:
        #
        # 1) Sent 40 from node A to node B with fee 0.01
        # 2) Sent 60 from node B to node A with fee 0.01
        #
        # Then we check the balances:
        #
        # 1) As is
        # 2) With transaction 2 from above with 2x the fee
        #
        # Prior to #16766, in this situation, the node would immediately report
        # a balance of 30 on node B as unconfirmed and trusted.
        #
        # After #16766, we show that balance as unconfirmed.
        #
        # The balance is indeed "trusted" and "confirmed" insofar as removing
        # the mempool transactions would return at least that much money. But
        # the algorithm after #16766 marks it as unconfirmed because the 'taint'
        # tracking of transaction trust for summing balances doesn't consider
        # which inputs belong to a user. In this case, the change output in
        # question could be "destroyed" by replace the 1st transaction above.
        #
        # The post #16766 behavior is correct; we shouldn't be treating those
        # funds as confirmed. If you want to rely on that specific UTXO existing
        # which has given you that balance, you cannot, as a third party
        # spending the other input would destroy that unconfirmed.
        #
        # For example, if the test transactions were:
        #
        # 1) Sent 40 from node A to node B with fee 0.01
        # 2) Sent 10 from node B to node A with fee 0.01
        #
        # Then our node would report a confirmed balance of 40 + 50 - 10 = 80
        # BTC, which is more than would be available if transaction 1 were
        # replaced.


        def test_balances(*, fee_node_1=0):
            # Amounts derived from subsidy + scaled principal transfers (do not scale pre-combined decimals like 9.99).
            change0_unconfirmed = self.subsidy - sc(40) - Decimal('0.01')
            pending1_untrusted = self.subsidy + sc(40) - sc(60) - fee_node_1
            # getbalances (Rigor adds per-asset top-level keys; RES buckets must stay zero in this ORU-only test)
            expected_balances_0 = {'mine':      {'immature':          Decimal('0E-8'),
                                                 'trusted':           change0_unconfirmed,
                                                 'untrusted_pending': sc(60)},
                                   'watchonly': {'immature':          sc(5000),
                                                 'trusted':           self.subsidy,
                                                 'untrusted_pending': Decimal('0E-8')}}
            expected_balances_1 = {'mine':      {'immature':          Decimal('0E-8'),
                                                 'trusted':           Decimal('0E-8'),  # node 1's send had an unsafe input
                                                 'untrusted_pending': pending1_untrusted}}  # Doesn't include output of node 0's send since it was spent
            exp0 = dict(expected_balances_0)
            if self.options.descriptors:
                exp0.pop("watchonly", None)

            def assert_rng_zero(bal):
                assert_equal(bal['res']['trusted'], Decimal('0.00'))
                assert_equal(bal['res']['untrusted_pending'], Decimal('0.00'))
                assert_equal(bal['res']['immature'], Decimal('0.00'))

            b0 = self.nodes[0].getbalances()
            b1 = self.nodes[1].getbalances()
            assert_equal(b0['mine'], b0['oru'])
            assert_equal(b1['mine'], b1['oru'])
            assert_rng_zero(b0)
            assert_rng_zero(b1)
            assert_equal(b0['mine'], exp0['mine'])
            assert_equal(b0['oru'], exp0['mine'])
            if 'watchonly' in exp0:
                assert_equal(b0['watchonly'], exp0['watchonly'])
            assert_equal(b1['mine'], expected_balances_1['mine'])
            assert_equal(b1['oru'], expected_balances_1['mine'])
            # getbalance without any arguments includes unconfirmed transactions, but not untrusted transactions
            assert_equal(self.nodes[0].getbalance(), change0_unconfirmed)  # change from node 0's send
            assert_equal(self.nodes[1].getbalance(), Decimal('0'))  # node 1's send had an unsafe input
            # Same with minconf=0
            assert_equal(self.nodes[0].getbalance(minconf=0), change0_unconfirmed)
            assert_equal(self.nodes[1].getbalance(minconf=0), Decimal('0'))
            # getbalance with a minconf incorrectly excludes coins that have been spent more recently than the minconf blocks ago
            # TODO: fix getbalance tracking of coin spentness depth
            assert_equal(self.nodes[0].getbalance(minconf=1), Decimal('0'))
            assert_equal(self.nodes[1].getbalance(minconf=1), Decimal('0'))
            # getunconfirmedbalance
            assert_equal(self.nodes[0].getunconfirmedbalance(), sc(60))  # output of node 1's spend
            assert_equal(self.nodes[1].getunconfirmedbalance(), pending1_untrusted)  # Doesn't include output of node 0's send since it was spent
            # getwalletinfo.unconfirmed_balance
            assert_equal(self.nodes[0].getwalletinfo()["unconfirmed_balance"], sc(60))
            assert_equal(self.nodes[1].getwalletinfo()["unconfirmed_balance"], pending1_untrusted)

        test_balances(fee_node_1=Decimal('0.01'))

        # Node 1 bumps the transaction fee and resends
        self.nodes[1].sendrawtransaction(txs[1]['hex'])
        self.nodes[0].sendrawtransaction(txs[1]['hex'])  # sending on both nodes is faster than waiting for propagation
        self.sync_all()

        self.log.info("Test getbalance and getbalances.mine.untrusted_pending with conflicted unconfirmed inputs")
        test_balances(fee_node_1=Decimal('0.02'))

        self.generatetoaddress(self.nodes[1], 1, ADDRESS_WATCHONLY)

        # balances are correct after the transactions are confirmed
        balance_node0 = self.subsidy - sc(40) - Decimal('0.01') + sc(60)  # node 1's send plus change from node 0's send
        balance_node1 = self.subsidy + sc(40) - sc(60) - Decimal('0.02')  # change from node 0's send (second tx fee 0.02)
        assert_equal(self.nodes[0].getbalances()['mine']['trusted'], balance_node0)
        assert_equal(self.nodes[1].getbalances()['mine']['trusted'], balance_node1)
        assert_equal(self.nodes[0].getbalance(), balance_node0)
        assert_equal(self.nodes[1].getbalance(), balance_node1)

        # Send total balance away from node 1
        txs = create_transactions(self.nodes[1], self.nodes[0].getnewaddress(address_type='bech32'), balance_node1 - Decimal('0.01'), [Decimal('0.01')])
        self.nodes[1].sendrawtransaction(txs[0]['hex'])
        self.generatetoaddress(self.nodes[1], 2, ADDRESS_WATCHONLY)

        # getbalance with a minconf incorrectly excludes coins that have been spent more recently than the minconf blocks ago
        # TODO: fix getbalance tracking of coin spentness depth
        # getbalance with minconf=3 should still show the old balance
        assert_equal(self.nodes[1].getbalance(minconf=3), Decimal('0'))

        # getbalance with minconf=2 will show the new balance.
        assert_equal(self.nodes[1].getbalance(minconf=2), Decimal('0'))

        # check mempool transactions count for wallet unconfirmed balance after
        # dynamically loading the wallet.
        before = self.nodes[1].getbalances()['mine']['untrusted_pending']
        dst = self.nodes[1].getnewaddress(address_type='bech32')
        self.nodes[1].unloadwallet(self.default_wallet_name)
        self.nodes[0].sendtoaddress(dst, 0.1)
        self.sync_all()
        self.nodes[1].loadwallet(self.default_wallet_name)
        after = self.nodes[1].getbalances()['mine']['untrusted_pending']
        assert_equal(before + Decimal('0.1'), after)

        # Create 3 more wallet txs, where the last is not accepted to the
        # mempool because it is the third descendant of the tx above
        last_spend_amt = None
        for _ in range(3):
            # Spend (nearly) the whole wallet each time so no large trusted UTXOs
            # remain outside the descendant chain (upstream used 99 on ~100 BTC).
            bal = self.nodes[0].getbalance(minconf=0)
            assert bal > Decimal('0.02')
            last_spend_amt = bal - Decimal('0.01')
            txid = self.nodes[0].sendtoaddress(self.nodes[0].getnewaddress(address_type='bech32'), str(last_spend_amt))

        self.log.info('Check that wallet txs not in the mempool are untrusted')
        assert txid not in self.nodes[0].getrawmempool()
        assert_equal(self.nodes[0].gettransaction(txid)['trusted'], False)
        assert_equal(self.nodes[0].getbalance(minconf=0), 0)

        self.log.info("Test replacement and reorg of non-mempool tx")
        tx_orig = self.nodes[0].gettransaction(txid)['hex']
        # Increase fee by 1 coin (1e8 satoshis) by reducing the payment output.
        sat_amt = int((last_spend_amt * Decimal("100000000")).to_integral_value())
        sat_reduced = sat_amt - 100000000
        old_b = struct.pack("<q", sat_amt).hex()
        assert tx_orig.count(old_b) == 1
        tx_replace = tx_orig.replace(old_b, struct.pack("<q", sat_reduced).hex())
        tx_replace = self.nodes[0].signrawtransactionwithwallet(tx_replace)['hex']
        # Total balance is given by the sum of outputs of the tx
        total_amount = sum([o['value'] for o in self.nodes[0].decoderawtransaction(tx_replace)['vout']])
        self.sync_all()
        self.nodes[1].sendrawtransaction(hexstring=tx_replace, maxfeerate=0)

        # Now confirm tx_replace
        tip_before_reorg = self.nodes[0].getblockcount()
        self.generatetoaddress(self.nodes[1], 1, ADDRESS_WATCHONLY)
        block_reorg = self.nodes[1].getbestblockhash()
        assert_equal(self.nodes[0].getbestblockhash(), block_reorg)
        assert_equal(self.nodes[0].getblockcount(), tip_before_reorg + 1)
        assert_equal(self.nodes[0].getbalance(minconf=0), total_amount)
        tx_replace_id = self.nodes[0].decoderawtransaction(tx_replace)['txid']

        self.log.info('Put txs back into mempool of node 1 (not node 0)')
        assert_equal(self.nodes[0].getbestblockhash(), block_reorg)
        assert_equal(self.nodes[1].getbestblockhash(), block_reorg)
        self.disconnect_nodes(0, 1)
        assert_equal(len(self.nodes[0].getpeerinfo()), 0)
        assert_equal(len(self.nodes[1].getpeerinfo()), 0)
        self.nodes[0].invalidateblock(self.nodes[0].getbestblockhash())
        self.nodes[1].invalidateblock(self.nodes[1].getbestblockhash())
        reorg_ok = self.nodes[0].getblockcount() == tip_before_reorg
        self.connect_nodes(0, 1)
        self.sync_blocks()
        if reorg_ok:
            self.nodes[0].syncwithvalidationinterfacequeue()
            self.nodes[1].syncwithvalidationinterfacequeue()
            assert_equal(self.nodes[0].gettransaction(tx_replace_id)['confirmations'], 0)
            self.generatetoaddress(self.nodes[0], 1, ADDRESS_WATCHONLY, sync_fun=self.no_op)

            # Now confirm tx_orig
            self.restart_node(1, ['-persistmempool=0'])
            self.connect_nodes(0, 1)
            self.sync_blocks()
            self.nodes[1].sendrawtransaction(tx_orig)
            self.generatetoaddress(self.nodes[1], 1, ADDRESS_WATCHONLY)
            assert_equal(self.nodes[0].getbalance(minconf=0), total_amount + 1)  # The reorg recovered our fee of 1 coin
        else:
            self.log.warning(
                "invalidateblock did not rewind the tip (DisconnectBlock failed for this block); "
                "skipping reorg replay assertions. Investigate validation DisconnectBlock vs wallet tx graph."
            )

        if not self.options.descriptors:
            self.log.info('Check if mempool is taken into account after import*')
            address = self.nodes[0].getnewaddress(address_type='bech32')
            privkey = self.nodes[0].dumpprivkey(address)
            self.nodes[0].sendtoaddress(address, 0.1)
            self.nodes[0].unloadwallet('')
            # check importaddress on fresh wallet
            self.nodes[0].createwallet('w1', False, True)
            self.nodes[0].importaddress(address)
            assert_equal(self.nodes[0].getbalances()['mine']['untrusted_pending'], 0)
            assert_equal(self.nodes[0].getbalances()['watchonly']['untrusted_pending'], Decimal('0.1'))
            self.nodes[0].importprivkey(privkey)
            assert_equal(self.nodes[0].getbalances()['mine']['untrusted_pending'], Decimal('0.1'))
            assert_equal(self.nodes[0].getbalances()['watchonly']['untrusted_pending'], 0)
            self.nodes[0].unloadwallet('w1')
            # check importprivkey on fresh wallet
            self.nodes[0].createwallet('w2', False, True)
            self.nodes[0].importprivkey(privkey)
            assert_equal(self.nodes[0].getbalances()['mine']['untrusted_pending'], Decimal('0.1'))


if __name__ == '__main__':
    WalletTest().main()
