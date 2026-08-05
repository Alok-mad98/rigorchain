#!/usr/bin/env python3
# Copyright (c) 2014-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the listdescriptors RPC."""

from test_framework.blocktools import (
    TIME_GENESIS_BLOCK,
)
from test_framework.descriptors import (
    descsum_create,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)


class ListDescriptorsTest(BitcoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser, legacy=False)

    def set_test_params(self):
        self.num_nodes = 1

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()
        self.skip_if_no_sqlite()

    # do not create any wallet by default
    def init_wallet(self, *, node):
        return

    def run_test(self):
        node = self.nodes[0]
        assert_raises_rpc_error(-18, 'No wallet is loaded.', node.listdescriptors)

        self.log.info('Non-descriptor wallet creation is rejected; listdescriptors requires a descriptor wallet.')
        assert_raises_rpc_error(-4, "descriptors=false is not supported", node.createwallet, wallet_name='w1', descriptors=False)
        self.log.info('Test the command for empty descriptors wallet.')
        node.createwallet(wallet_name='w2', blank=True, descriptors=True)
        assert_equal(0, len(node.get_wallet_rpc('w2').listdescriptors()['descriptors']))

        self.log.info('Test the command for a default descriptors wallet.')
        node.createwallet(wallet_name='w3', descriptors=True)
        result = node.get_wallet_rpc('w3').listdescriptors()
        assert_equal("w3", result['wallet_name'])
        assert_equal(8, len(result['descriptors']))
        assert_equal(8, len([d for d in result['descriptors'] if d['active']]))
        assert_equal(4, len([d for d in result['descriptors'] if d['internal']]))
        for item in result['descriptors']:
            assert item['desc'] != ''
            # next_index / range are only present for ranged descriptors (e.g. wpkh); ard() entries omit them.
            if 'next_index' in item:
                assert item['next_index'] == 0
            if 'range' in item:
                assert_equal(len(item['range']), 2)
            assert item['timestamp'] is not None

        self.log.info('Test that descriptor strings are returned in lexicographically sorted order.')
        descriptor_strings = [descriptor['desc'] for descriptor in result['descriptors']]
        assert_equal(descriptor_strings, sorted(descriptor_strings))

        self.log.info('Test descriptors with hardened derivations are listed in importable form.')
        xprv = 'tprv8ZgxMBicQKsPeuVhWwi6wuMQGfPKi9Li5GtX35jVNknACgqe3CY4g5xgkfDDJcmtF7o1QnxWDRYw4H5P26PXq7sbcUkEqeR4fg3Kxp2tigg'
        xpub_acc = 'tpubDCMVLhErorrAGfApiJSJzEKwqeaf2z3NrkVMxgYQjZLzMjXMBeRw2muGNYbvaekAE8rUFLftyEar4LdrG2wXyyTJQZ26zptmeTEjPTaATts'
        hardened_path = '/84\'/1\'/0\''
        wallet = node.get_wallet_rpc('w2')
        wallet.importdescriptors([{
            'desc': descsum_create('wpkh(' + xprv + hardened_path + '/0/*)'),
            'timestamp': TIME_GENESIS_BLOCK,
        }])
        pub0 = wallet.listdescriptors()
        assert_equal('w2', pub0['wallet_name'])
        assert_equal(1, len(pub0['descriptors']))
        pe = pub0['descriptors'][0]
        assert_equal(TIME_GENESIS_BLOCK, pe['timestamp'])
        assert_equal(False, pe['active'])
        assert_equal([0, 0], pe['range'])
        assert_equal(0, pe['next'])
        assert_equal(0, pe['next_index'])
        assert pe['desc'].startswith('wpkh(')
        assert "0/*)" in pe['desc'] and "84" in pe['desc']
        assert_equal(pub0, wallet.listdescriptors(False))

        self.log.info('Test list private descriptors')
        priv0 = wallet.listdescriptors(True)
        assert_equal('w2', priv0['wallet_name'])
        assert_equal(1, len(priv0['descriptors']))
        pde = priv0['descriptors'][0]
        assert_equal(TIME_GENESIS_BLOCK, pde['timestamp'])
        assert_equal(False, pde['active'])
        assert_equal([0, 0], pde['range'])
        assert_equal(0, pde['next'])
        assert_equal(0, pde['next_index'])
        assert xprv[:8] in pde['desc']

        self.log.info("Test listdescriptors with encrypted wallet")
        wallet.encryptwallet("pass")
        assert_equal(pub0, wallet.listdescriptors())

        self.log.info('Test list private descriptors with encrypted wallet')
        assert_raises_rpc_error(-13, 'Please enter the wallet passphrase with walletpassphrase first.', wallet.listdescriptors, True)
        wallet.walletpassphrase(passphrase="pass", timeout=1000000)
        assert_equal(priv0, wallet.listdescriptors(True))

        self.log.info('Test list private descriptors with watch-only wallet')
        node.createwallet(wallet_name='watch-only', descriptors=True, disable_private_keys=True)
        watch_only_wallet = node.get_wallet_rpc('watch-only')
        watch_only_wallet.importdescriptors([{
            'desc': descsum_create('wpkh(' + xpub_acc + ')'),
            'timestamp': TIME_GENESIS_BLOCK,
        }])
        assert_raises_rpc_error(-4, 'Can\'t get descriptor string', watch_only_wallet.listdescriptors, True)

        self.log.info('Test non-active non-range combo descriptor')
        node.createwallet(wallet_name='w4', blank=True, descriptors=True)
        wallet = node.get_wallet_rpc('w4')
        wallet.importdescriptors([{
            'desc': descsum_create('combo(' + node.get_deterministic_priv_key().key + ')'),
            'timestamp': TIME_GENESIS_BLOCK,
        }])
        combo_list = wallet.listdescriptors()
        assert_equal('w4', combo_list['wallet_name'])
        assert_equal(1, len(combo_list['descriptors']))
        ce = combo_list['descriptors'][0]
        assert_equal(False, ce['active'])
        assert_equal(TIME_GENESIS_BLOCK, ce['timestamp'])
        assert ce['desc'].startswith('combo(')


if __name__ == '__main__':
    ListDescriptorsTest().main()
