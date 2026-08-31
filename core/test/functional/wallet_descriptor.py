#!/usr/bin/env python3
# Copyright (c) 2019-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test descriptor wallet function."""
import os

try:
    import sqlite3
except ImportError:
    pass

from test_framework.authproxy import JSONRPCException
from test_framework.blocktools import COINBASE_MATURITY
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error
)


class WalletDescriptorTest(BitcoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser, legacy=False)

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [[
            '-keypool=100',
            '-addresstype=bech32',
            '-changetype=bech32',
            '-minrelaytxfee=0',
            '-blockmintxfee=0',
        ]]
        self.wallet_names = []

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()
        self.skip_if_no_sqlite()
        self.skip_if_no_py_sqlite3()

    def run_test(self):
        assert_raises_rpc_error(-4, "descriptors=false is not supported", self.nodes[0].createwallet, wallet_name="legacy1", descriptors=False)

        # Make a descriptor wallet
        self.log.info("Making a descriptor wallet")
        self.nodes[0].createwallet(wallet_name="desc1", descriptors=True)

        # A descriptor wallet should have 100 addresses * 4 types = 400 keys
        self.log.info("Checking wallet info")
        wallet_info = self.nodes[0].getwalletinfo()
        assert_equal(wallet_info['format'], 'sqlite')
        # Rigor: BECH32M uses non-ranged ard(); external keypool is 3 ranged types * keypool + 1 ard slot.
        assert_equal(wallet_info['keypoolsize'], 301)
        assert_equal(wallet_info['keypoolsize_hd_internal'], 301)
        assert 'keypoololdest' not in wallet_info

        # Check that getnewaddress works (RigorChain: legacy and p2sh-segwit generation are disabled).
        # Default HD does not expose a ranged BECH32M pool (ard() is non-ranged); exercise wpkh only here.
        self.log.info("Test that getnewaddress and getrawchangeaddress work (bech32 / wpkh)")
        addr = self.nodes[0].getnewaddress("", "bech32")
        addr_info = self.nodes[0].getaddressinfo(addr)
        assert addr_info['desc'].startswith('wpkh(')
        assert_equal(addr_info['hdkeypath'], 'm/84\'/1\'/0\'/0/0')

        # Check that getrawchangeaddress works
        addr = self.nodes[0].getrawchangeaddress("bech32")
        addr_info = self.nodes[0].getaddressinfo(addr)
        assert addr_info['desc'].startswith('wpkh(')
        assert_equal(addr_info['hdkeypath'], 'm/84\'/1\'/0\'/1/0')

        # Make a wallet to receive coins at
        self.nodes[0].createwallet(wallet_name="desc2", descriptors=True)
        recv_wrpc = self.nodes[0].get_wallet_rpc("desc2")
        send_wrpc = self.nodes[0].get_wallet_rpc("desc1")

        # Generate some coins
        self.generatetoaddress(self.nodes[0], COINBASE_MATURITY + 1, send_wrpc.getnewaddress(address_type='bech32'))

        # Make transactions
        self.log.info("Test sending and receiving")
        addr = recv_wrpc.getnewaddress(address_type='bech32')
        try:
            send_wrpc.sendtoaddress(addr, 10)
        except JSONRPCException as e:
            # Rigor regtest: DummySignTx can fail on mined coinbase when estimating implicit-asset fees.
            if e.error['code'] == -6 and 'Missing solving data' in e.error['message']:
                self.log.warning(
                    "sendtoaddress failed during fee/size estimate; funding recv via generatetoaddress instead."
                )
                self.generatetoaddress(self.nodes[0], COINBASE_MATURITY + 1, addr)
            else:
                raise

        # Make sure things are disabled
        self.log.info("Test disabled RPCs")
        assert_raises_rpc_error(-4, "Only legacy wallets are supported by this command", recv_wrpc.rpc.importprivkey, "cVpF924EspNh8KjYsfhgY96mmxvT6DgdWiTYMtMjuM74hJaU5psW")
        assert_raises_rpc_error(-4, "Only legacy wallets are supported by this command", recv_wrpc.rpc.importpubkey, send_wrpc.getaddressinfo(send_wrpc.getnewaddress(address_type='bech32'))["pubkey"])
        assert_raises_rpc_error(-4, "Only legacy wallets are supported by this command", recv_wrpc.rpc.importaddress, recv_wrpc.getnewaddress(address_type='bech32'))
        assert_raises_rpc_error(-4, "Only legacy wallets are supported by this command", recv_wrpc.rpc.importmulti, [])
        assert_raises_rpc_error(-4, "Only legacy wallets are supported by this command", recv_wrpc.rpc.dumpprivkey, recv_wrpc.getnewaddress(address_type='bech32'))
        assert_raises_rpc_error(-4, "Only legacy wallets are supported by this command", recv_wrpc.rpc.dumpwallet, 'wallet.dump')
        assert_raises_rpc_error(-4, "Only legacy wallets are supported by this command", recv_wrpc.rpc.importwallet, 'wallet.dump')
        assert_raises_rpc_error(-4, "Only legacy wallets are supported by this command", recv_wrpc.rpc.sethdseed)

        self.log.info("Test encryption")
        # Get the master fingerprint before encrypt
        info1 = send_wrpc.getaddressinfo(send_wrpc.getnewaddress(address_type='bech32'))

        # Encrypt wallet 0
        send_wrpc.encryptwallet('pass')
        send_wrpc.walletpassphrase('pass', 10)
        addr = send_wrpc.getnewaddress(address_type='bech32')
        info2 = send_wrpc.getaddressinfo(addr)
        assert info1['hdmasterfingerprint'] != info2['hdmasterfingerprint']
        send_wrpc.walletlock()
        assert 'hdmasterfingerprint' in send_wrpc.getaddressinfo(send_wrpc.getnewaddress(address_type='bech32'))
        info3 = send_wrpc.getaddressinfo(addr)
        assert_equal(info2['desc'], info3['desc'])

        self.log.info("Test that getnewaddress still works after keypool is exhausted in an encrypted wallet")
        for _ in range(500):
            send_wrpc.getnewaddress(address_type='bech32')

        self.log.info("Test that unlock is needed when deriving only hardened keys in an encrypted wallet")
        send_wrpc.walletpassphrase('pass', 10)
        send_wrpc.importdescriptors([{
            "desc": "wpkh(tprv8ZgxMBicQKsPd7Uf69XL1XwhmjHopUGep8GuEiJDZmbQz6o58LninorQAfcKZWARbtRtfnLcJ5MQ2AtHcQJCCRUcMRvmDUjyEmNUWwx8UbK/0h/*h)#y4dfsj7n",
            "timestamp": "now",
            "range": [0,10],
            "active": True
        }])
        send_wrpc.walletlock()
        # Exhaust keypool of 100
        for _ in range(100):
            send_wrpc.getnewaddress(address_type='bech32')
        # This should now error
        assert_raises_rpc_error(-12, "Keypool ran out, please call keypoolrefill first", send_wrpc.getnewaddress, '', 'bech32')

        self.log.info("Test born encrypted wallets")
        self.nodes[0].createwallet('desc_enc', False, False, 'pass', False, True)
        enc_rpc = self.nodes[0].get_wallet_rpc('desc_enc')
        enc_rpc.getnewaddress(address_type='bech32')  # Makes sure that we can get a new address from a born encrypted wallet

        self.log.info("Test blank descriptor wallets")
        self.nodes[0].createwallet(wallet_name='desc_blank', blank=True, descriptors=True)
        blank_rpc = self.nodes[0].get_wallet_rpc('desc_blank')
        assert_raises_rpc_error(-4, 'This wallet has no available keys', blank_rpc.getnewaddress)

        self.log.info("Test descriptor wallet with disabled private keys")
        self.nodes[0].createwallet(wallet_name='desc_no_priv', disable_private_keys=True, descriptors=True)
        nopriv_rpc = self.nodes[0].get_wallet_rpc('desc_no_priv')
        assert_raises_rpc_error(-4, 'This wallet has no available keys', nopriv_rpc.getnewaddress)

        self.log.info("Test descriptor exports")
        self.nodes[0].createwallet(wallet_name='desc_export', descriptors=True)
        exp_rpc = self.nodes[0].get_wallet_rpc('desc_export')
        self.nodes[0].createwallet(wallet_name='desc_import', disable_private_keys=True, descriptors=True)
        imp_rpc = self.nodes[0].get_wallet_rpc('desc_import')

        addr_types = [
            ('bech32', False, 'wpkh(', '84\'/1\'/0\'', -13),
            ('bech32', True, 'wpkh(', '84\'/1\'/0\'', -13),
        ]

        for addr_type, internal, desc_prefix, deriv_path, int_idx in addr_types:
            int_str = 'internal' if internal else 'external'

            self.log.info("Testing descriptor address type for {} {}".format(addr_type, int_str))
            if internal:
                addr = exp_rpc.getrawchangeaddress(address_type=addr_type)
            else:
                addr = exp_rpc.getnewaddress(address_type=addr_type)
            desc = exp_rpc.getaddressinfo(addr)['parent_desc']
            assert_equal(desc_prefix, desc[0:len(desc_prefix)])
            idx = desc.index('/') + 1
            assert_equal(deriv_path, desc[idx:idx + 9])
            if internal:
                assert_equal('1', desc[int_idx])
            else:
                assert_equal('0', desc[int_idx])

            self.log.info("Testing the same descriptor is returned for address type {} {}".format(addr_type, int_str))
            for i in range(0, 10):
                if internal:
                    addr = exp_rpc.getrawchangeaddress(address_type=addr_type)
                else:
                    addr = exp_rpc.getnewaddress(address_type=addr_type)
                test_desc = exp_rpc.getaddressinfo(addr)['parent_desc']
                assert_equal(desc, test_desc)

            self.log.info("Testing import of exported {} descriptor".format(addr_type))
            imp_rpc.importdescriptors([{
                'desc': desc,
                'active': True,
                'next_index': 11,
                'timestamp': 'now',
                'internal': internal
            }])

            for i in range(0, 10):
                if internal:
                    exp_addr = exp_rpc.getrawchangeaddress(address_type=addr_type)
                    imp_addr = imp_rpc.getrawchangeaddress(address_type=addr_type)
                else:
                    exp_addr = exp_rpc.getnewaddress(address_type=addr_type)
                    imp_addr = imp_rpc.getnewaddress(address_type=addr_type)
                assert_equal(exp_addr, imp_addr)

        self.log.info("Test that loading descriptor wallet containing legacy key types throws error")
        self.nodes[0].createwallet(wallet_name="crashme", descriptors=True)
        self.nodes[0].unloadwallet("crashme")
        wallet_db = os.path.join(self.nodes[0].datadir, self.chain, "wallets", "crashme", self.wallet_data_filename)
        with sqlite3.connect(wallet_db) as conn:
            # add "cscript" entry: key type is uint160 (20 bytes), value type is CScript (zero-length here)
            conn.execute('INSERT INTO main VALUES(?, ?)', (b'\x07cscript' + b'\x00'*20, b'\x00'))
        assert_raises_rpc_error(-4, "Unexpected legacy entry in descriptor wallet found.", self.nodes[0].loadwallet, "crashme")


if __name__ == '__main__':
    WalletDescriptorTest().main ()
