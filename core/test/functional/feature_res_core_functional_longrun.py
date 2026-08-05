#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""RES core functional long-run / burn-in harness (multi-node, consensus-aligned).

This is **not** a naive ``SHA256(txid||block_hash)`` toy: winner selection and
``h`` values come from node RPC ``getconvergenceresult`` (same rules as
``feature_res_core_convergence_draw.py``).

Topology: star (B→A, C→A only). NODE-A mines convergence blocks; B/C sync.
Convergence txs use ``test_framework/rigor_block_builder.create_convergence_tx`` (same path as
``feature_res_core_convergence_draw.py``): in-process wallet RPC + ``prevtxs``, with spend/change
pinned to the ORUM pool address (avoids ``rigor-cli`` subprocess / JSON quirks that can leave
witnesses empty and surface ``Witness program hash mismatch``).

**Wallet hygiene:** this test overrides ``import_deterministic_coinbase_privkeys`` to **omit**
``importprivkey`` (framework default mixes deterministic keys with descriptor bech32 coinbases;
``listunspent`` can then pick an input whose keys do not match the signing path →
``Witness program hash mismatch`` / ``complete: false``). All mining uses one
``getnewaddress(bech32)`` pool so ORUM UTXOs are descriptor-homogeneous.

Environment (optional)::

    RES_LONGRUN_BLOCKS=5           # blocks to mine in the main loop
    RES_LONGRUN_TXS_PER_BLOCK=3    # convergence txs per block (>=2)
    RES_CAPTURE_DIR=...            # artifact root (default: tmpdir/RES_longrun)

