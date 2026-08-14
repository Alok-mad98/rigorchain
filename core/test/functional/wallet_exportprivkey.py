#!/usr/bin/env python3
# Copyright (c) 2026 The RIGOR developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test dumporuprivkey / dumpresprivkey descriptor-first address export.

Coverage:
- ORU: export WIF, re-import externally, address match
- RES: export Ed25519 secret hex, verify format / re-import
- Negatives: unknown address, locked wallet, watch-only, wrong asset
"""

from test_framework.descriptors import descsum_create
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error


def extract_res_secret(priv_descriptors):
    for d in priv_descriptors:
        desc = d["desc"]
        if not desc.startswith("res("):
            continue
        inner = desc[len("res("):desc.index(")")]
        if len(inner) == 128:
            return inner, desc
    return None, None


class WalletExportPrivkeyTest(BitcoinTestFramework):
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

        self.log.info("ORU: generate address, dumporuprivkey, re-import, address match")
        node.createwallet(wallet_name="oru", descriptors=True)
        oru = node.get_wallet_rpc("oru")
        oru_addr = oru.getnewaddress(address_type="bech32")
        exported = oru.dumporuprivkey(oru_addr)
        assert_equal(exported["address"], oru_addr)
        assert_equal(exported["asset"], "ORU")
        assert_equal(exported["format"], "WIF")
        assert exported["private_key"]

        node.createwallet(wallet_name="oru_ext", blank=True, descriptors=True)
        oru_ext = node.get_wallet_rpc("oru_ext")
        imported = oru_ext.importrigorprivkey(exported["private_key"], "", False, "ORU")
        assert_equal(imported["success"], True)
        assert_equal(imported["address"], oru_addr)

        self.log.info("RES: dumpresprivkey secret format and re-import")
        node.createwallet(wallet_name="res", descriptors=True)
        res = node.get_wallet_rpc("res")
        res_secret, res_desc = extract_res_secret(res.listdescriptors(True)["descriptors"])
        assert res_secret is not None
        res_addr = node.deriveaddresses(res_desc)[0]
        # Ensure address is known to wallet receive path
        assert_equal(res.getaddressinfo(res_addr)["ismine"], True)

        res_exported = res.dumpresprivkey(res_addr)
        assert_equal(res_exported["address"], res_addr)
        assert_equal(res_exported["asset"], "RES")
        assert_equal(res_exported["format"], "ed25519")
        assert_equal(len(res_exported["private_key"]), 128)
        assert_equal(res_exported["private_key"], res_secret)

        node.createwallet(wallet_name="res_ext", blank=True, descriptors=True)
        res_ext = node.get_wallet_rpc("res_ext")
        res_imp = res_ext.importrigorprivkey(res_exported["private_key"], "", False, "RES")
        assert_equal(res_imp["address"], res_addr)

        self.log.info("Negative: unknown address")
        node.createwallet(wallet_name="other", descriptors=True)
        other = node.get_wallet_rpc("other")
        unknown = other.getnewaddress(address_type="bech32")
        assert_raises_rpc_error(-4, "Address not found in wallet", oru.dumporuprivkey, unknown)

        self.log.info("Negative: wrong asset")
        assert_raises_rpc_error(-5, "Only ORU addresses are accepted", oru.dumporuprivkey, res_addr)
        assert_raises_rpc_error(-5, "Only RES addresses are accepted", res.dumpresprivkey, oru_addr)

        self.log.info("Negative: locked wallet")
        node.createwallet(wallet_name="enc", descriptors=True)
        enc = node.get_wallet_rpc("enc")
        enc_addr = enc.getnewaddress(address_type="bech32")
        enc.encryptwallet("testpass")
        assert_raises_rpc_error(-13, "Wallet is locked", enc.dumporuprivkey, enc_addr)
        enc.walletpassphrase("testpass", 60)
        unlocked = enc.dumporuprivkey(enc_addr)
        assert_equal(unlocked["asset"], "ORU")
        enc.walletlock()

        self.log.info("Negative: watch-only wallet")
        node.createwallet(wallet_name="watch", disable_private_keys=True, blank=True, descriptors=True)
        watch = node.get_wallet_rpc("watch")
        pub_hex = res_secret[64:]
        pub_desc = descsum_create(f"res({pub_hex})")
        wr = watch.importdescriptors([{"desc": pub_desc, "timestamp": "now"}])
        assert_equal(wr[0]["success"], True)
        assert_raises_rpc_error(
            -4,
            "Private key unavailable for watch-only descriptor",
            watch.dumpresprivkey,
            res_addr,
        )

        self.log.info("dumpprivkey still redirects on descriptor wallets")
        assert_raises_rpc_error(
            -4,
            "dumpprivkey is not supported on descriptor wallets",
            oru.dumpprivkey,
            oru_addr,
        )


if __name__ == "__main__":
    WalletExportPrivkeyTest().main()
