#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""TASK-RES-CONSENSUS-CORRECTNESS-SUITE: frozen RES convergence invariants (consensus-only path).

This is **not** a wallet UX test. It locks the RES convergence protocol surface that
``ListConvergenceRootCommitmentTxids`` / ``ConnectBlock`` / ``getconvergenceresult`` /
``getreplaysnapshot`` must keep bit-identical under replay.

Invariants exercised (devtest harness, ORUM-domain CVG2 convergence txs):

* **F gating:** Valid CVG2 convergence txs are counted in **F**; ``convergence_root_set_size``
  equals the number of ``candidates`` rows with ``convergence_type == v1_cvg2`` and positive
  ``convergence_amount`` (body RES mint rows are extra ``candidates`` with ``none`` — excluded from **F**).
* **Draw threshold:** ``|F| < 1`` ⇒ no draw winner (zero txid), no body RES mint,
  ``rigor_res_minted == 0`` (no phantom mint).
* **``|F| >= 1``:** deterministic argmin winner (Python cross-check vs
  ``draw_winner_txid``); **exactly one** body tx **at height + 1** carries
  ``RES_REWARD_ATOMS``; PCORB witness binds mint to winner; ``mint_count`` increases by
  at most one per such block (no duplicate mint in the harness path).
* **Deferred draw (SECURITY-001-A8):** a block AWARDS the draw over the set it commits
  and the block after it PAYS the winner, so the awarding block never mints. ``sigma_seed``
  is the awarding block's own hash, which is what makes re-rolling σ cost a proof of work.
* **Replay / multi-node:** follower ``state_hash`` and canonical **RPL2**
  ``serialized_snapshot`` match miner; after **restart** and **-reindex-chainstate** on
  the follower, tip ``getconvergenceresult`` JSON and snapshots stay byte-identical to
  the miner (``getdeterministicstatehash`` agrees with ``getreplaysnapshot`` on each node).

