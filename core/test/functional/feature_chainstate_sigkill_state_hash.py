#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""TASK-H2 H-3-2: deterministic state-hash crash recovery with mid-batch flush stress.

This closes the gap described in ``issues/H3-2-functional-mining-bootstrap.md``:
functional tests use the ``devtest`` chain (same consensus bind rules as regtest;
RFC-000 §5 bootstrap bind for heights 1..k-1) and the framework's deterministic
witness mining address so ``generatetoaddress`` can advance the tip past the
draw-bind floor without consensus shortcuts.

Scenario (single node):

  1. Start ``devtest`` with small ``-dbcache`` and small ``-dbbatchsize`` so
     ``CCoinsViewDB::BatchWrite`` is likely to perform mid-batch partial flushes
     (observable under ``-debug=coindb`` as ``BatchWrite produced N mid-batch …``).
  2. Mine ``TARGET_BLOCKS`` blocks (default 120, > RFC-000 k=10 and > original
     H-3-2 ~105 design point).
  3. Drain the validation interface queue, capture ``getdeterministicstatehash``.
  4. Force a chainstate flush via ``gettxoutsetinfo`` and assert at least one
     ``BatchWrite`` in this process produced ``N >= 1`` mid-batch partial flushes
     (H-3-2 acceptance criterion for the LevelDB envelope).
  5. ``SIGKILL`` the node (uncatchable; no shutdown hooks).
  6. Restart with identical args; assert height, best block hash, and
     ``state_hash`` match the pre-kill capture byte-for-byte.
"""

from __future__ import annotations

import re
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


# > CONVERGENCE_DRAW_BIND_DEPTH (10) and >= historical H-3-2 ~105 block design.
TARGET_BLOCKS = 120
# Small LevelDB write chunks to make mid-batch partial flushes likely during flush.
# Must be small vs serialized coin rows so one FlushStateToDisk spans multiple writes.
DB_BATCH_BYTES = 4096


def _max_mid_batch_partial_flushes_since(log_path: str, start_byte: int) -> int:
    """Scan new bytes in debug.log; return max observed mid-batch partial count."""
    max_n = 0
    pat = re.compile(rb"BatchWrite produced (\d+) mid-batch partial flush")
    with open(log_path, "rb") as f:
        f.seek(start_byte)
        chunk = f.read()
    for m in pat.finditer(chunk):
        max_n = max(max_n, int(m.group(1)))
    return max_n


class ChainstateSigkillStateHashTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.chain = "devtest"
        self.extra_args = [[
            "-dbcache=4",
            f"-dbbatchsize={DB_BATCH_BYTES}",
            "-debug=coindb",
        ]]

    def run_test(self):
        node = self.nodes[0]
        addr = node.get_deterministic_witness_mining_address()

        self.log.info("Mining %d blocks on devtest (deterministic witness coinbase).", TARGET_BLOCKS)
        self.generatetoaddress(node, TARGET_BLOCKS, addr)
        node.syncwithvalidationinterfacequeue()

        assert_equal(node.getblockcount(), TARGET_BLOCKS)

        log_start = node.debug_log_bytes()
        self.log.info("Forcing chainstate flush via gettxoutsetinfo (expect COINDB partial batches).")
        node.gettxoutsetinfo()
        node.syncwithvalidationinterfacequeue()

        partial_max = _max_mid_batch_partial_flushes_since(str(node.debug_log_path), log_start)
        self.log.info("Max mid-batch partial flush count observed this flush window: %d", partial_max)
        assert partial_max >= 1, (
            "Expected at least one BatchWrite mid-batch partial flush under -debug=coindb; "
            "got 0 — raise TARGET_BLOCKS or lower -dbbatchsize, or check COINDB logging."
        )

        # Sample authoritative state only after the flush and validation callbacks
        # have quiesced (same ordering discipline as feature_deterministic_replay.py C-15).
        pre = node.getdeterministicstatehash()
        self.log.info(
            "pre-kill state_hash=%s height=%d best=%s",
            pre["state_hash"],
            pre["height"],
            pre["best_block_hash"],
        )
        assert_equal(pre["height"], TARGET_BLOCKS)
        assert pre["best_block_hash"] != "0" * 64

        self.log.info("SIGKILL-ing node 0.")
        node.process.kill()
        node.process.wait(timeout=30)
        node.running = False
        node.process = None

        self.log.info("Restarting node 0 with identical args.")
        self.start_node(0, extra_args=self.extra_args[0])
        node.syncwithvalidationinterfacequeue()

        post = node.getdeterministicstatehash()
        self.log.info(
            "post-restart state_hash=%s height=%d best=%s",
            post["state_hash"],
            post["height"],
            post["best_block_hash"],
        )

        assert_equal(pre["height"], post["height"])
        assert_equal(pre["best_block_hash"], post["best_block_hash"])
        assert_equal(pre["state_hash"], post["state_hash"])


if __name__ == "__main__":
    ChainstateSigkillStateHashTest().main()
