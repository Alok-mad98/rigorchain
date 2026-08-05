#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Assert RigorChain rejects Bitcoin Taproot descriptor semantics."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error


class TaprootRejectTest(BitcoinTestFramework):
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
        taproot_err = "not supported on RigorChain"

        tr_desc = "tr(a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd)"
        rawtr_desc = "rawtr(a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd)"

        assert_raises_rpc_error(-5, taproot_err, node.getdescriptorinfo, tr_desc)
        assert_raises_rpc_error(-5, taproot_err, node.getdescriptorinfo, rawtr_desc)

        # deriveaddresses requires checksum form.
        assert_raises_rpc_error(
            -5,
            taproot_err,
            node.deriveaddresses,
            "tr(a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd)#dh4fyxrd",
        )

        result = node.importdescriptors(
            [{"desc": "tr(a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd)#dh4fyxrd", "timestamp": "now"}]
        )
        assert_equal(result[0]["success"], False)
        assert taproot_err in result[0]["error"]["message"]


if __name__ == "__main__":
    TaprootRejectTest().main()