Post-loop: restart NODE-B, ``-reindex-chainstate`` on NODE-C, re-sync, then
assert identical ``getconvergenceresult`` JSON and ``state_hash`` / RPL2
across all nodes for the recorded tip.
"""

from __future__ import annotations

import json
import os
import time
from decimal import Decimal

from test_framework.rigor_block_builder import create_convergence_tx
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

from feature_res_core_convergence_draw import (
    RES_REWARD_ATOMS,
    DEVTEST_MINING_RES_FEE_ADDR,
    _body_res_mint_rows,
    _pcorb_convergence_txid_hex_from_mint_tx,
    expected_draw_candidate_winner,
)

SYNC_TIMEOUT = 120


def _canonical_json(obj):
    if isinstance(obj, dict):
        return {k: _canonical_json(v) for k, v in obj.items()}
    if isinstance(obj, list):
        return [_canonical_json(v) for v in obj]
    if isinstance(obj, Decimal):
        return format(obj, "f")
    return obj


def _json_norm(obj) -> str:
    return json.dumps(_canonical_json(obj), sort_keys=True)


class RESCoreFunctionalLongRunTest(BitcoinTestFramework):
    def set_test_params(self):
        self.chain = "devtest"
        self.setup_clean_chain = True
        self.num_nodes = 3
        fee_arg = f"-miningresfeeaddr={DEVTEST_MINING_RES_FEE_ADDR}"
        self.extra_args = [[fee_arg], [fee_arg], [fee_arg]]
        self.wallet_names = ["default_wallet", False, False]
        self._requires_wallet = True

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def import_deterministic_coinbase_privkeys(self):
        """Descriptor wallet only: createwallet without ``importprivkey``.

        The default framework import adds deterministic mining keys alongside descriptor
        ``getnewaddress`` outputs; both may appear as P2WPKH ORU UTXOs, but signing can fail with
        ``Witness program hash mismatch`` when the wrong row is chosen first. Long-run mining uses
        only descriptor addresses (see ``run_test``).
        """
        for i in range(self.num_nodes):
            wn = (
                self.default_wallet_name
                if self.wallet_names is None
                else self.wallet_names[i]
                if i < len(self.wallet_names)
                else False
            )
            if wn is False:
                continue
            n = self.nodes[i]
            if wn is not None:
                n.createwallet(
                    wallet_name=wn,
                    descriptors=self.options.descriptors,
                    load_on_startup=True,
                )

    def setup_network(self):
        self.setup_nodes()
        self.connect_nodes(1, 0)
        self.connect_nodes(2, 0)
        self.sync_all()

    def _broadcast_convergence(self, node, *, orum_amt: str = "1.0") -> str:
        if orum_amt != "1.0":
            raise AssertionError("long-run helper currently supports 1.0 ORUM convergence only")
        tx = create_convergence_tx(
            node,
            Decimal(orum_amt),
            spend_addresses=[self._orum_pool_addr],
            change_address=self._orum_pool_addr,
        )
        raw_hex = tx.serialize_with_witness().hex()
        txid = node.sendrawtransaction(raw_hex, 0, orum_amt)
        meta = node.getmempoolentry(txid).get("convergence", {})
        assert meta.get("is_convergence", False)
        assert meta.get("meets_min_convergence", False)
        return txid

    def _mine_convergence_block(self, node, n_convergence: int) -> str:
        txids = []
        for _ in range(n_convergence):
            txids.append(self._broadcast_convergence(node))
        assert_equal(len(set(txids)), n_convergence)
        self.sync_mempools()
        mining_addr = self._orum_pool_addr
        out = self.generatetoaddress(node, 1, mining_addr)
        assert_equal(len(out), 1)
        block_hash = out[0]
        blk_tx = node.getblock(block_hash, 1)["tx"]
        for t in txids:
            assert t in blk_tx
        return block_hash

    def _drain_all(self):
        for n in self.nodes:
            n.syncwithvalidationinterfacequeue()

    def _wait_tri_replay_state_hash_agree(self, timeout: int = 120) -> None:
        """After IBD/restart, tip can match before validation queues drain (cf. C-15 in deterministic replay)."""

        deadline = time.time() + timeout
        last_hs = ("", "", "")
        while time.time() < deadline:
            self._drain_all()
            snaps = [n.getreplaysnapshot() for n in self.nodes]
            last_hs = tuple(s["state_hash"] for s in snaps)
            if last_hs[0] == last_hs[1] == last_hs[2]:
                return
            time.sleep(0.2)
        self._drain_all()
        snaps = [n.getreplaysnapshot() for n in self.nodes]
        last_hs = tuple(s["state_hash"] for s in snaps)
        tips = [n.getbestblockhash() for n in self.nodes]
        extra = " ".join(
            f"N{i}: h={snaps[i]['height']} mint={snaps[i]['mint_count']} utxo={snaps[i]['utxo_count']}"
            for i in range(3)
        )
        raise AssertionError(
            f"state_hash not aligned after {timeout}s "
            f"(A={last_hs[0]} B={last_hs[1]} C={last_hs[2]} tips={tips} {extra}). "
            "If you changed chainstate/index persistence, rebuild rigord/bitcoind "
            "(e.g. fix libbitcoinconsensus link, then `make -j$(nproc)`) before re-running this test."
        )

    def _replay_snap_subset(self, node):
        r = node.getreplaysnapshot()
        return {
            "height": r["height"],
            "block_hash": r["block_hash"],
            "state_hash": r["state_hash"],
            "mint_count": r["mint_count"],
            "utxo_count": r["utxo_count"],
            "res_balance": str(r["res_balance"]),
            "serialized_snapshot": r["serialized_snapshot"],
        }

    def _assert_tri_convergence_equal(self, label: str, block_hash: str):
        frs = [n.getconvergenceresult(block_hash) for n in self.nodes]
        n0 = _json_norm(frs[0])
        # test_framework.util.assert_equal(thing1, thing2, *args) requires every *arg to equal thing1;
        # do not pass a human message as a third argument.
        assert_equal(n0, _json_norm(frs[1]))
        assert_equal(n0, _json_norm(frs[2]))

    def _assert_tri_replay_equal(self, tag: str):
        self._drain_all()
        snaps = [self._replay_snap_subset(n) for n in self.nodes]
        assert_equal(snaps[0]["state_hash"], snaps[1]["state_hash"])
        assert_equal(snaps[0]["state_hash"], snaps[2]["state_hash"])
        assert_equal(
            snaps[0]["serialized_snapshot"],
            snaps[1]["serialized_snapshot"],
        )
        assert_equal(
            snaps[0]["serialized_snapshot"],
            snaps[2]["serialized_snapshot"],
        )

    def run_test(self):
        res_blocks = int(os.environ.get("RES_LONGRUN_BLOCKS", "5"))
        txs_per_block = int(os.environ.get("RES_LONGRUN_TXS_PER_BLOCK", "3"))
        assert txs_per_block >= 2, "convergence draw requires at least 2 root txs per block"

        capture_root = os.environ.get(
            "RES_CAPTURE_DIR",
            os.path.join(self.options.tmpdir, "RES_longrun"),
        )
        os.makedirs(capture_root, exist_ok=True)

        self.log.info(
            "RES long-run: %d blocks × %d convergence txs (capture %s)",
            res_blocks,
            txs_per_block,
            capture_root,
        )

        node_a = self.nodes[0]
        self._orum_pool_addr = node_a.getnewaddress(label="res-longrun-orum-pool", address_type="bech32")
        self.generatetoaddress(node_a, 130, self._orum_pool_addr)
        self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)

        recorded_hashes: list[str] = []

        for block_idx in range(res_blocks):
            self.log.info("Long-run block %s / %s", block_idx + 1, res_blocks)
            block_hash = self._mine_convergence_block(node_a, txs_per_block)
            self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)
            self._drain_all()

            self._assert_tri_convergence_equal(f"loop_{block_idx}", block_hash)
            fr = node_a.getconvergenceresult(block_hash)
            exp_w = expected_draw_candidate_winner(fr)
            rpc_w = fr.get("draw_winner_txid", "0" * 64).lower()
            # SECURITY-001-A8: the mint settling this draw lands in the next block, so extend the
            # chain by one empty block and look for it there.
            payout_hash = self.generatetoaddress(node_a, 1, self._orum_pool_addr)[0]
            self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)
            self._drain_all()
            assert_equal(_body_res_mint_rows(node_a, block_hash), [])
            if exp_w is not None:
                assert_equal(rpc_w, exp_w.lower())
                rows = _body_res_mint_rows(node_a, payout_hash)
                assert_equal(len(rows), 1)
                assert_equal(rows[0][1], RES_REWARD_ATOMS)
                assert_equal(_pcorb_convergence_txid_hex_from_mint_tx(node_a, rows[0][0], payout_hash), rpc_w)

            block_dir = os.path.join(capture_root, f"block_{block_idx + 1:04d}")
            os.makedirs(block_dir, exist_ok=True)

            winner_payload = {
                "block_hash": block_hash,
                "height": fr.get("height"),
                "convergence_root_set_size": fr.get("convergence_root_set_size"),
                "winner_txid_rpc": rpc_w,
                "winner_txid_expected_argmin": exp_w,
                "res_probability_target": fr.get("res_probability_target"),
                "sigma_seed": fr.get("sigma_seed"),
                "payout_height": fr.get("payout_height"),
                "payout_block_hash": payout_hash,
                "candidates": sorted(fr["candidates"], key=lambda c: c["txid"]),
            }
            with open(os.path.join(block_dir, "RES_winner.json"), "w", encoding="ascii") as f:
                json.dump(_canonical_json(winner_payload), f, indent=2, sort_keys=True)

            for idx, node in enumerate(self.nodes):
                label = chr(ord("A") + idx)
                with open(
                    os.path.join(block_dir, f"{label}_getblockchaininfo.json"),
                    "w",
                    encoding="ascii",
                ) as f:
                    json.dump(_canonical_json(node.getblockchaininfo()), f, indent=2, sort_keys=True)
                rpl2 = node.getreplaysnapshot()["serialized_snapshot"]
                with open(
                    os.path.join(block_dir, f"{label}_replaysnap_rpl2.hex"),
                    "w",
                    encoding="ascii",
                ) as hf:
                    hf.write(rpl2)

            recorded_hashes.append(block_hash)

        self.log.info("PHASE-2: restart NODE-B; -reindex-chainstate NODE-C; re-assert tri-equality")
        self._drain_all()
        self.restart_node(1)
        self.connect_nodes(1, 0)
        self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)
        self._drain_all()

        merged_c = list(self.extra_args[2]) + ["-reindex-chainstate"]
        self._drain_all()
        self.restart_node(2, extra_args=merged_c)
        self.connect_nodes(2, 0)
        self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)
        self._drain_all()

        tip = self.nodes[0].getbestblockhash()
        assert_equal(tip, self.nodes[1].getbestblockhash())
        assert_equal(tip, self.nodes[2].getbestblockhash())

        self._wait_tri_replay_state_hash_agree()

        for i, bh in enumerate(recorded_hashes):
            self._assert_tri_convergence_equal(f"post_restart_hist_{i}", bh)
        self._assert_tri_convergence_equal("post_restart_tip", tip)
        self._assert_tri_replay_equal("post_restart_tip")

        self.log.info("Artifacts under %s", capture_root)
        print("TASK-RES-CORE-FUNCTIONAL-LONGRUN: PASS", flush=True)


if __name__ == "__main__":
    RESCoreFunctionalLongRunTest().main()
