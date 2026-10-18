#!/usr/bin/env python3
# Copyright (c) 2026 The RIGOR developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test importrigorprivkey / descriptor-first importprivkey for ORU and RES.

RIGOR CKey material is Ed25519-based (src/key.cpp). Do not compare imported ORU
addresses against Bitcoin secp256k1 helpers such as get_generate_key().
"""

import os

from test_framework.descriptors import descsum_create
from test_framework.ed25519 import seed_to_keypair
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error
from test_framework.wallet_util import bytes_to_wif


class ImportRigorPrivkeyTest(BitcoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser, legacy=False)

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()
        self.skip_if_no_sqlite()

    def run_test(self):
        node = self.nodes[0]

        self.log.info("ORU: importrigorprivkey WIF → wpkh / Bech32 (node-derived address)")
        wif = bytes_to_wif(os.urandom(32))
        node.createwallet(wallet_name="oru_dst", blank=True, descriptors=True)
        oru_dst = node.get_wallet_rpc("oru_dst")
        result = oru_dst.importrigorprivkey(wif, "oru-label", False, "ORU")
        assert_equal(result["success"], True)
        assert_equal(result["asset"], "ORU")
        assert result["descriptor"].startswith("wpkh(")
        assert "#" in result["descriptor"]
        derived = node.deriveaddresses(result["descriptor"])[0]
        assert_equal(result["address"], derived)
        info = oru_dst.getaddressinfo(result["address"])
        assert_equal(info["ismine"], True)
        assert_equal(info["iswatchonly"], False)

        self.log.info("ORU: importprivkey compat path on descriptor wallet")
        wif2 = bytes_to_wif(os.urandom(32))
        node.createwallet(wallet_name="oru2", blank=True, descriptors=True)
        oru2 = node.get_wallet_rpc("oru2")
        oru2.importprivkey(wif2, "", False)
        node.createwallet(wallet_name="oru2b", blank=True, descriptors=True)
        oru2b = node.get_wallet_rpc("oru2b")
        r2 = oru2b.importrigorprivkey(wif2, "", False, "ORU")
        assert_equal(oru2.getaddressinfo(r2["address"])["ismine"], True)

        self.log.info("dumpprivkey is absent; listdescriptors true is the private export path")
        assert_equal(node.help("dumpprivkey"), "help: unknown command: dumpprivkey")
        priv_descs = oru_dst.listdescriptors(True)["descriptors"]
        assert any(d["desc"].startswith("wpkh(") for d in priv_descs)

        self.log.info("RES: import a 64-byte Ed25519 seed||pubkey secret via importrigorprivkey")
        # RIGOR expects the libsodium layout: 32-byte seed followed by the 32-byte pubkey.
        pk32, sk64 = seed_to_keypair(os.urandom(32))
        res_secret = sk64.hex()
        res_desc = descsum_create(f"res({res_secret})")
        res_addr = node.deriveaddresses(res_desc)[0]

        node.createwallet(wallet_name="res_dst", blank=True, descriptors=True)
        res_dst = node.get_wallet_rpc("res_dst")
        imported = res_dst.importrigorprivkey(res_secret, "res-label", False, "RES")
        assert_equal(imported["success"], True)
        assert_equal(imported["asset"], "RES")
        assert imported["descriptor"].startswith("res(")
        assert_equal(imported["address"], res_addr)
        assert imported["descriptor"].split("#")[0] == res_desc.split("#")[0]

        self.log.info("Watch-only: public res() import; private key import must fail")
        node.createwallet(wallet_name="watch", disable_private_keys=True, blank=True, descriptors=True)
        watch = node.get_wallet_rpc("watch")
        pub_hex = res_secret[64:]
        pub_desc = descsum_create(f"res({pub_hex})")
        wr = watch.importdescriptors([{"desc": pub_desc, "timestamp": "now"}])
        assert_equal(wr[0]["success"], True)
        # Descriptor watch-only wallets report ismine=true / iswatchonly=false for watched scripts.
        winfo = watch.getwalletinfo()
        assert_equal(winfo["private_keys_enabled"], False)
        info = watch.getaddressinfo(res_addr)
        assert_equal(info["ismine"], True)
        assert_equal(info["solvable"], True)
        assert_raises_rpc_error(-4, "private keys disabled", watch.importrigorprivkey, res_secret)

        self.log.info("Reject Bitcoin Taproot tr(): RES must use res()")
        fake_pub = "02" + "11" * 32
        bad = watch.importdescriptors([{"desc": descsum_create("tr(" + fake_pub + ")"), "timestamp": "now"}])
        assert_equal(bad[0]["success"], False)
        assert "tr() is not supported" in bad[0]["error"]["message"]


if __name__ == '__main__':
    ImportRigorPrivkeyTest().main()
