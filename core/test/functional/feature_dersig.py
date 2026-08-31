#!/usr/bin/env python3
# Copyright (c) 2015-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""RIGOR spend signatures are 64-byte Ed25519.

Bitcoin BIP66 (DER SIG) delayed activation and secp256k1 P2PK do not exist.
``SCRIPT_ERR_SIG_DER`` is reused for a witness signature that is not 64 bytes.
A 64-byte value that fails Ed25519 verify is ``SCRIPT_ERR_SIG_NULLFAIL``.
"""

from test_framework.blocktools import COINBASE_MATURITY
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal
from test_framework.wallet import MiniWallet


SIG_WRONG_LENGTH = 'mandatory-script-verify-flag-failed (Non-canonical DER signature)'
SIG_BAD_VERIFY = 'mandatory-script-verify-flag-failed (Signature must be zero for failed CHECK(MULTI)SIG operation)'


class Ed25519SigEncodingTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [[
            '-whitelist=noban@127.0.0.1',
            '-par=1',
        ]]
        self.setup_clean_chain = True

    def testmempool_reject(self, tx, reason):
        assert_equal(
            [{
                'txid': tx.hash,
                'wtxid': tx.getwtxid(),
                'allowed': False,
                'reject-reason': reason,
            }],
            self.nodes[0].testmempoolaccept(rawtxs=[tx.serialize().hex()], maxfeerate=0),
        )

    def run_test(self):
        self.miniwallet = MiniWallet(self.nodes[0])
        self.generate(self.miniwallet, COINBASE_MATURITY + 1)

        self.log.info("Wrong-length Ed25519 witness signature is rejected")
        spendtx = self.miniwallet.create_self_transfer()['tx']
        spendtx.wit.vtxinwit[0].scriptWitness.stack[0] += b'\x00'
        self.testmempool_reject(spendtx, SIG_WRONG_LENGTH)

        self.log.info("64-byte witness that fails Ed25519 verify is rejected")
        spendtx = self.miniwallet.create_self_transfer()['tx']
        sig = bytearray(spendtx.wit.vtxinwit[0].scriptWitness.stack[0])
        sig[0] ^= 0x01
        spendtx.wit.vtxinwit[0].scriptWitness.stack[0] = bytes(sig)
        self.testmempool_reject(spendtx, SIG_BAD_VERIFY)

        self.log.info("A canonical 64-byte Ed25519 spend is accepted in a block")
        # Reject-path create_self_transfer marked UTXOs spent locally; they never left the chain.
        self.miniwallet.rescan_utxos()
        txid = self.miniwallet.send_self_transfer(from_node=self.nodes[0])['txid']
        assert txid in self.nodes[0].getrawmempool()
        blockhash = self.generate(self.nodes[0], 1)[0]
        assert txid not in self.nodes[0].getrawmempool()
        assert txid in self.nodes[0].getblock(blockhash)['tx']


if __name__ == '__main__':
    Ed25519SigEncodingTest().main()
