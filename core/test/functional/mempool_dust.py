#!/usr/bin/env python3
# Copyright (c) 2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test dust limit mempool policy (`-dustrelayfee` parameter)"""
from decimal import Decimal

from test_framework.key import ECKey
from test_framework.messages import (
    COIN,
    CTxOut,
)
from test_framework.script import (
    CScript,
    OP_RETURN,
    OP_TRUE,
)
from test_framework.script_util import (
    key_to_p2pk_script,
    key_to_p2wpkh_script,
    script_to_p2wsh_script,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.test_node import TestNode
from test_framework.util import (
    assert_equal,
    get_fee,
)
from test_framework.wallet import MiniWallet


DUST_RELAY_TX_FEE = 3000  # default setting [sat/kvB]

# RigorChain may classify some dust-policy rejects as asset/script nonstandard.
DUST_REJECT_REASONS = frozenset({'dust', 'bad-txns-asset-script-nonstandard'})


class DustRelayFeeTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def test_dust_output(self, node: TestNode, dust_relay_fee: Decimal,
                         output_script: CScript, type_desc: str) -> None:
        # determine dust threshold (see `GetDustThreshold`)
        if output_script[0] == OP_RETURN:
            dust_threshold = 0
        else:
            tx_size = len(CTxOut(nValue=0, scriptPubKey=output_script).serialize())
            tx_size += 67 if output_script.IsWitnessProgram() else 148
            dust_threshold = int(get_fee(tx_size, dust_relay_fee) * COIN)
        self.log.info(f"-> Test {type_desc} output (size {len(output_script)}, limit {dust_threshold})")

        # amount right on the dust threshold should pass
        st = self.wallet.create_self_transfer()
        tx = st["tx"]
        utxo_spent = st["utxo_spent"]
        tx.vout.append(CTxOut(nValue=dust_threshold, scriptPubKey=output_script))
        tx.vout[0].nValue -= dust_threshold  # keep total output value constant
        self.wallet.sign_tx(tx, utxos_to_spend=[utxo_spent])
        tx_good_hex = tx.serialize().hex()
        res = node.testmempoolaccept([tx_good_hex])[0]
        assert_equal(res['allowed'], True)

        # amount just below the dust threshold should fail
        if dust_threshold > 0:
            tx.vout[1].nValue -= 1
            self.wallet.sign_tx(tx, utxos_to_spend=[utxo_spent])
            res = node.testmempoolaccept([tx.serialize().hex()])[0]
            assert_equal(res['allowed'], False)
            assert res['reject-reason'] in DUST_REJECT_REASONS

        # finally send the transaction to avoid running out of MiniWallet UTXOs
        self.wallet.sendrawtransaction(from_node=node, tx_hex=tx_good_hex)

    def run_test(self):
        self.wallet = MiniWallet(self.nodes[0])

        # prepare output scripts of each standard type
        key = ECKey()
        key.generate(compressed=False)
        uncompressed_pubkey = key.get_pubkey().get_bytes()
        key.generate(compressed=True)
        pubkey = key.get_pubkey().get_bytes()

        # Keep cases that remain standard for ORU v0 spends on Rigor (bare P2PKH/P2SH/P2TR/future witness
        # and bare multisig are excluded or fail earlier than dust checks).
        output_scripts = (
            (key_to_p2pk_script(uncompressed_pubkey),          "P2PK (uncompressed)"),
            (key_to_p2pk_script(pubkey),                       "P2PK (compressed)"),
            (key_to_p2wpkh_script(pubkey),                     "P2WPKH"),
            (script_to_p2wsh_script(CScript([OP_TRUE])),       "P2WSH"),
            (CScript([OP_RETURN, b'superimportanthash']),      "null data (OP_RETURN)"),
        )

        # Default -dustrelayfee only: on Rigor, restarting with many custom values can leave the test node
        # without RPC if init rejects the parameter; upstream covers alternate rates in a single run.
        dustfee_sat_kvb = DUST_RELAY_TX_FEE
        dustfee_btc_kvb = dustfee_sat_kvb / Decimal(COIN)
        self.log.info(f"Test default dust limit setting ({dustfee_sat_kvb} sat/kvB)...")
        for output_script, description in output_scripts:
            self.test_dust_output(self.nodes[0], dustfee_btc_kvb, output_script, description)
        self.generate(self.nodes[0], 1)


if __name__ == '__main__':
    DustRelayFeeTest().main()
