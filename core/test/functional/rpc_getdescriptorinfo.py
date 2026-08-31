#!/usr/bin/env python3
# Copyright (c) 2019-2022 The Bitcoin Core developers
# Copyright (c) 2026 The RIGOR developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test getdescriptorinfo RPC for RIGOR locks."""

from test_framework.descriptors import descsum_create
from test_framework.ed25519 import Ed25519Key
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)


class DescriptorTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [["-disablewallet"]]
        self.wallet_names = []

    def test_desc(self, desc, isrange, issolvable, hasprivatekeys):
        info = self.nodes[0].getdescriptorinfo(desc)
        assert_equal(info, self.nodes[0].getdescriptorinfo(descsum_create(desc)))
        assert_equal(info['descriptor'], descsum_create(desc))
        assert_equal(info['isrange'], isrange)
        assert_equal(info['issolvable'], issolvable)
        assert_equal(info['hasprivatekeys'], hasprivatekeys)

    def run_test(self):
        assert_raises_rpc_error(-1, 'getdescriptorinfo', self.nodes[0].getdescriptorinfo)
        assert_raises_rpc_error(-3, 'JSON value of type number is not of expected type string', self.nodes[0].getdescriptorinfo, 1)
        assert_raises_rpc_error(-5, "'' is not a valid descriptor function", self.nodes[0].getdescriptorinfo, "")

        key = Ed25519Key.from_seed_int(1)
        cpk = key.get_cpubkey_bytes().hex()
        pk32 = key.get_pubkey_bytes().hex()
        secp = "0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798"

        self.log.info("wpkh() and res() analyse")
        self.test_desc(f'wpkh({cpk})', isrange=False, issolvable=True, hasprivatekeys=False)
        self.test_desc(f'res({pk32})', isrange=False, issolvable=True, hasprivatekeys=False)

        self.log.info("Bitcoin leftover descriptors are rejected")
        for expr, needle in (
            (f'pk({secp})', 'pk() is not supported'),
            (f'pkh({secp})', 'pkh() is not supported'),
            (f'combo({secp})', 'combo() is not supported'),
            (f'sh(wpkh({secp}))', 'sh() is not supported'),
            (f'wsh(pkh({secp}))', 'wsh() is not supported'),
            (f'multi(1,{secp},{secp})', 'multi() is not supported'),
            (f'tr({secp})', 'tr() is not supported'),
        ):
            assert_raises_rpc_error(-5, needle, self.nodes[0].getdescriptorinfo, expr)


if __name__ == '__main__':
    DescriptorTest().main()
