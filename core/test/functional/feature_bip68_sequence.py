#!/usr/bin/env python3
# Copyright (c) 2014-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test BIP68 nSequence relative lock times.

CSV is genesis-active on RIGOR (CSVHeight = 1). There is no delayed-activation
window and no P2WSH anyone-can-spend fixture: spends are Ed25519 P2WPKH.
BIP68 still applies only to nVersion >= 2, or when the disable flag is unset.
"""

from decimal import Decimal
import random
import time

from test_framework.blocktools import (
    NORMAL_GBT_REQUEST_PARAMS,
    add_witness_commitment,
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
from test_framework.script import CScript
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)
from test_framework.wallet import MiniWallet

SEQUENCE_LOCKTIME_DISABLE_FLAG = (1 << 31)
SEQUENCE_LOCKTIME_TYPE_FLAG = (1 << 22)  # time (0 means height)
SEQUENCE_LOCKTIME_GRANULARITY = 9
SEQUENCE_LOCKTIME_MASK = 0x0000ffff

NOT_FINAL_ERROR = "non-BIP68-final"


def _utxo_from_tx(tx, vout=0):
    tx.rehash()
    return {
        "txid": tx.hash,
        "vout": vout,
        "value": Decimal(tx.vout[vout].nValue) / COIN,
    }


class BIP68Test(BitcoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)

    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [[]]

    def run_test(self):
        self.relayfee = self.nodes[0].getnetworkinfo()["relayfee"]
        self.wallet = MiniWallet(self.nodes[0])
        self.spk = CScript(self.wallet.get_scriptPubKey())

        self.log.info("Running test disable flag")
        self.test_disable_flag()

        self.log.info("Running test sequence-lock-confirmed-inputs")
        self.test_sequence_lock_confirmed_inputs()

        self.log.info("Running test sequence-lock-unconfirmed-inputs")
        self.test_sequence_lock_unconfirmed_inputs()

        self.log.info("BIP68 is consensus (no pre-activation exception)")
        self.test_bip68_is_consensus()

        self.log.info("Verifying nVersion=2 transactions are standard")
        self.test_version2_relay()

        self.log.info("Passed")

    def spend_to_self(self, utxo, *, sequence, nversion=2):
        """Spend a MiniWallet P2WPKH coin back to MiniWallet with the given nSequence."""
        tx = CTransaction()
        tx.nVersion = nversion
        tx.vin = [CTxIn(COutPoint(int(utxo["txid"], 16), utxo["vout"]), nSequence=sequence)]
        value = int((utxo["value"] - self.relayfee) * COIN)
        tx.vout = [CTxOut(value, self.spk)]
        self.wallet.sign_tx(tx, utxos_to_spend=[utxo])
        return tx

    def test_disable_flag(self):
        utxo = self.wallet.send_self_transfer(from_node=self.nodes[0])["new_utxo"]

        sequence_value = SEQUENCE_LOCKTIME_DISABLE_FLAG | 1
        tx1 = self.spend_to_self(utxo, sequence=sequence_value)
        self.wallet.sendrawtransaction(from_node=self.nodes[0], tx_hex=tx1.serialize().hex())
        tx1_utxo = _utxo_from_tx(tx1)

        # nVersion=2 + relative lock of 1 confirmation: not final while tx1 is unconfirmed.
        tx2 = self.spend_to_self(tx1_utxo, sequence=1, nversion=2)
        assert_raises_rpc_error(-26, NOT_FINAL_ERROR, self.wallet.sendrawtransaction, from_node=self.nodes[0], tx_hex=tx2.serialize().hex())

        # nVersion=1 disables BIP68; must re-sign because the sighash commits to nVersion.
        tx2 = self.spend_to_self(tx1_utxo, sequence=1, nversion=1)
        self.wallet.sendrawtransaction(from_node=self.nodes[0], tx_hex=tx2.serialize().hex())

    def get_median_time_past(self, confirmations):
        block_hash = self.nodes[0].getblockhash(self.nodes[0].getblockcount() - confirmations)
        return self.nodes[0].getblockheader(block_hash)["mediantime"]

    def test_sequence_lock_confirmed_inputs(self):
        max_outputs = 50
        while len(self.wallet.get_utxos(include_immature_coinbase=False, mark_as_spent=False)) < 200:
            num_outputs = random.randint(1, max_outputs)
            self.wallet.send_self_transfer_multi(from_node=self.nodes[0], num_outputs=num_outputs)
            self.generate(self.wallet, 1)

        utxos = self.wallet.get_utxos(include_immature_coinbase=False)

        for _ in range(400):
            num_inputs = random.randint(1, 10)
            random.shuffle(utxos)
            spent = utxos[:num_inputs]

            should_pass = True
            using_sequence_locks = False

            tx = CTransaction()
            tx.nVersion = 2
            value = 0
            for j in range(num_inputs):
                sequence_value = 0xfffffffe

                if random.randint(0, 1):
                    using_sequence_locks = True
                    input_will_pass = (random.randint(1, 10) == 1)
                    sequence_value = spent[j]["confirmations"]
                    if not input_will_pass:
                        sequence_value += 1
                        should_pass = False

                    orig_time = self.get_median_time_past(spent[j]["confirmations"])
                    cur_time = self.get_median_time_past(0)

                    can_time_lock = True
                    if ((cur_time - orig_time) >> SEQUENCE_LOCKTIME_GRANULARITY) >= SEQUENCE_LOCKTIME_MASK:
                        can_time_lock = False

                    if random.randint(0, 1) and can_time_lock:
                        time_delta = sequence_value << SEQUENCE_LOCKTIME_GRANULARITY
                        if input_will_pass and time_delta > cur_time - orig_time:
                            sequence_value = ((cur_time - orig_time) >> SEQUENCE_LOCKTIME_GRANULARITY)
                        elif (not input_will_pass and time_delta <= cur_time - orig_time):
                            sequence_value = ((cur_time - orig_time) >> SEQUENCE_LOCKTIME_GRANULARITY) + 1
                        sequence_value |= SEQUENCE_LOCKTIME_TYPE_FLAG
                tx.vin.append(CTxIn(COutPoint(int(spent[j]["txid"], 16), spent[j]["vout"]), nSequence=sequence_value))
                value += spent[j]["value"] * COIN
            tx_size = 50 + 183 * num_inputs
            tx.vout.append(CTxOut(int(value - self.relayfee * tx_size * COIN / 1000), self.spk))
            self.wallet.sign_tx(tx, utxos_to_spend=spent)

            if using_sequence_locks and not should_pass:
                assert_raises_rpc_error(-26, NOT_FINAL_ERROR, self.wallet.sendrawtransaction, from_node=self.nodes[0], tx_hex=tx.serialize().hex())
            else:
                self.wallet.sendrawtransaction(from_node=self.nodes[0], tx_hex=tx.serialize().hex())
                self.wallet.rescan_utxos()
                utxos = self.wallet.get_utxos(include_immature_coinbase=False)

    def test_sequence_lock_unconfirmed_inputs(self):
        cur_height = self.nodes[0].getblockcount()

        self.wallet.rescan_utxos()
        tx1 = self.wallet.send_self_transfer(from_node=self.nodes[0])["tx"]
        tx1.rehash()

        tx2 = self.spend_to_self(_utxo_from_tx(tx1), sequence=0, nversion=2)
        tx2_raw = tx2.serialize().hex()
        tx2.rehash()
        self.wallet.sendrawtransaction(from_node=self.nodes[0], tx_hex=tx2_raw)

        def test_nonzero_locks(orig_tx, node, use_height_lock):
            sequence_value = 1
            if not use_height_lock:
                sequence_value |= SEQUENCE_LOCKTIME_TYPE_FLAG
            tx = self.spend_to_self(_utxo_from_tx(orig_tx), sequence=sequence_value, nversion=2)

            if orig_tx.hash in node.getrawmempool():
                assert_raises_rpc_error(-26, NOT_FINAL_ERROR, self.wallet.sendrawtransaction, from_node=node, tx_hex=tx.serialize().hex())
            else:
                self.wallet.sendrawtransaction(from_node=node, tx_hex=tx.serialize().hex())
            return tx

        test_nonzero_locks(tx2, self.nodes[0], use_height_lock=True)
        test_nonzero_locks(tx2, self.nodes[0], use_height_lock=False)

        self.nodes[0].prioritisetransaction(txid=tx2.hash, fee_delta=int(-self.relayfee * COIN))
        cur_time = int(time.time())
        for _ in range(10):
            self.nodes[0].setmocktime(cur_time + 600)
            self.generate(self.wallet, 1, sync_fun=self.no_op)
            cur_time += 600

        assert tx2.hash in self.nodes[0].getrawmempool()

        test_nonzero_locks(tx2, self.nodes[0], use_height_lock=True)
        test_nonzero_locks(tx2, self.nodes[0], use_height_lock=False)

        self.nodes[0].prioritisetransaction(txid=tx2.hash, fee_delta=int(self.relayfee * COIN))

        self.nodes[0].setmocktime(cur_time + 600)
        tmpl = self.nodes[0].getblocktemplate(NORMAL_GBT_REQUEST_PARAMS)
        self.generate(self.nodes[0], 1)
        assert tx2.hash not in self.nodes[0].getrawmempool()

        tx3 = test_nonzero_locks(tx2, self.nodes[0], use_height_lock=False)
        assert tx3.hash in self.nodes[0].getrawmempool()

        self.generate(self.nodes[0], 1)
        assert tx3.hash not in self.nodes[0].getrawmempool()

        tx4 = test_nonzero_locks(tx3, self.nodes[0], use_height_lock=True)
        assert tx4.hash in self.nodes[0].getrawmempool()

        tx5 = test_nonzero_locks(tx4, self.nodes[0], use_height_lock=True)
        assert tx5.hash not in self.nodes[0].getrawmempool()

        confirmed = [
            u for u in self.wallet.get_utxos(mark_as_spent=False)
            if u["txid"] != tx4.hash and u.get("confirmations", 0) > 0
        ]
        assert confirmed
        utxo = self.wallet.get_utxo(txid=confirmed[0]["txid"], vout=confirmed[0]["vout"])
        tx5.vin.append(CTxIn(COutPoint(int(utxo["txid"], 16), utxo["vout"]), nSequence=1))
        tx5.vout[0].nValue += int(utxo["value"] * COIN)
        self.wallet.sign_tx(tx5, utxos_to_spend=[_utxo_from_tx(tx4), utxo])

        assert_raises_rpc_error(-26, NOT_FINAL_ERROR, self.wallet.sendrawtransaction, from_node=self.nodes[0], tx_hex=tx5.serialize().hex())

        self.nodes[0].invalidateblock(self.nodes[0].getbestblockhash())
        assert tx4.hash not in self.nodes[0].getrawmempool()
        assert tx3.hash in self.nodes[0].getrawmempool()

        for i in range(2):
            block = create_block(
                tmpl=tmpl,
                ntime=cur_time,
                coinbase=create_coinbase(tmpl["height"], script_pubkey=self.spk),
            )
            add_witness_commitment(block)
            block.solve()
            tip = block.sha256
            assert_equal(None if i == 1 else "inconclusive", self.nodes[0].submitblock(block.serialize().hex()))
            tmpl = self.nodes[0].getblocktemplate(NORMAL_GBT_REQUEST_PARAMS)
            tmpl["previousblockhash"] = "%x" % tip
            tmpl["transactions"] = []
            cur_time += 1

        mempool = self.nodes[0].getrawmempool()
        assert tx3.hash not in mempool
        assert tx2.hash in mempool

        self.nodes[0].setmocktime(0)
        self.nodes[0].invalidateblock(self.nodes[0].getblockhash(cur_height + 1))
        self.generate(self.wallet, 10, sync_fun=self.no_op)

    def test_bip68_is_consensus(self):
        """A v2 spend with a 100-block relative lock cannot be mined after one confirmation."""
        parent = self.wallet.send_self_transfer(from_node=self.nodes[0])
        self.generate(self.nodes[0], 1)
        tx3 = self.spend_to_self(parent["new_utxo"], sequence=100, nversion=2)
        assert_raises_rpc_error(-26, NOT_FINAL_ERROR, self.wallet.sendrawtransaction, from_node=self.nodes[0], tx_hex=tx3.serialize().hex())
        assert_raises_rpc_error(
            -25,
            "TestBlockValidity failed",
            self.generateblock,
            self.nodes[0],
            self.wallet.get_address(),
            [tx3.serialize().hex()],
        )

    def test_version2_relay(self):
        mini_wallet = MiniWallet(self.nodes[0])
        mini_wallet.rescan_utxos()
        st = mini_wallet.create_self_transfer()
        tx = st["tx"]
        tx.nVersion = 2
        mini_wallet.sign_tx(tx, utxos_to_spend=[st["utxo_spent"]])
        mini_wallet.sendrawtransaction(from_node=self.nodes[0], tx_hex=tx.serialize().hex())


if __name__ == "__main__":
    BIP68Test().main()
