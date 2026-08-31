#!/usr/bin/env python3
# Copyright (c) 2019-2022 The Bitcoin Core developers
# Copyright (c) 2026 The RIGOR developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test importdescriptors for RIGOR locks.

Bitcoin pkh/sh/wsh/tr/combo/multi descriptors are consensus-invalid here.
Live coverage is wpkh() (ORU) and res() (RES), plus the generic RPC guards
(checksum, range, labels, private-keys-disabled).
"""

import os

from test_framework.blocktools import COINBASE_MATURITY
from test_framework.descriptors import descsum_create
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error
from test_framework.wallet_util import bytes_to_wif, test_address


class ImportDescriptorsTest(BitcoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser, legacy=False)

    def set_test_params(self):
        self.num_nodes = 2
        self.extra_args = [["-addresstype=bech32"],
                           ["-addresstype=bech32", "-keypool=5"]]
        for args in self.extra_args:
            args.append("-whitelist=noban@127.0.0.1")
        self.setup_clean_chain = True
        self.wallet_names = []

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()
        self.skip_if_no_sqlite()

    def test_importdesc(self, req, success, error_code=None, error_message=None, warnings=None, wallet=None):
        """Run importdescriptors and assert success / error payload."""
        if warnings is None:
            warnings = []
        wrpc = self.nodes[1].get_wallet_rpc('w1')
        if wallet is not None:
            wrpc = wallet

        result = wrpc.importdescriptors([req])
        observed_warnings = []
        if 'warnings' in result[0]:
            observed_warnings = result[0]['warnings']
        assert_equal("\n".join(sorted(warnings)), "\n".join(sorted(observed_warnings)))
        assert_equal(result[0]['success'], success)
        if error_code is not None:
            assert_equal(result[0]['error']['code'], error_code)
            assert_equal(result[0]['error']['message'], error_message)

    def run_test(self):
        self.log.info('Setting up wallets')
        self.nodes[0].createwallet(wallet_name='w0', disable_private_keys=False, descriptors=True)
        w0 = self.nodes[0].get_wallet_rpc('w0')

        self.nodes[1].createwallet(wallet_name='w1', disable_private_keys=True, blank=True, descriptors=True)
        w1 = self.nodes[1].get_wallet_rpc('w1')
        assert_equal(w1.getwalletinfo()['keypoolsize'], 0)

        self.nodes[1].createwallet(wallet_name="wpriv", disable_private_keys=False, blank=True, descriptors=True)
        wpriv = self.nodes[1].get_wallet_rpc("wpriv")
        assert_equal(wpriv.getwalletinfo()['keypoolsize'], 0)

        self.log.info('Mining ORU coinbases to bech32 (not bech32m/RES)')
        self.generatetoaddress(self.nodes[0], COINBASE_MATURITY + 1, w0.getnewaddress(address_type='bech32'))

        self.log.info("Import should fail if a descriptor is not provided")
        self.test_importdesc({"timestamp": "now"},
                             success=False,
                             error_code=-8,
                             error_message='Descriptor not found.')

        secp_pub = "03789ed0bb717d88f7d321a368d905e7430207ebbd82bd342cf11ae157a7ace5fd"
        self.log.info("Bitcoin leftover descriptors are rejected")
        for expr, message in (
            (f"pk({secp_pub})", "pk() is not supported: RIGOR consensus allows only wpkh() (ORU) and res() (RES)"),
            (f"pkh({secp_pub})", "pkh() is not supported: RIGOR consensus allows only wpkh() (ORU) and res() (RES)"),
            (f"combo({secp_pub})", "combo() is not supported: RIGOR consensus allows only wpkh() (ORU) and res() (RES)"),
            (f"sh(wpkh({secp_pub}))", "sh() is not supported: RIGOR consensus does not permit P2SH"),
            (f"wsh(multi(1,{secp_pub}))", "wsh() is not supported: RIGOR consensus does not permit P2WSH script-hash locks"),
            (f"multi(1,{secp_pub})", "multi() is not supported: RIGOR consensus allows only wpkh() (ORU) and res() (RES)"),
            (f"tr({secp_pub})", "tr() is not supported on RigorChain; use res() for witness v1 RES (Ed25519), not Bitcoin Taproot"),
        ):
            self.test_importdesc({"desc": descsum_create(expr), "timestamp": "now"},
                                 success=False,
                                 error_code=-5,
                                 error_message=message)

        oru_addr = w0.getnewaddress(address_type='bech32')
        oru_info = w0.getaddressinfo(oru_addr)
        assert oru_info['desc'].startswith('wpkh(')

        self.log.info("Should not import a wpkh descriptor without checksum")
        inner = oru_info['desc'].split('#')[0]
        self.test_importdesc({"desc": inner, "timestamp": "now"},
                             success=False,
                             error_code=-5,
                             error_message="Missing checksum")

        self.log.info("Range should not be specified for an un-ranged wpkh")
        self.test_importdesc({"desc": oru_info['desc'], "timestamp": "now", "range": 1},
                             success=False,
                             error_code=-8,
                             error_message="Range should not be specified for an un-ranged descriptor")

        self.log.info("Active unranged wpkh must be rejected")
        self.test_importdesc({"desc": oru_info['desc'], "timestamp": "now", "active": True},
                             success=False,
                             error_code=-8,
                             error_message="Active descriptors must be ranged")

        self.log.info("Should import a wpkh descriptor (watch-only)")
        import_request = {"desc": oru_info['desc'],
                          "timestamp": "now",
                          "label": "Descriptor import test"}
        self.test_importdesc(import_request, success=True)
        test_address(w1,
                     oru_addr,
                     solvable=True,
                     ismine=True,
                     labels=["Descriptor import test"])
        assert_equal(w1.getwalletinfo()['keypoolsize'], 0)

        self.log.info("Test can import same descriptor with public key twice")
        self.test_importdesc(import_request, success=True)

        self.log.info("Test can update descriptor label")
        self.test_importdesc({**import_request, "label": "Updated label"}, success=True)
        test_address(w1, oru_addr, solvable=True, ismine=True, labels=["Updated label"])

        self.log.info("Internal addresses cannot have labels")
        self.test_importdesc({**import_request, "internal": True},
                             success=False,
                             error_code=-8,
                             error_message="Internal addresses should not have a label")

        self.log.info("Unranged res() may be imported active")
        res_addr = w0.getnewaddress(address_type='bech32m')
        res_info = w0.getaddressinfo(res_addr)
        assert "res(" in res_info['desc']
        self.test_importdesc({"desc": res_info['desc'], "timestamp": "now", "active": True},
                             success=True)
        test_address(w1, res_addr, solvable=True, ismine=True)

        self.log.info("Watch-only wallet must reject a wpkh that carries a private key")
        wif = bytes_to_wif(os.urandom(32))
        priv_desc = descsum_create(f"wpkh({wif})")
        self.test_importdesc({"desc": priv_desc, "timestamp": "now"},
                             success=False,
                             error_code=-4,
                             error_message="Cannot import private keys to a wallet with private keys disabled")

        self.log.info("Private wallet can import a ranged wpkh xprv and derive an ORU address")
        xprvs = w0.listdescriptors(True)['descriptors']
        priv_wpkh = next(d['desc'] for d in xprvs
                         if d['desc'].startswith('wpkh(') and not d.get('internal'))
        priv_wpkh_internal = next(d['desc'] for d in xprvs
                                  if d['desc'].startswith('wpkh(') and d.get('internal'))
        self.test_importdesc({"desc": priv_wpkh, "timestamp": "now", "active": True, "range": [0, 20]},
                             success=True,
                             wallet=wpriv)
        recv = wpriv.getnewaddress(address_type='bech32')
        test_address(wpriv, recv, solvable=True, ismine=True, iswatchonly=False)
        assert wpriv.getaddressinfo(recv)['desc'].startswith('wpkh(')

        self.test_ranged_range_argument(priv_wpkh)
        self.test_keypool_topup(priv_wpkh, priv_wpkh_internal)
        self.test_active_conflict(priv_wpkh, priv_wpkh_internal)
        self.test_encrypted_wallet(priv_wpkh)
        self.test_non_descriptor_wallet_rejected()

    def test_ranged_range_argument(self, priv_wpkh):
        """A ranged descriptor's range/next_index arguments are the wallet's derivation contract."""
        self.log.info("Ranged wpkh: range, next_index and label handling")
        self.nodes[1].createwallet(wallet_name='wrange', blank=True, descriptors=True)
        wrange = self.nodes[1].get_wallet_rpc('wrange')

        for req, message in (
            ({"range": [10, 5]}, "Range specified as [begin,end] must not have begin after end"),
            ({"range": [-1, 5]}, "Range should be greater or equal than 0"),
            ({"range": [0, 10], "next_index": 11}, "next_index is out of range"),
            ({"range": [0, 10], "label": "ranged"}, "Ranged descriptors should not have a label"),
        ):
            self.test_importdesc({"desc": priv_wpkh, "timestamp": "now", **req},
                                 success=False,
                                 error_code=-8,
                                 error_message=message,
                                 wallet=wrange)

        # Omitting the range is legal but derives only -keypool entries, so it warns rather than
        # silently producing a wallet that stops recognising its own addresses at index 5.
        self.test_importdesc({"desc": priv_wpkh, "timestamp": "now"},
                             success=True,
                             warnings=["Range not given, using default keypool range"],
                             wallet=wrange)
        wrange.unloadwallet()

        self.log.info("next_index selects the first address a ranged descriptor hands out")
        self.nodes[1].createwallet(wallet_name='wnext', blank=True, descriptors=True)
        wnext = self.nodes[1].get_wallet_rpc('wnext')
        self.test_importdesc({"desc": priv_wpkh, "timestamp": "now", "active": True,
                              "range": [0, 10], "next_index": 4},
                             success=True,
                             wallet=wnext)
        assert_equal(self.active_descriptor(wnext, internal=False)['next_index'], 4)
        wnext.getnewaddress(address_type='bech32')
        assert_equal(self.active_descriptor(wnext, internal=False)['next_index'], 5)
        wnext.unloadwallet()

    def test_keypool_topup(self, priv_wpkh, priv_wpkh_internal):
        """Importing active ranged descriptors is what fills the keypool; -keypool does not."""
        self.log.info("Ranged wpkh import tops up the external and internal keypools")
        self.nodes[1].createwallet(wallet_name='wpool', blank=True, descriptors=True)
        wpool = self.nodes[1].get_wallet_rpc('wpool')
        assert_equal(wpool.getwalletinfo()['keypoolsize'], 0)
        assert_equal(wpool.getwalletinfo()['keypoolsize_hd_internal'], 0)

        self.test_importdesc({"desc": priv_wpkh, "timestamp": "now", "active": True, "range": [0, 24]},
                             success=True, wallet=wpool)
        self.assert_keypool_matches_range(wpool)
        assert_equal(wpool.getwalletinfo()['keypoolsize_hd_internal'], 0)

        self.test_importdesc({"desc": priv_wpkh_internal, "timestamp": "now", "active": True,
                              "internal": True, "range": [0, 9]},
                             success=True, wallet=wpool)
        self.assert_keypool_matches_range(wpool)

        # Handing out an address consumes from the range rather than extending it.
        before = wpool.getwalletinfo()
        before_next = self.active_descriptor(wpool, internal=False)['next_index']
        wpool.getnewaddress(address_type='bech32')
        wpool.getrawchangeaddress(address_type='bech32')
        after = wpool.getwalletinfo()
        assert_equal(after['keypoolsize'], before['keypoolsize'] - 1)
        assert_equal(after['keypoolsize_hd_internal'], before['keypoolsize_hd_internal'] - 1)
        assert_equal(self.active_descriptor(wpool, internal=False)['next_index'], before_next + 1)
        self.assert_keypool_matches_range(wpool)
        wpool.unloadwallet()

    def test_active_conflict(self, priv_wpkh, priv_wpkh_internal):
        """One descriptor per (output type, externality) is active; a second import displaces it."""
        self.log.info("A second active ranged wpkh replaces the first for the same output type")
        self.nodes[1].createwallet(wallet_name='wactive', blank=True, descriptors=True)
        wactive = self.nodes[1].get_wallet_rpc('wactive')

        self.test_importdesc({"desc": priv_wpkh, "timestamp": "now", "active": True, "range": [0, 9]},
                             success=True, wallet=wactive)
        assert_equal(self.active_descriptor(wactive, internal=False)['desc'], priv_wpkh)

        self.test_importdesc({"desc": priv_wpkh_internal, "timestamp": "now", "active": True, "range": [0, 9]},
                             success=True, wallet=wactive)
        assert_equal(self.active_descriptor(wactive, internal=False)['desc'], priv_wpkh_internal)
        # The displaced descriptor is still watched, just no longer used for new addresses.
        descs = {d['desc']: d for d in wactive.listdescriptors(True)['descriptors']}
        assert_equal(descs[priv_wpkh]['active'], False)

        # Shrinking the range of an active descriptor would orphan already-issued addresses.
        self.test_importdesc({"desc": priv_wpkh_internal, "timestamp": "now", "active": True, "range": [0, 4]},
                             success=False,
                             error_code=-8,
                             error_message="new range must include current range = [0,9]",
                             wallet=wactive)
        wactive.unloadwallet()

    def test_encrypted_wallet(self, priv_wpkh):
        """The keypool is topped up during the import, so the passphrase must be held throughout."""
        self.log.info("An encrypted wallet must be unlocked to import a descriptor with keys")
        self.nodes[1].createwallet(wallet_name='wenc', blank=True, descriptors=True, passphrase='pass')
        wenc = self.nodes[1].get_wallet_rpc('wenc')
        wenc.walletlock()

        request = {"desc": priv_wpkh, "timestamp": "now", "active": True, "range": [0, 9]}
        assert_raises_rpc_error(-13, "Error: Please enter the wallet passphrase with walletpassphrase first.",
                                wenc.importdescriptors, [request])
        assert_equal(wenc.getwalletinfo()['keypoolsize'], 0)

        wenc.walletpassphrase('pass', 30)
        self.test_importdesc(request, success=True, wallet=wenc)
        assert wenc.getwalletinfo()['keypoolsize'] > 0
        self.assert_keypool_matches_range(wenc)
        wenc.unloadwallet()

    def test_non_descriptor_wallet_rejected(self):
        """importdescriptors has a non-descriptor branch upstream; RIGOR has no such wallet to reach it."""
        self.log.info("Legacy wallets cannot be created, so the non-descriptor import path is unreachable")
        assert_raises_rpc_error(-4, "descriptors=false is not supported", self.nodes[1].createwallet,
                               wallet_name='wlegacy', descriptors=False)

    def assert_keypool_matches_range(self, wallet):
        """getwalletinfo's pool sizes are just the unconsumed tail of each active range."""
        info = wallet.getwalletinfo()
        for internal, field in ((False, 'keypoolsize'), (True, 'keypoolsize_hd_internal')):
            actives = [d for d in wallet.listdescriptors(True)['descriptors']
                       if d['active'] and d.get('internal', False) == internal and 'range' in d]
            unconsumed = sum(d['range'][1] + 1 - d['next_index'] for d in actives)
            assert_equal(info[field], unconsumed)

    @staticmethod
    def active_descriptor(wallet, *, internal):
        actives = [d for d in wallet.listdescriptors(True)['descriptors']
                   if d['active'] and d.get('internal', False) == internal]
        assert_equal(len(actives), 1)
        return actives[0]


if __name__ == '__main__':
    ImportDescriptorsTest().main()
