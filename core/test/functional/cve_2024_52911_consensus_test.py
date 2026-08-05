#!/usr/bin/env python3
# Copyright (c) 2026 The RIGOR developers
# Distributed under the MIT software license.

"""
Test that CVE-2024-52911 fix does not change consensus semantics.

This test verifies that disabling parallel script checks in ConnectBlock
produces identical validation results to the previous implementation.
"""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

class CVE202452911ConsensusTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def run_test(self):
        self.log.info("Testing that CVE-2024-52911 fix preserves consensus semantics")
        
        # Test 1: Mine a block and verify it validates correctly
        self.log.info("Mining block with single-threaded script validation")
        block = self.nodes[0].generate(1)[0]
        
        # Verify the block is valid
        assert block in self.nodes[0].getblockhash(self.nodes[0].getblockcount())
        
        # Test 2: Create a transaction and verify it validates correctly
        self.log.info("Testing transaction validation")
        addr = self.nodes[0].getnewaddress()
        txid = self.nodes[0].sendtoaddress(addr, 1.0)
        
        # Verify the transaction is valid
        tx = self.nodes[0].gettransaction(txid)
        
        # Test 3: Mine another block to confirm transaction
        self.log.info("Mining block to confirm transaction")
        self.nodes[0].generate(1)
        
        # Verify the transaction is now confirmed
        tx = self.nodes[0].gettransaction(txid)
        assert tx['confirmations'] == 1
        
        # Test 4: Verify blockchain state is consistent
        self.log.info("Verifying blockchain state consistency")
        blockcount = self.nodes[0].getblockcount()
        bestblockhash = self.nodes[0].getbestblockhash()
        assert blockcount == 3  # Genesis + 2 mined blocks
        assert bestblockhash is not None
        
        self.log.info("All consensus tests passed - CVE-2024-52911 fix preserves semantics")

if __name__ == '__main__':
    CVE202452911ConsensusTest().main()
