#!/usr/bin/env python3
# Copyright (c) 2017-2022 The Bitcoin Core developers
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""RigorChain: wallet bech32 / bech32m address behaviour.

Legacy P2PKH and P2SH-segwit are disabled (RPC rejects generation; mempool policy
rejects such outputs). This test covers only supported types.
"""

from test_framework.address import FUNCTIONAL_CHAIN_BECH32_HRP
from test_framework.blocktools import COINBASE_MATURITY
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error


class AddressTypeTest(BitcoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)

    def set_test_params(self):
        self.num_nodes = 3
        # Nodes 0–1: wallets under test; node 2: mines and funds.
        self.extra_args = [
            ["-addresstype=bech32"],
            ["-addresstype=bech32"],
            ["-addresstype=bech32"],
        ]
        for args in self.extra_args:
            args.append("-whitelist=noban@127.0.0.1")
        self.supports_cli = False

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_network(self):
        self.setup_nodes()
        self.connect_nodes(0, 2)
        self.connect_nodes(1, 2)
        self.connect_nodes(0, 1)

    def test_address_bech32(self, node_idx, address):
        info = self.nodes[node_idx].getaddressinfo(address)
        assert self.nodes[node_idx].validateaddress(address)["isvalid"]
        assert_equal(info["solvable"], True)
        assert not info["isscript"]
        assert info["iswitness"]
        assert_equal(info["witness_version"], 0)
        assert_equal(len(info["witness_program"]), 40)
        assert "pubkey" in info

    def test_address_bech32m(self, node_idx, address):
        info = self.nodes[node_idx].getaddressinfo(address)
        assert self.nodes[node_idx].validateaddress(address)["isvalid"]
        assert_equal(info["solvable"], True)
        assert info["isscript"]
        assert info["iswitness"]
        assert_equal(info["witness_version"], 1)
        assert_equal(len(info["witness_program"]), 64)

    def test_res_v1_address_matches_descriptor(self, node_idx):
        """Witness v1 RES: explicit getnewaddress \"\" \"bech32m\", Bech32m encoding, res() desc, spk matches deriveaddresses."""
        node = self.nodes[node_idx]
        hrp = FUNCTIONAL_CHAIN_BECH32_HRP[self.chain]
        addr = node.getnewaddress("", "bech32m")
        assert_equal(addr.lower()[: len(hrp) + 1], (hrp + "1").lower())

        info = node.getaddressinfo(addr)
        assert_equal(info["solvable"], True)
        assert "desc" in info
        assert "res(" in info["desc"]

        spk_hex = info["scriptPubKey"]
        # getaddressinfo "desc" is InferDescriptor()->ToString(), which includes a single '#…' checksum.
        desc_str = info["desc"]
        di = node.getdescriptorinfo(desc_str)
        if di["isrange"]:
            assert "hdkeypath" in info
            idx = int(info["hdkeypath"].split("/")[-1])
            derived_addrs = node.deriveaddresses(desc_str, f"[{idx},{idx}]")
        else:
            derived_addrs = node.deriveaddresses(desc_str)
        assert_equal(len(derived_addrs), 1)
        assert_equal(derived_addrs[0], addr)
        assert_equal(node.getaddressinfo(derived_addrs[0])["scriptPubKey"], spk_hex)

    def run_test(self):
        miner = 2
        self.generate(self.nodes[miner], COINBASE_MATURITY + 1)

        self.log.info("RPC rejects legacy and p2sh-segwit address types")
        for addr_type in ("legacy", "p2sh-segwit"):
            # ParseOutputType does not accept these names; the wallet reports them as
            # unknown rather than the older "disabled on RigorChain" parameter error.
            assert_raises_rpc_error(
                -5,
                "Unknown address type",
                self.nodes[0].getnewaddress,
                "",
                addr_type,
            )
            assert_raises_rpc_error(
                -5,
                "Unknown address type",
                self.nodes[0].getrawchangeaddress,
                addr_type,
            )

        self.log.info("Default and explicit bech32m addresses")
        a0 = self.nodes[0].getnewaddress(address_type='bech32m')
        a0e = self.nodes[0].getnewaddress("", "bech32m")
        self.test_address_bech32m(0, a0)
        self.test_address_bech32m(0, a0e)
        rc = self.nodes[0].getrawchangeaddress()
        self.test_address_bech32(0, rc)

        if self.options.descriptors:
            self.log.info("Descriptor wallet: bech32m addresses")
            m = self.nodes[0].getnewaddress("", "bech32m")
            mc = self.nodes[0].getrawchangeaddress("bech32m")
            self.test_address_bech32m(0, m)
            self.test_address_bech32m(0, mc)
            self.log.info("Descriptor wallet: V1 RES address matches res() expansion (getdescriptorinfo / deriveaddresses)")
            self.test_res_v1_address_matches_descriptor(0)
        else:
            self.log.info("Legacy wallet cannot use bech32m (unchanged upstream rule)")
            assert_raises_rpc_error(
                -8,
                "Legacy wallets cannot provide bech32m addresses",
                self.nodes[0].getnewaddress,
                "",
                "bech32m",
            )


if __name__ == "__main__":
    AddressTypeTest().main()
