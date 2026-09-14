#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P041: rigor-miner copy-bytes GBT → SHA3-256 RGRPOW01 → submitblock against a live node.

Does not change consensus. Skips if miner/minerd is not built.
Mines heights 1, 2, 3 sequentially and checks mined coinbase == GBT
default_coinbase_tx. Also submits mutated templates that the node must reject.
"""

import copy
import os
import subprocess
import time

from test_framework.messages import CBlock, CTransaction, from_hex, uint256_from_compact
from test_framework.rigor_pow import header80_from_block, rigor_pow_digest_int
from test_framework.test_framework import BitcoinTestFramework, SkipTest
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
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


class MiningSoloGbtCopyBytesTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.supports_cli = False

    def skip_test_if_missing_module(self):
        self.minerd = _minerd_path(self.config)
        if not self.minerd:
            raise SkipTest("miner/minerd not built; configure && make in miner/")

    def gbt(self, node, addr):
        return node.getblocktemplate({"rules": ["segwit"], "coinbase_address": addr})

    def block_from_gbt(self, tmpl):
        cb = from_hex(CTransaction(), tmpl["default_coinbase_tx"])
        block = CBlock()
        block.nVersion = tmpl["version"]
        block.hashPrevBlock = int(tmpl["previousblockhash"], 16)
        block.nTime = tmpl["curtime"]
        block.nBits = int(tmpl["bits"], 16)
        block.nNonce = 0
        block.vtx = [cb]
        for tx in tmpl["transactions"]:
            block.vtx.append(from_hex(CTransaction(), tx["data"]))
        block.hashMerkleRoot = block.calc_merkle_root()
        return block

    def spawn_minerd(self, node, addr):
        user, password = get_auth_cookie(node.datadir, self.chain)
        url = f"http://127.0.0.1:{rpc_port(node.index)}"
        return subprocess.Popen(
            [
                self.minerd,
                "-a", "rgrpow01",
                "-q",
                "-t", "1",
                "--submit-delay", "2",
                "--no-getwork",
                "--coinbase-addr", addr,
                "-o", url,
                "-u", user,
                "-p", password,
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )

    def stop_minerd(self, proc):
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()

    def verify_mined(self, node, expect_height, tmpl):
        h = node.getblockhash(expect_height)
        blk = node.getblock(h, 2)
        raw = node.getblock(h, 0)
        block = from_hex(CBlock(), raw)
        mined_cb = block.vtx[0].serialize().hex()
        assert_equal(mined_cb, tmpl["default_coinbase_tx"])
        assert_equal(blk["merkleroot"], tmpl["merkleroot"])
        assert_equal(blk["previousblockhash"], tmpl["previousblockhash"])
        assert_equal(blk["version"], tmpl["version"])
        assert_equal(blk["bits"], tmpl["bits"])
        assert_equal(blk["tx"][0]["vin"][0]["coinbase"], tmpl["default_coinbase_scriptsig"])
        assert_equal(blk["tx"][0]["txid"], tmpl["default_coinbase_txid"])
        wit = blk["tx"][0]["vin"][0].get("txinwitness", [])
        assert_equal(len(wit), 1)
        assert_equal(wit[0], "00" * 32)
        commit = tmpl["default_witness_commitment"]
        assert any(v["scriptPubKey"]["hex"] == commit for v in blk["tx"][0]["vout"])
        assert "default_res_mint" not in tmpl
        assert not any(v["scriptPubKey"]["hex"].endswith("bb") for v in blk["tx"][0]["vout"])
        self.log.info(
            "SOLO accepted height=%s prev=%s bits=%s time=%s nonce=%s merkle=%s cb=%s oru=%s hash=%s",
            expect_height,
            blk["previousblockhash"],
            blk["bits"],
            blk["time"],
            blk["nonce"],
            blk["merkleroot"],
            blk["tx"][0]["txid"],
            tmpl["coinbasevalue"],
            h,
        )
        return {
            "height": expect_height,
            "prevhash": blk["previousblockhash"],
            "nBits": blk["bits"],
            "nTime": blk["time"],
            "nonce": blk["nonce"],
            "merkle": blk["merkleroot"],
            "coinbase_txid": blk["tx"][0]["txid"],
            "oru_subsidy": tmpl["coinbasevalue"],
            "hash": h,
        }

    def mine_next(self, node, addr):
        """Snapshot the template with the miner stopped, mine exactly that height, verify.

        Taking the GBT while nothing is mining removes any dependency on how fast
        minerd runs: the snapshot always belongs to the height we then verify.
        """
        tmpl = self.gbt(node, addr)
        height = tmpl["height"]
        assert_equal(height, node.getblockcount() + 1)
        timeout = 90 * self.options.timeout_factor
        proc = self.spawn_minerd(node, addr)
        try:
            deadline = time.time() + timeout
            while node.getblockcount() < height:
                if proc.poll() is not None:
                    out = proc.stdout.read().decode("utf-8", errors="replace") if proc.stdout else ""
                    raise AssertionError(f"minerd exited {proc.returncode}:\n{out}")
                if time.time() > deadline:
                    raise AssertionError(f"minerd did not reach height {height} in {timeout}s")
                time.sleep(0.05)
        finally:
            self.stop_minerd(proc)
        return self.verify_mined(node, height, tmpl)

    def reject_mutations(self, node, addr):
        tmpl = self.gbt(node, addr)
        assert_equal(tmpl["coinbase_address"], addr)
        base = self.block_from_gbt(tmpl)
        assert_equal(
            f"{base.hashMerkleRoot:064x}",
            tmpl["merkleroot"],
        )

        high = copy.deepcopy(base)
        target = uint256_from_compact(high.nBits)
        found = False
        for nonce in range(1, 4096):
            high.nNonce = nonce
            high.rehash()
            if rigor_pow_digest_int(header80_from_block(high)) > target:
                found = True
                break
        assert found, "could not find a high-hash nonce"
        assert_equal(node.submitblock(high.serialize().hex()), "high-hash")

        bits = copy.deepcopy(base)
        # Keep an easy compact target so solve() finishes; only the compact must differ from GBT/ASERT.
        bits.nBits = 0x207ffffe
        bits.solve()
        assert_equal(node.submitblock(bits.serialize().hex()), "bad-diffbits")

        malformed = base.serialize().hex()[:-8]
        assert_raises_rpc_error(-22, "Block decode failed", node.submitblock, malformed)

        bad_h = copy.deepcopy(base)
        ss = bytearray(bad_h.vtx[0].vin[0].scriptSig)
        assert len(ss) >= 2
        ss[0] = 0x52  # OP_2, height prefix no longer matches
        bad_h.vtx[0].vin[0].scriptSig = bytes(ss)
        bad_h.vtx[0].rehash()
        bad_h.hashMerkleRoot = bad_h.calc_merkle_root()
        bad_h.solve()
        assert_equal(node.submitblock(bad_h.serialize().hex()), "bad-cb-height")

        wit = copy.deepcopy(base)
        commit = bytes.fromhex(tmpl["default_witness_commitment"])
        found_commit = False
        for i, out in enumerate(wit.vtx[0].vout):
            if out.scriptPubKey == commit:
                spk = bytearray(out.scriptPubKey)
                spk[-1] ^= 0x01
                wit.vtx[0].vout[i].scriptPubKey = bytes(spk)
                found_commit = True
                break
        assert found_commit
        wit.vtx[0].rehash()
        wit.hashMerkleRoot = wit.calc_merkle_root()
        wit.solve()
        assert_equal(node.submitblock(wit.serialize().hex()), "bad-witness-merkle-match")

        assert "default_res_mint" not in tmpl
        self.log.info("RFC-019 GBT has no coinbase RES mint; PoW does not create RES")

        assert_equal(node.getblockcount(), 0)

    def run_test(self):
        node = self.nodes[0]
        genesis = node.getblock(node.getblockhash(0), 2)
        genesis_cb = genesis["tx"][0]
        genesis_value = sum(vout["value"] for vout in genesis_cb["vout"])
        assert_equal(genesis_value, 0)
        self.log.info("Genesis height 0 ORU subsidy is 0")

        addr = node.get_deterministic_witness_mining_address()
        tmpl = self.gbt(node, addr)
        for key in (
            "default_coinbase_scriptsig",
            "default_coinbase_tx",
            "default_coinbase_txid",
            "merkleroot",
            "rigor_coinbase_immutable",
            "default_witness_commitment",
            "transactions",
            "coinbasevalue",
            "rules",
            "version",
            "previousblockhash",
            "curtime",
            "bits",
            "target",
            "height",
            "coinbase_address",
        ):
            assert key in tmpl, f"GBT missing {key}"
        for retired in ("paid_winner_txid", "convergence_root", "default_res_mint"):
            assert retired not in tmpl, f"RFC-019 GBT must not serve {retired}"
        assert tmpl["rigor_coinbase_immutable"] is True
        assert "segwit" in tmpl["rules"] or "!segwit" in tmpl["rules"]
        assert_equal(tmpl["height"], 1)
        assert_equal(tmpl["coinbase_address"], addr)
        ss = bytes.fromhex(tmpl["default_coinbase_scriptsig"])
        assert_greater_than(len(ss), 1)
        assert ss[0] == 0x51  # height 1 = OP_1
        cb = bytes.fromhex(tmpl["default_coinbase_tx"])
        assert bytes.fromhex(tmpl["default_coinbase_scriptsig"]) in cb
        assert bytes.fromhex(tmpl["default_witness_commitment"]) in cb
        assert b"RIGR" not in bytes.fromhex(tmpl["default_coinbase_scriptsig"])

        self.reject_mutations(node, addr)

        log = [self.mine_next(node, addr) for _ in range(3)]
        heights = [entry["height"] for entry in log]
        assert_equal(heights[0], 1)
        assert heights == sorted(set(heights)), f"heights not strictly increasing: {heights}"
        assert_greater_than(node.getblockcount(), 2)
        self.log.info("continuous SOLO heights %s: %s", heights, log)


if __name__ == "__main__":
    MiningSoloGbtCopyBytesTest().main()