Complements ``feature_res_core_convergence_draw.py`` (single-node depth) and
``feature_res_core_functional_longrun.py`` (extended burn-in); this suite is the
compact **consensus freeze gate** for CI.
"""

from __future__ import annotations

import hashlib
import json
import os
import time
from decimal import Decimal

from test_framework.rigor_block_builder import create_convergence_tx
from test_framework.messages import ser_uint256
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than_or_equal

from feature_res_core_convergence_draw import (
    DEVTEST_MINING_RES_FEE_ADDR,
    RES_REWARD_ATOMS,
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


def _json_norm(obj: object) -> str:
    return json.dumps(_canonical_json(obj), sort_keys=True)


class RESConsensusCorrectnessSuiteTest(BitcoinTestFramework):
    def set_test_params(self):
        self.chain = "devtest"
        self.setup_clean_chain = True
        self.num_nodes = 2
        fee_arg = f"-miningresfeeaddr={DEVTEST_MINING_RES_FEE_ADDR}"
        self.extra_args = [[fee_arg], [fee_arg]]
        self.wallet_names = ["default_wallet", False]
        self._requires_wallet = True

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_network(self):
        self.setup_nodes()
        self.connect_nodes(1, 0)
        self.sync_all()

    def _run_convergence_tx_helper(self, *, orum: str = "1.0") -> dict:
        node = self.nodes[0]
        blockcount = node.getblockcount()
        tx = create_convergence_tx(node, Decimal(orum))
        raw_hex = tx.serialize_with_witness().hex()
        txid = node.sendrawtransaction(raw_hex, 0, orum)
        entry = node.getmempoolentry(txid)
        convergence_meta = entry.get("convergence", {})
        if not convergence_meta.get("is_convergence", False):
            raise AssertionError("tx not recognized as convergence by mempool metadata")
        if not convergence_meta.get("meets_min_convergence", False):
            raise AssertionError(
                "convergence amount below required threshold (meets_min_convergence false)"
            )
        prev = tx.vin[0].prevout
        utxo_txid = ser_uint256(prev.hash)[::-1].hex()
        conv_sats = tx.vout[0].nValue
        chg_sats = tx.vout[1].nValue if len(tx.vout) > 1 else 0
        return {
            "height": blockcount,
            "txid": txid,
            "utxo_txid": utxo_txid,
            "utxo_vout": int(prev.n),
            "convergence_sats": conv_sats,
            "change_sats": chg_sats,
        }

    def _mine_convergence_block(self, n_convergence: int) -> str:
        node = self.nodes[0]
        tx_infos = []
        for _ in range(n_convergence):
            tx_infos.append(self._run_convergence_tx_helper())
        txids = {t["txid"] for t in tx_infos}
        assert len(txids) == n_convergence, "convergence helper must yield distinct txids"
        self.sync_mempools()
        mining_addr = node.get_deterministic_witness_mining_address()
        out = self.generatetoaddress(node, 1, mining_addr)
        assert_equal(len(out), 1)
        block_hash = out[0]
        blk_tx = node.getblock(block_hash, 1)["tx"]
        for t in tx_infos:
            assert t["txid"] in blk_tx, f"expected convergence tx {t['txid']} in block"
        self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)
        return block_hash

    def _drain_all(self):
        for n in self.nodes:
            n.syncwithvalidationinterfacequeue()

    def _wait_pair_replay_agree(self, timeout: int = 120) -> None:
        deadline = time.time() + timeout
        while time.time() < deadline:
            self._drain_all()
            a = self.nodes[0].getreplaysnapshot()
            b = self.nodes[1].getreplaysnapshot()
            if (
                a["state_hash"] == b["state_hash"]
                and a["serialized_snapshot"] == b["serialized_snapshot"]
            ):
                return
            time.sleep(0.2)
        self._drain_all()
        a = self.nodes[0].getreplaysnapshot()
        b = self.nodes[1].getreplaysnapshot()
        raise AssertionError(
            "miner vs follower state_hash/RPL2 not aligned "
            f"(A_sh={a['state_hash']} B_sh={b['state_hash']} "
            f"same_rpl2={a['serialized_snapshot'] == b['serialized_snapshot']})"
        )

    def _assert_pair_convergence_rpc_equal(self, label: str, block_hash: str) -> None:
        fr0 = self.nodes[0].getconvergenceresult(block_hash)
        fr1 = self.nodes[1].getconvergenceresult(block_hash)
        if _json_norm(fr0) != _json_norm(fr1):
            raise AssertionError(f"[{label}] getconvergenceresult JSON mismatch miner vs follower")

    def _assert_f_matches_cvg2_candidates(self, label: str, fr: dict) -> None:
        """|F| equals the count of body txs that are CVG2 convergence (RPC rows).

        When a RES mint body tx is present it appears as an extra ``candidates`` row with
        ``convergence_type == none`` — it MUST NOT be counted in **F**."""
        n_f = int(fr["convergence_root_set_size"])
        cands = fr["candidates"]
        cvg2 = [
            c
            for c in cands
            if c.get("convergence_type") == "v1_cvg2"
            and Decimal(str(c.get("convergence_amount", "0"))) > Decimal(0)
        ]
        if n_f != len(cvg2):
            raise AssertionError(
                f"[{label}] |F|={n_f} must equal CVG2 convergence candidate rows={len(cvg2)}"
            )

    def _assert_draw_mint_invariants(self, label: str, block_hash: str) -> str:
        """Assert the draw `block_hash` awards, and the mint the NEXT block pays for it.

        SECURITY-001-A8: awarding and paying are one block apart, so this mines the payout
        block (empty mempool) and returns its hash.
        """
        self._drain_all()
        payout_hash = self.generatetoaddress(
            self.nodes[0], 1, self.nodes[0].get_deterministic_witness_mining_address()
        )[0]
        self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)
        self._drain_all()
        for n in self.nodes:
            self._assert_one_node_block_invariants(n, label, block_hash, payout_hash)
        self._assert_pair_convergence_rpc_equal(label, block_hash)
        self._wait_pair_replay_agree()
        return payout_hash

    def _assert_one_node_block_invariants(self, node, label: str, block_hash: str, payout_hash: str) -> None:
        fr = node.getconvergenceresult(block_hash)
        assert_equal(fr["hash"], block_hash)
        assert_equal(fr["sigma_seed"], block_hash)
        self._assert_f_matches_cvg2_candidates(label, fr)

        exp_winner = expected_draw_candidate_winner(fr)
        rpc_w = fr.get("draw_winner_txid", "0" * 64).lower()
        if exp_winner is None:
            assert rpc_w == "0" * 64, f"[{label}] no draw ⇒ zero winner txid"
        else:
            assert rpc_w == exp_winner.lower(), f"[{label}] deterministic winner mismatch"

        snap = node.getreplaysnapshot()
        det = node.getdeterministicstatehash()
        if snap["state_hash"] != det["state_hash"]:
            raise AssertionError(f"[{label}] replay vs deterministic state_hash mismatch")

        assert_equal(node.getblockheader(payout_hash)["previousblockhash"], block_hash)
        assert_equal(int(node.getblock(payout_hash, 1)["height"]), int(fr["payout_height"]))
        hdr = node.getblockheader(payout_hash)
        res_mint_rows = _body_res_mint_rows(node, payout_hash)

        if exp_winner is not None:
            assert_greater_than_or_equal(int(fr["convergence_root_set_size"]), 1)
            assert len(res_mint_rows) == 1, f"[{label}] exactly one coinbase OP_RES_MINT"
            mint_txid, mint_atoms = res_mint_rows[0]
            assert mint_atoms == RES_REWARD_ATOMS, f"[{label}] RES mint atoms"
            assert (
                _pcorb_convergence_txid_hex_from_mint_tx(node, mint_txid, payout_hash) == rpc_w
            ), f"[{label}] payout coinbase must pay the parent-draw winner"
            assert_equal(hdr.get("rigor_res_minted"), 1)
        else:
            assert res_mint_rows == [], f"[{label}] no phantom coinbase OP_RES_MINT"
            assert_equal(hdr.get("rigor_res_minted"), 0)

    def run_test(self):
        miner = self.nodes[0]
        self.log.info("Mature coinbase ORUM pool on miner (bech32)")
        mining_addr = miner.getnewaddress(label="res-cc-miner", address_type="bech32")
        self.generatetoaddress(miner, 130, mining_addr)
        self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)

        snap0 = miner.getreplaysnapshot()
        m0 = int(snap0["mint_count"])

        self.log.info("INV-1 |F|>=1: single convergence ⇒ draw + exactly one RES mint")
        self._run_convergence_tx_helper()
        self.sync_mempools()
        self.generatetoaddress(miner, 1, miner.get_deterministic_witness_mining_address())
        bh_lo = miner.getbestblockhash()
        self._assert_draw_mint_invariants("inv_f_eq_1", bh_lo)
        assert_equal(int(miner.getconvergenceresult(bh_lo)["convergence_root_set_size"]), 1)
        if expected_draw_candidate_winner(miner.getconvergenceresult(bh_lo)) is not None:
            assert_equal(int(miner.getreplaysnapshot()["mint_count"]), m0 + 1)

        self.log.info("INV-2 |F|>=2: draw + exactly one RES mint; cross-node RPC+RPL2 match")
        mb0 = int(miner.getreplaysnapshot()["mint_count"])
        bh_hi = self._mine_convergence_block(2)
        self._assert_draw_mint_invariants("inv_f_ge_2", bh_hi)
        if expected_draw_candidate_winner(miner.getconvergenceresult(bh_hi)) is not None:
            assert_equal(int(miner.getreplaysnapshot()["mint_count"]), mb0 + 1)

        self.log.info("INV-3 |F|>=3: scaled candidate set; still single mint if winner exists")
        bh_3 = self._mine_convergence_block(3)
        payout_3 = self._assert_draw_mint_invariants("inv_f_three", bh_3)
        fr3 = miner.getconvergenceresult(bh_3)
        if expected_draw_candidate_winner(fr3) is not None:
            assert_equal(len(_body_res_mint_rows(miner, payout_3)), 1)

        self.log.info("INV-4 follower restart + -reindex-chainstate: tip convergence + RPL2 frozen-identical")
        self._drain_all()
        tip = miner.getbestblockhash()
        rpl_a0 = miner.getreplaysnapshot()["serialized_snapshot"]
        rpl_b0 = self.nodes[1].getreplaysnapshot()["serialized_snapshot"]
        assert_equal(rpl_a0, rpl_b0)

        self.restart_node(1)
        self.connect_nodes(1, 0)
        self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)
        self._wait_pair_replay_agree()
        assert_equal(tip, self.nodes[1].getbestblockhash())
        self._assert_pair_convergence_rpc_equal("post_restart_follower", tip)
        assert_equal(miner.getreplaysnapshot()["serialized_snapshot"], self.nodes[1].getreplaysnapshot()["serialized_snapshot"])

        merged = list(self.extra_args[1]) + ["-reindex-chainstate"]
        self.restart_node(1, extra_args=merged)
        self.connect_nodes(1, 0)
        self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)
        self._wait_pair_replay_agree()
        assert_equal(tip, self.nodes[1].getbestblockhash())
        self._assert_pair_convergence_rpc_equal("post_reindex_follower", tip)
        assert_equal(miner.getreplaysnapshot()["serialized_snapshot"], self.nodes[1].getreplaysnapshot()["serialized_snapshot"])

        for bh in (bh_lo, bh_hi, bh_3, tip):
            self._assert_pair_convergence_rpc_equal(f"hist_{bh[:10]}", bh)

        out_dir = os.path.join(self.options.tmpdir, "TASK_RES_CONSENSUS_CORRECTNESS_SUITE")
        os.makedirs(out_dir, exist_ok=True)
        payload = {
            "tip": tip,
            "state_hash": miner.getreplaysnapshot()["state_hash"],
            "rpl2_sha256": hashlib.sha256(bytes.fromhex(rpl_a0)).hexdigest(),
            "recorded_blocks": {"f_lt_2": bh_lo, "f_ge_2": bh_hi, "f_three": bh_3},
        }
        with open(os.path.join(out_dir, "suite_manifest.json"), "w", encoding="ascii") as f:
            json.dump(payload, f, indent=2, sort_keys=True)

        self.log.info("Wrote %s", out_dir)
        print("TASK-RES-CONSENSUS-CORRECTNESS-SUITE: PASS", flush=True)


if __name__ == "__main__":
    RESConsensusCorrectnessSuiteTest().main()
