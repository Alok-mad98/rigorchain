#!/usr/bin/env python3
# Copyright (c) 2018-2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test bitcoind shutdown."""

import base64
import http.client
import json
import time
from threading import Thread
from urllib.parse import urlparse

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, get_rpc_proxy

def test_long_call(node):
    block = node.waitfornewblock()
    assert_equal(block['height'], 0)

def drop_gbt_longpoll(node):
    """Start a GBT longpoll and close the client socket (killed solo miner)."""
    template = node.getblocktemplate({'rules': ['segwit']})
    parsed = urlparse(node.url)
    auth = base64.b64encode(f"{parsed.username}:{parsed.password}".encode()).decode()
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port, timeout=2)
    body = json.dumps({
        'jsonrpc': '1.0',
        'id': 'drop-lp',
        'method': 'getblocktemplate',
        'params': [{'rules': ['segwit'], 'longpollid': template['longpollid']}],
    })
    conn.request('POST', '/', body, {
        'Authorization': f'Basic {auth}',
        'Content-Type': 'application/json',
    })
    # Let the worker enter the longpoll wait, then drop the TCP client.
    time.sleep(0.5)
    conn.close()

class ShutdownTest(BitcoinTestFramework):

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.supports_cli = False

    def run_test(self):
        self.log.info("Abandoned GBT longpoll (miner already gone) must not pin stop")
        self.generate(self.nodes[0], 1)
        drop_gbt_longpoll(self.nodes[0])
        self.stop_node(0)
        self.start_node(0)

        node = get_rpc_proxy(self.nodes[0].url, 1, timeout=600, coveragedir=self.nodes[0].coverage_dir)
        # Force connection establishment by executing a dummy command.
        node.getblockcount()
        Thread(target=test_long_call, args=(node,)).start()
        # Wait until the server is executing the above `waitfornewblock`.
        self.wait_until(lambda: len(self.nodes[0].getrpcinfo()['active_commands']) == 2)
        # Wait 1 second after requesting shutdown but not before the `stop` call
        # finishes. This is to ensure event loop waits for current connections
        # to close.
        self.stop_node(0, wait=1000)

if __name__ == '__main__':
    ShutdownTest().main()
