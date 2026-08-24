#!/usr/bin/env python3
# Copyright (c) 2018-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the deriveaddresses rpc call."""
from test_framework.test_framework import BitcoinTestFramework
from test_framework.descriptors import descsum_create
from test_framework.util import assert_equal, assert_raises_rpc_error

TPRV = "tprv8ZgxMBicQKsPd7Uf69XL1XwhmjHopUGep8GuEiJDZmbQz6o58LninorQAfcKZWARbtRtfnLcJ5MQ2AtHcQJCCRUcMRvmDUjyEmNUWwx8UbK"
TPUB = "tpubD6NzVbkrYhZ4WaWSyoBvQwbpLkojyoTZPRsgXELWz3Popb3qkjcJyJUGLnL4qHHoQvao8ESaAstxYSnhyswJ76uZPStJRJCTKvosUCJZL5B"


class DeriveaddressesTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def run_test(self):
        assert_raises_rpc_error(-5, "Missing checksum", self.nodes[0].deriveaddresses, "a")

        # Ed25519 BIP32 is hardened-only. Unhardened paths are rejected at parse time.
        assert_raises_rpc_error(-5, "hardened", self.nodes[0].deriveaddresses, descsum_create(f"wpkh({TPRV}/1/1/0)"))
        assert_raises_rpc_error(-5, "hardened", self.nodes[0].deriveaddresses, descsum_create(f"wpkh({TPUB}/1/1/0)"))

        descriptor = descsum_create(f"wpkh({TPRV}/1'/1'/0')")
        addrs = self.nodes[0].deriveaddresses(descriptor)
        assert_equal(len(addrs), 1)
        assert addrs[0].startswith("rgrt1")

        descriptor_no_sum = descriptor[:-9]
        assert_raises_rpc_error(-5, "Missing checksum", self.nodes[0].deriveaddresses, descriptor_no_sum)

        ranged_descriptor = descsum_create(f"wpkh({TPRV}/1'/1'/*')")
        ranged = self.nodes[0].deriveaddresses(ranged_descriptor, [1, 2])
        assert_equal(len(ranged), 2)
        assert ranged[0] != ranged[1]
        from_zero = self.nodes[0].deriveaddresses(ranged_descriptor, 2)
        assert_equal(len(from_zero), 3)
        assert_equal(from_zero[0], addrs[0])

        assert_raises_rpc_error(-8, "Range should not be specified for an un-ranged descriptor", self.nodes[0].deriveaddresses, descsum_create(f"wpkh({TPRV}/1'/1'/0')"), [0, 2])

        assert_raises_rpc_error(-8, "Range must be specified for a ranged descriptor", self.nodes[0].deriveaddresses, ranged_descriptor)

        assert_raises_rpc_error(-8, "End of range is too high", self.nodes[0].deriveaddresses, ranged_descriptor, 10000000000)

        assert_raises_rpc_error(-8, "Range is too large", self.nodes[0].deriveaddresses, ranged_descriptor, [1000000000, 2000000000])

        assert_raises_rpc_error(-8, "Range specified as [begin,end] must not have begin after end", self.nodes[0].deriveaddresses, ranged_descriptor, [2, 0])

        assert_raises_rpc_error(-8, "Range should be greater or equal than 0", self.nodes[0].deriveaddresses, ranged_descriptor, [-1, 0])

        combo_descriptor = descsum_create(f"combo({TPRV}/1'/1'/0')")
        combo_addrs = self.nodes[0].deriveaddresses(combo_descriptor)
        assert_equal(len(combo_addrs), 4)

        max_index = self.nodes[0].deriveaddresses(ranged_descriptor, [2147483647, 2147483647])
        assert_equal(len(max_index), 1)
        assert max_index[0].startswith("rgrt1")

        hardened_without_privkey_descriptor = descsum_create(f"wpkh({TPUB}/1'/1'/0')")
        assert_raises_rpc_error(-5, "Cannot derive script without private keys", self.nodes[0].deriveaddresses, hardened_without_privkey_descriptor)


if __name__ == '__main__':
    DeriveaddressesTest().main()
