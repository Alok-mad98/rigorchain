#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""wallet address domain binding (ORU vs RES) — RPC + spend-path guards.

Mirrors RFC-ADDRESS-001 / sendtoaddress checks and WalletStrictSendDestinationOk (witness + script-inferred asset).
"""

from test_framework.address import FUNCTIONAL_CHAIN_BECH32_HRP, mini_wallet_wpkh_address
from test_framework.segwit_addr import decode_segwit_address, encode_segwit_address
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_raises_rpc_error


class WalletAssetDomainTest(BitcoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        self.log.info("Fund default descriptor wallet (ORU bech32)")
        n = self.nodes[0]
        self.generatetoaddress(n, 101, n.getnewaddress(address_type="bech32"))
        w = n.get_wallet_rpc(self.default_wallet_name)

        hrp = FUNCTIONAL_CHAIN_BECH32_HRP["regtest"]
        _v0, p0 = decode_segwit_address(hrp, mini_wallet_wpkh_address())
        oru_ext = encode_segwit_address(hrp, 0, p0)
        _v1, p1 = decode_segwit_address(
            hrp,
            "rgrt1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vqmam9sw",
        )
        res_addr = encode_segwit_address(hrp, 1, p1)

        self.log.info("sendtoaddress: ORU asset to witness v1 RES address must fail")
        assert_raises_rpc_error(
            -5,
            "Cannot send ORU to V1",
            w.sendtoaddress,
            res_addr,
            0.01,
        )

        self.log.info("sendtoaddress: RES asset to witness v0 ORU address must fail")
        assert_raises_rpc_error(
            -5,
            "Cannot send RES to V0",
            w.sendtoaddress,
            oru_ext,
            0.01,
            "",
            "",
            False,
            False,
            6,
            "unset",
            False,
            None,
            False,
            "RES",
        )

        self.log.info("sendmany: mixed ORU+RES recipients rejected")
        assert_raises_rpc_error(
            -8,
            "cannot mix ORU",
            w.sendmany,
            "",
            {oru_ext: 0.01, res_addr: 0.01},
        )

        self.log.info(": PASS")


if __name__ == "__main__":
    WalletAssetDomainTest().main()
