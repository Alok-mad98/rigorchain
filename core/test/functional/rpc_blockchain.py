#!/usr/bin/env python3
# Copyright (c) 2014-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test RPCs related to blockchainstate.

Test the following RPCs:
    - getblockchaininfo
    - getdeploymentinfo
    - getchaintxstats
    - gettxoutsetinfo
    - getblockheader
    - getdifficulty
    - getnetworkhashps
    - waitforblockheight
    - getblock
    - getblockhash
    - getbestblockhash
    - verifychain

Tests correspond to code in rpc/blockchain.cpp.
"""

from decimal import Decimal
import http.client
import os
import subprocess
import textwrap

from test_framework.blocktools import (
    MAX_FUTURE_BLOCK_TIME,
    add_witness_commitment,
    create_block,
    create_coinbase,
    get_regtest_block_subsidy_sats,
)
from test_framework.messages import (
    CBlockHeader,
    COIN,
    from_hex,
    msg_block,
)
from test_framework.p2p import P2PInterface
from test_framework.rigor_pow import rigor_pow_digest_hex
from test_framework.script import CScript, hash256
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_greater_than_or_equal,
    assert_raises,
    assert_raises_rpc_error,
    assert_is_hex_string,
    assert_is_hash_string,
    get_datadir_path,
)
from test_framework.test_node import ErrorMatch
from test_framework.wallet import MiniWallet


HEIGHT = 200  # blocks mined
TIME_RANGE_STEP = 600  # ten-minute steps


class BlockchainTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.supports_cli = False

    def run_test(self):
        self.wallet = MiniWallet(self.nodes[0])
        self._test_prune_disk_space()
        self.mine_chain()
        self._test_max_future_block_time()
        self.restart_node(
            0,
            extra_args=[
                "-stopatheight=207",
                "-checkblocks=-1",  # Check all blocks
                "-prune=1",  # Set pruning after rescan is complete
            ],
        )

        self._test_getblockchaininfo()
        self._test_getdeploymentinfo()
        self._test_getchaintxstats()
        self._test_gettxoutsetinfo()
        self._test_getblockheader()
        self._test_getdifficulty()
        self._test_getnetworkhashps()
        self._test_stopatheight()
        self._test_waitforblockheight()
        self._test_getblock()
        self._test_y2106()
        assert self.nodes[0].verifychain(4, 0)

    def mine_chain(self):
        # RIGOR regtest genesis is not Bitcoin TIME_GENESIS_BLOCK; mock from the real genesis time.
        genesis_time = self.nodes[0].getblock(self.nodes[0].getblockhash(0))['time']
        self.log.info(f"Generate {HEIGHT} blocks after the genesis block in ten-minute steps")
        for i in range(HEIGHT):
            t = genesis_time + (i + 1) * TIME_RANGE_STEP
            self.nodes[0].setmocktime(t)
            self.generate(self.wallet, 1)
        assert_equal(self.nodes[0].getblockchaininfo()['blocks'], HEIGHT)
        self.time_range_tip = genesis_time + HEIGHT * TIME_RANGE_STEP
        self.time_range_mtp = genesis_time + (HEIGHT - 5) * TIME_RANGE_STEP
        self.time_last_block = self.time_range_tip

    def _test_prune_disk_space(self):
        self.log.info("Test that a manually pruned node does not run into "
                      "integer overflow on first start up")
        self.restart_node(0, extra_args=["-prune=1"])
        self.log.info("Avoid warning when assumed chain size is enough")
        self.restart_node(0, extra_args=["-prune=123456789"])

    def _test_max_future_block_time(self):
        self.stop_node(0)
        self.log.info("A block tip of more than MAX_FUTURE_BLOCK_TIME in the future raises an error")
        self.nodes[0].assert_start_raises_init_error(
            extra_args=[f"-mocktime={self.time_range_tip - MAX_FUTURE_BLOCK_TIME - 1}"],
            expected_msg=": The block database contains a block which appears to be from the future."
            " This may be due to your computer's date and time being set incorrectly."
            f" Only rebuild the block database if you are sure that your computer's date and time are correct.{os.linesep}"
            "Please restart with -reindex or -reindex-chainstate to recover.",
        )
        self.log.info("A block tip of MAX_FUTURE_BLOCK_TIME in the future is fine")
        self.start_node(0, extra_args=[f"-mocktime={self.time_range_tip - MAX_FUTURE_BLOCK_TIME}"])

    def _test_getblockchaininfo(self):
        self.log.info("Test getblockchaininfo")

        keys = [
            'bestblockhash',
            'blocks',
            'chain',
            'chainwork',
            'cumulative_convergence',
            'current_convergence_sum',
            'difficulty',
            'headers',
            'initialblockdownload',
            'mediantime',
            'pruned',
            'size_on_disk',
            'time',
            'verificationprogress',
            'warnings',
        ]
        res = self.nodes[0].getblockchaininfo()

        assert_equal(res['time'], self.time_last_block)
        assert_equal(res['mediantime'], self.time_range_mtp)

        # result should have these additional pruning keys if manual pruning is enabled
        assert_equal(sorted(res.keys()), sorted(['pruneheight', 'automatic_pruning'] + keys))

        # size_on_disk should be > 0
        assert_greater_than(res['size_on_disk'], 0)

        # pruneheight should be greater or equal to 0
        assert_greater_than_or_equal(res['pruneheight'], 0)

        # check other pruning fields given that prune=1
        assert res['pruned']
        assert not res['automatic_pruning']

        self.restart_node(0, ['-stopatheight=207'])
        res = self.nodes[0].getblockchaininfo()
        # should have exact keys
        assert_equal(sorted(res.keys()), keys)

        self.stop_node(0)
        # -testactivationheight is not a registered argument on any RIGOR chain, so it is
        # rejected while parsing arguments, before the value is even looked at. One case
        # covers every spelling.
        self.nodes[0].assert_start_raises_init_error(
            extra_args=['-testactivationheight=csv@5'],
            expected_msg="Invalid parameter -testactivationheight",
            match=ErrorMatch.PARTIAL_REGEX,
        )
        self.nodes[0].assert_start_raises_init_error(
            extra_args=['-vbparams=testdummy:0:0'],
            expected_msg="Invalid parameter -vbparams",
            match=ErrorMatch.PARTIAL_REGEX,
        )
        self.start_node(0, extra_args=[
            '-stopatheight=207',
            '-prune=550',
        ])

        res = self.nodes[0].getblockchaininfo()
        # result should have these additional pruning keys if prune=550
        assert_equal(sorted(res.keys()), sorted(['pruneheight', 'automatic_pruning', 'prune_target_size'] + keys))

        # check related fields
        assert res['pruned']
        assert_equal(res['pruneheight'], 0)
        assert res['automatic_pruning']
        assert_equal(res['prune_target_size'], 576716800)
        assert_greater_than(res['size_on_disk'], 0)

    def _test_getdeploymentinfo(self):
        self.log.info("Test getdeploymentinfo")
        node = self.nodes[0]

        info = node.getdeploymentinfo()
        assert_equal(info['hash'], node.getbestblockhash())
        assert_equal(info['height'], node.getblockcount())

        # Every buried softfork is active from genesis and pinned to a fixed height, so there is
        # no bit, no signalling window, and nothing for -testactivationheight to move.
        for name, expected_height in (('bip34', 1), ('bip66', 1), ('bip65', 1), ('csv', 1), ('segwit', 0)):
            deployment = info['deployments'][name]
            assert_equal(deployment['type'], 'buried')
            assert_equal(deployment['active'], True)
            assert_equal(deployment['height'], expected_height)

        # Querying a specific block is the path the REST endpoint uses.
        assert_equal(node.getdeploymentinfo(node.getblockhash(0))['height'], 0)
        assert_raises_rpc_error(-5, "Block not found", node.getdeploymentinfo, "00" * 32)

    def _test_y2106(self):
        self.log.info("Check that block timestamps work until year 2106")
        self.generate(self.nodes[0], 8)[-1]
        time_2106 = 2**32 - 1
        self.nodes[0].setmocktime(time_2106)
        last = self.generate(self.nodes[0], 6)[-1]
        assert_equal(self.nodes[0].getblockheader(last)["mediantime"], time_2106)

    def _test_getchaintxstats(self):
        self.log.info("Test getchaintxstats")

        # Test `getchaintxstats` invalid extra parameters
        assert_raises_rpc_error(-1, 'getchaintxstats', self.nodes[0].getchaintxstats, 0, '', 0)

        # Test `getchaintxstats` invalid `nblocks`
        assert_raises_rpc_error(-3, "JSON value of type string is not of expected type number", self.nodes[0].getchaintxstats, '')
        assert_raises_rpc_error(-8, "Invalid block count: should be between 0 and the block's height - 1", self.nodes[0].getchaintxstats, -1)
        assert_raises_rpc_error(-8, "Invalid block count: should be between 0 and the block's height - 1", self.nodes[0].getchaintxstats, self.nodes[0].getblockcount())

        # Test `getchaintxstats` invalid `blockhash`
        assert_raises_rpc_error(-3, "JSON value of type number is not of expected type string", self.nodes[0].getchaintxstats, blockhash=0)
        assert_raises_rpc_error(-8, "blockhash must be of length 64 (not 1, for '0')", self.nodes[0].getchaintxstats, blockhash='0')
        assert_raises_rpc_error(-8, "blockhash must be hexadecimal string (not 'ZZZ0000000000000000000000000000000000000000000000000000000000000')", self.nodes[0].getchaintxstats, blockhash='ZZZ0000000000000000000000000000000000000000000000000000000000000')
        assert_raises_rpc_error(-5, "Block not found", self.nodes[0].getchaintxstats, blockhash='0000000000000000000000000000000000000000000000000000000000000000')
        blockhash = self.nodes[0].getblockhash(HEIGHT)
        self.nodes[0].invalidateblock(blockhash)
        assert_raises_rpc_error(-8, "Block is not in main chain", self.nodes[0].getchaintxstats, blockhash=blockhash)
        self.nodes[0].reconsiderblock(blockhash)

        chaintxstats = self.nodes[0].getchaintxstats(nblocks=1)
        # 200 txs plus genesis tx
        assert_equal(chaintxstats['txcount'], HEIGHT + 1)
        # tx rate should be 1 per 10 minutes, or 1/600
        # we have to round because of binary math
        assert_equal(round(chaintxstats['txrate'] * TIME_RANGE_STEP, 10), Decimal(1))

        b1_hash = self.nodes[0].getblockhash(1)
        b1 = self.nodes[0].getblock(b1_hash)
        b200_hash = self.nodes[0].getblockhash(HEIGHT)
        b200 = self.nodes[0].getblock(b200_hash)
        time_diff = b200['mediantime'] - b1['mediantime']

        chaintxstats = self.nodes[0].getchaintxstats()
        assert_equal(chaintxstats['time'], b200['time'])
        assert_equal(chaintxstats['txcount'], HEIGHT + 1)
        assert_equal(chaintxstats['window_final_block_hash'], b200_hash)
        assert_equal(chaintxstats['window_final_block_height'], HEIGHT )
        assert_equal(chaintxstats['window_block_count'], HEIGHT - 1)
        assert_equal(chaintxstats['window_tx_count'], HEIGHT - 1)
        assert_equal(chaintxstats['window_interval'], time_diff)
        assert_equal(round(chaintxstats['txrate'] * time_diff, 10), Decimal(HEIGHT - 1))

        chaintxstats = self.nodes[0].getchaintxstats(blockhash=b1_hash)
        assert_equal(chaintxstats['time'], b1['time'])
        assert_equal(chaintxstats['txcount'], 2)
        assert_equal(chaintxstats['window_final_block_hash'], b1_hash)
        assert_equal(chaintxstats['window_final_block_height'], 1)
        assert_equal(chaintxstats['window_block_count'], 0)
        assert 'window_tx_count' not in chaintxstats
        assert 'window_interval' not in chaintxstats
        assert 'txrate' not in chaintxstats

    def _test_gettxoutsetinfo(self):
        node = self.nodes[0]
        res = node.gettxoutsetinfo()

        # Heights 1..HEIGHT (genesis coinbase is not in the UTXO set). Regtest
        # halves at 150 from 1000 ORU, so this is not Bitcoin's 50/25 schedule.
        expected_amount = Decimal(sum(get_regtest_block_subsidy_sats(h) for h in range(1, HEIGHT + 1))) / COIN
        assert_equal(res['total_amount'], expected_amount)
        assert_equal(res['transactions'], HEIGHT)
        assert_equal(res['height'], HEIGHT)
        assert_equal(res['txouts'], HEIGHT)
        # P2WPKH scriptPubKey is 22 bytes → GetBogoSize = 72 per output.
        assert_equal(res['bogosize'], HEIGHT * 72),
        assert_equal(res['bestblock'], node.getblockhash(HEIGHT))
        size = res['disk_size']
        assert size > 6400
        assert size < 64000
        assert_equal(len(res['bestblock']), 64)
        assert_equal(len(res['hash_serialized_2']), 64)

        self.log.info("Test gettxoutsetinfo works for blockchain with just the genesis block")
        b1hash = node.getblockhash(1)
        node.invalidateblock(b1hash)

        res2 = node.gettxoutsetinfo()
        assert_equal(res2['transactions'], 0)
        assert_equal(res2['total_amount'], Decimal('0'))
        assert_equal(res2['height'], 0)
        assert_equal(res2['txouts'], 0)
        assert_equal(res2['bogosize'], 0),
        assert_equal(res2['bestblock'], node.getblockhash(0))
        assert_equal(len(res2['hash_serialized_2']), 64)

        self.log.info("Test gettxoutsetinfo returns the same result after invalidate/reconsider block")
        node.reconsiderblock(b1hash)

        res3 = node.gettxoutsetinfo()
        # The field 'disk_size' is non-deterministic and can thus not be
        # compared between res and res3.  Everything else should be the same.
        del res['disk_size'], res3['disk_size']
        assert_equal(res, res3)

        self.log.info("Test gettxoutsetinfo hash_type option")
        # Adding hash_type 'hash_serialized_2', which is the default, should
        # not change the result.
        res4 = node.gettxoutsetinfo(hash_type='hash_serialized_2')
        del res4['disk_size']
        assert_equal(res, res4)

        # hash_type none should not return a UTXO set hash.
        res5 = node.gettxoutsetinfo(hash_type='none')
        assert 'hash_serialized_2' not in res5

        # hash_type muhash should return a different UTXO set hash.
        res6 = node.gettxoutsetinfo(hash_type='muhash')
        assert 'muhash' in res6
        assert res['hash_serialized_2'] != res6['muhash']

        # muhash should not be returned unless requested.
        for r in [res, res2, res3, res4, res5]:
            assert 'muhash' not in r

        # Unknown hash_type raises an error
        assert_raises_rpc_error(-8, "'foo hash' is not a valid hash_type", node.gettxoutsetinfo, "foo hash")

    def _test_getblockheader(self):
        self.log.info("Test getblockheader")
        node = self.nodes[0]

        assert_raises_rpc_error(-8, "hash must be of length 64 (not 8, for 'nonsense')", node.getblockheader, "nonsense")
        assert_raises_rpc_error(-8, "hash must be hexadecimal string (not 'ZZZ7bb8b1697ea987f3b223ba7819250cae33efacb068d23dc24859824a77844')", node.getblockheader, "ZZZ7bb8b1697ea987f3b223ba7819250cae33efacb068d23dc24859824a77844")
        assert_raises_rpc_error(-5, "Block not found", node.getblockheader, "0cf7bb8b1697ea987f3b223ba7819250cae33efacb068d23dc24859824a77844")

        besthash = node.getbestblockhash()
        secondbesthash = node.getblockhash(HEIGHT - 1)
        header = node.getblockheader(blockhash=besthash)

        assert_equal(header['hash'], besthash)
        assert 'pow_hash' in header
        assert_equal(header['height'], HEIGHT)
        assert_equal(header['confirmations'], 1)
        assert_equal(header['previousblockhash'], secondbesthash)
        assert_is_hex_string(header['chainwork'])
        assert_equal(header['nTx'], 1)
        assert_is_hash_string(header['hash'])
        assert_is_hash_string(header['pow_hash'])
        assert_is_hash_string(header['previousblockhash'])
        assert_is_hash_string(header['merkleroot'])
        assert_is_hash_string(header['bits'], length=None)
        assert isinstance(header['time'], int)
        assert_equal(header['mediantime'], self.time_range_mtp)
        assert isinstance(header['nonce'], int)
        assert isinstance(header['version'], int)
        assert isinstance(int(header['versionHex'], 16), int)
        assert isinstance(header['difficulty'], Decimal)

        # Test with verbose=False, which should return the header as hex.
        header_hex = node.getblockheader(blockhash=besthash, verbose=False)
        assert_is_hex_string(header_hex)

        header = from_hex(CBlockHeader(), header_hex)
        header.calc_sha256()
        assert_equal(header.hash, besthash)
        expected_pow_hash = rigor_pow_digest_hex(bytes.fromhex(header_hex))
        assert_equal(node.getblockheader(besthash)['pow_hash'], expected_pow_hash)

        assert 'previousblockhash' not in node.getblockheader(node.getblockhash(0))
        assert 'nextblockhash' not in node.getblockheader(node.getbestblockhash())

    def _test_getdifficulty(self):
        self.log.info("Test getdifficulty")
        difficulty = self.nodes[0].getdifficulty()
        # 1 hash in 2 should be valid, so difficulty should be 1/2**31
        # binary => decimal => binary math is why we do this check
        assert abs(difficulty * 2**31 - 1) < 0.0001

    def _test_getnetworkhashps(self):
        self.log.info("Test getnetworkhashps")
        hashes_per_second = self.nodes[0].getnetworkhashps()
        assert_raises_rpc_error(
            -3,
            textwrap.dedent("""
            Wrong type passed:
            {
                "Position 1 (nblocks)": "JSON value of type string is not of expected type number",
                "Position 2 (height)": "JSON value of type array is not of expected type number"
            }
            """).strip(),
            lambda: self.nodes[0].getnetworkhashps("a", []),
        )
        # This should be 2 hashes every 10 minutes or 1/300
        assert abs(hashes_per_second * 300 - 1) < 0.0001

    def _test_stopatheight(self):
        self.log.info("Test stopping at height")
        assert_equal(self.nodes[0].getblockcount(), HEIGHT)
        self.generate(self.wallet, 6)
        assert_equal(self.nodes[0].getblockcount(), HEIGHT + 6)
        self.log.debug('Node should not stop at this height')
        assert_raises(subprocess.TimeoutExpired, lambda: self.nodes[0].process.wait(timeout=3))
        try:
            self.generatetoaddress(self.nodes[0], 1, self.wallet.get_address(), sync_fun=self.no_op)
        except (ConnectionError, http.client.BadStatusLine):
            pass  # The node already shut down before response
        self.log.debug('Node should stop at this height...')
        self.nodes[0].wait_until_stopped()
        self.start_node(0)
        assert_equal(self.nodes[0].getblockcount(), HEIGHT + 7)

    def _test_waitforblockheight(self):
        self.log.info("Test waitforblockheight")
        node = self.nodes[0]
        peer = node.add_p2p_connection(P2PInterface())

        current_height = node.getblock(node.getbestblockhash())['height']

        # Create a fork somewhere below our current height, invalidate the tip
        # of that fork, and then ensure that waitforblockheight still
        # works as expected.
        #
        # (Previously this was broken based on setting
        # `rpc/blockchain.cpp:latestblock` incorrectly.)
        #
        fork_height = current_height - 100 # choose something vaguely near our tip
        fork_hash = node.getblockhash(fork_height)
        fork_block = node.getblock(fork_hash)

        def solve_and_send_block(prevhash, height, time):
            coinbase = create_coinbase(height, script_pubkey=CScript(self.wallet.get_scriptPubKey()))
            b = create_block(prevhash, coinbase, time)
            add_witness_commitment(b)
            b.solve()
            peer.send_and_ping(msg_block(b))
            return b

        b1 = solve_and_send_block(int(fork_hash, 16), fork_height+1, fork_block['time'] + 1)
        b2 = solve_and_send_block(b1.sha256, fork_height+2, b1.nTime + 1)

        node.invalidateblock(b2.hash)

        def assert_waitforheight(height, timeout=2):
            assert_equal(
                node.waitforblockheight(height=height, timeout=timeout)['height'],
                current_height)

        assert_waitforheight(0)
        assert_waitforheight(current_height - 1)
        assert_waitforheight(current_height)
        assert_waitforheight(current_height + 1)

    def _test_getblock(self):
        node = self.nodes[0]
        fee_per_byte = Decimal('0.00000010')
        fee_per_kb = 1000 * fee_per_byte

        self.wallet.send_self_transfer(fee_rate=fee_per_kb, from_node=node)
        blockhash = self.generate(node, 1)[0]

        def assert_hexblock_hashes(verbosity):
            block = node.getblock(blockhash, verbosity)
            assert_equal(blockhash, hash256(bytes.fromhex(block[:160]))[::-1].hex())

        def assert_fee_not_in_block(verbosity):
            block = node.getblock(blockhash, verbosity)
            assert 'fee' not in block['tx'][1]

        def assert_fee_in_block(verbosity):
            block = node.getblock(blockhash, verbosity)
            tx = block['tx'][1]
            assert 'fee' in tx
            # RIGOR fees are serialized wire bytes (no BIP141 witness discount).
            assert_equal(tx['fee'], tx['size'] * fee_per_byte)

        def assert_vin_contains_prevout(verbosity):
            block = node.getblock(blockhash, verbosity)
            tx = block["tx"][1]
            total_vin = Decimal("0.00000000")
            total_vout = Decimal("0.00000000")
            for vin in tx["vin"]:
                assert "prevout" in vin
                assert_equal(set(vin["prevout"].keys()), set(("value", "height", "generated", "scriptPubKey", "asset")))
                assert_equal(vin["prevout"]["generated"], True)
                total_vin += vin["prevout"]["value"]
            for vout in tx["vout"]:
                total_vout += vout["value"]
            assert_equal(total_vin, total_vout + tx["fee"])

        def assert_vin_does_not_contain_prevout(verbosity):
            block = node.getblock(blockhash, verbosity)
            tx = block["tx"][1]
            if isinstance(tx, str):
                # In verbosity level 1, only the transaction hashes are written
                pass
            else:
                for vin in tx["vin"]:
                    assert "prevout" not in vin

        self.log.info("Test that getblock with verbosity 0 hashes to expected value")
        assert_hexblock_hashes(0)
        assert_hexblock_hashes(False)

        self.log.info("Test that getblock with verbosity 1 doesn't include fee")
        assert_fee_not_in_block(1)
        assert_fee_not_in_block(True)

        block_v1 = node.getblock(blockhash, 1)
        assert 'pow_hash' in block_v1
        expected_pow_hash = rigor_pow_digest_hex(bytes.fromhex(node.getblockheader(blockhash, False)))
        assert_equal(block_v1['pow_hash'], expected_pow_hash)

        self.log.info('Test that getblock with verbosity 2 and 3 includes expected fee')
        assert_fee_in_block(2)
        assert_fee_in_block(3)

        self.log.info("Test that getblock with verbosity 1 and 2 does not include prevout")
        assert_vin_does_not_contain_prevout(1)
        assert_vin_does_not_contain_prevout(2)

        self.log.info("Test that getblock with verbosity 3 includes prevout")
        assert_vin_contains_prevout(3)

        self.log.info("Test that getblock with verbosity 2 and 3 still works with pruned Undo data")
        datadir = get_datadir_path(self.options.tmpdir, 0)

        self.log.info("Test getblock with invalid verbosity type returns proper error message")
        assert_raises_rpc_error(-3, "JSON value of type string is not of expected type number", node.getblock, blockhash, "2")

        def move_block_file(old, new):
            old_path = os.path.join(datadir, self.chain, 'blocks', old)
            new_path = os.path.join(datadir, self.chain, 'blocks', new)
            os.rename(old_path, new_path)

        # Move instead of deleting so we can restore chain state afterwards
        move_block_file('rev00000.dat', 'rev_wrong')

        assert_fee_not_in_block(2)
        assert_fee_not_in_block(3)
        assert_vin_does_not_contain_prevout(2)
        assert_vin_does_not_contain_prevout(3)

        # Restore chain state
        move_block_file('rev_wrong', 'rev00000.dat')

        assert 'previousblockhash' not in node.getblock(node.getblockhash(0))
        assert 'nextblockhash' not in node.getblock(node.getbestblockhash())


if __name__ == '__main__':
    BlockchainTest().main()
