#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P0-042: GBT nBits/ASERT, miner SOLO e2e (≥10 blocks), reorg, timestamp, high-hash."""

import os
import subprocess
import time

from test_framework.blocktools import NORMAL_GBT_REQUEST_PARAMS
from test_framework.messages import CBlock, from_hex, uint256_from_compact
from test_framework.rigor_pow import header80_from_block, rigor_pow_digest_int
from test_framework.test_framework import BitcoinTestFramework, SkipTest
from test_framework.util import (
    assert_equal,
    get_auth_cookie,
    rpc_port,
)


def _minerd_path(config) -> str:
    srcdir = config["environment"]["SRCDIR"]
    exeext = config["environment"]["EXEEXT"]
    repo = os.path.abspath(os.path.join(srcdir, os.pardir))
    for name in ("minerd", "rigor-miner"):
        path = os.path.join(repo, "miner", name + exeext)
        if os.path.isfile(path) and os.access(path, os.X_OK):
            return path
    return ""


class MiningPowPipelineP0042Test(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.supports_cli = False

    def skip_test_if_missing_module(self):
        self.minerd = _minerd_path(self.config)
        if not self.minerd:
            raise SkipTest("miner/minerd not built")

    def _spawn_minerd(self, node, addr):
        user, password = get_auth_cookie(node.datadir, self.chain)
        url = f"http://127.0.0.1:{rpc_port(node.index)}"
        return subprocess.Popen(
            [
                self.minerd, "-a", "sha256d", "-q", "-t", "2", "--no-getwork",
                "--coinbase-addr", addr, "-o", url, "-u", user, "-p", password,
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )

    def run_test(self):
        node = self.nodes[0]
        addr = node.get_deterministic_witness_mining_address()

        genesis = node.getblock(node.getblockhash(0), 2)
        assert_equal(sum(v["value"] for v in genesis["tx"][0]["vout"]), 0)

        tmpl0 = node.getblocktemplate(NORMAL_GBT_REQUEST_PARAMS)
        assert tmpl0["rigor_coinbase_immutable"] is True
        assert "time" in tmpl0["mutable"]
        assert "coinbase" not in tmpl0["mutable"]
        assert tmpl0["mintime"] <= tmpl0["curtime"] <= tmpl0["maxtime"]
        tip0 = node.getblockheader(node.getbestblockhash())
        assert_equal(tmpl0["prev_nbits"], tip0["bits"])
        assert_equal(tmpl0["previousblockhash"], node.getbestblockhash())
        # Height-1 ASERT copies genesis nBits (startup gap is not an exponent).
        assert_equal(tmpl0["bits"], tip0["bits"])
        assert_equal(len(tmpl0["bits"]), 8)

        proc = self._spawn_minerd(node, addr)
        minerd_log = ""
        try:
            deadline = time.time() + 180 * self.options.timeout_factor
            while time.time() < deadline:
                if node.getblockcount() >= 10:
                    break
                if proc.poll() is not None:
                    out = proc.stdout.read().decode("utf-8", errors="replace") if proc.stdout else ""
                    raise AssertionError(f"minerd exited {proc.returncode}:\n{out}")
                time.sleep(0.2)
            else:
                if proc.poll() is None:
                    proc.terminate()
                    try:
                        minerd_log = proc.communicate(timeout=5)[0].decode("utf-8", errors="replace")
                    except subprocess.TimeoutExpired:
                        proc.kill()
                        minerd_log = proc.stdout.read().decode("utf-8", errors="replace") if proc.stdout else ""
                raise AssertionError(
                    f"minerd did not mine 10 blocks (height={node.getblockcount()})\n{minerd_log}"
                )
            assert node.getblockcount() >= 10
            self.log.info("SOLO e2e: minerd produced height=%d", node.getblockcount())
        finally:
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    proc.kill()

        # 32-block ASERT/GBT/validation identity (node generate uses same OpuHash+ASERT).
        for i in range(32):
            tip = node.getblockheader(node.getbestblockhash())
            tmpl = node.getblocktemplate(NORMAL_GBT_REQUEST_PARAMS)
            assert_equal(tmpl["previousblockhash"], node.getbestblockhash())
            assert_equal(tmpl["prev_nbits"], tip["bits"])
            assert_equal(tmpl["height"], tip["height"] + 1)
            assert tmpl["mintime"] <= tmpl["curtime"] <= tmpl["maxtime"]
            hashes = self.generatetoaddress(node, 1, addr)
            blk = node.getblock(hashes[0])
            assert_equal(blk["bits"], tmpl["bits"])
            assert_equal(blk["previousblockhash"], tmpl["previousblockhash"])
        self.log.info("32-block GBT.nBits == accepted block bits")

        # Reorg: drop last 3, GBT must follow the new canonical parent.
        height_before = node.getblockcount()
        stale = node.getblockhash(height_before - 2)
        node.invalidateblock(stale)
        assert node.getblockcount() == height_before - 3
        tip = node.getblockheader(node.getbestblockhash())
        tmpl = node.getblocktemplate(NORMAL_GBT_REQUEST_PARAMS)
        assert_equal(tmpl["previousblockhash"], node.getbestblockhash())
        assert_equal(tmpl["prev_nbits"], tip["bits"])
        # Fast SOLO blocks share the same wall-clock second. Advancing mock time
        # so generate cannot recreate the invalidated header hash (duplicate).
        node.setmocktime(tip["time"] + 600)
        tmpl = node.getblocktemplate(NORMAL_GBT_REQUEST_PARAMS)
        assert_equal(tmpl["previousblockhash"], node.getbestblockhash())
        hashes = self.generatetoaddress(node, 1, addr)
        blk = node.getblock(hashes[0])
        assert_equal(blk["bits"], tmpl["bits"])
        assert_equal(blk["previousblockhash"], tmpl["previousblockhash"])
        self.log.info("reorg GBT nBits follows new parent")

        # Illegal timestamp and invalid PoW on a solved template clone.
        tip = node.getbestblockhash()
        raw = node.getblock(tip, 0)
        prev = node.getblock(tip)["previousblockhash"]
        parent = node.getblockheader(prev)
        block = from_hex(CBlock(), raw)
        block.nTime = parent["mediantime"]
        block.solve()
        assert_equal(node.submitblock(block.serialize().hex()), "time-too-old")

        block = from_hex(CBlock(), raw)
        target = uint256_from_compact(block.nBits)
        found_invalid_pow = False
        start_nonce = block.nNonce
        for nonce in range(start_nonce + 1, start_nonce + 128):
            block.nNonce = nonce & 0xFFFFFFFF
            block.rehash()
            if rigor_pow_digest_int(header80_from_block(block)) > target:
                found_invalid_pow = True
                break
        assert found_invalid_pow, "could not find a high-hash nonce on this header"
        assert_equal(node.submitblock(block.serialize().hex()), "high-hash")
        self.log.info("TIME-01 / POW-06: illegal nTime and high-hash rejected")


if __name__ == "__main__":
    MiningPowPipelineP0042Test().main()
