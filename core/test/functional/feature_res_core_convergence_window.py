#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""SECURITY-001-A9-C: the draw's candidate set spans a sliding window of blocks.

WHAT WENT WRONG BEFORE THIS. SECURITY-001-A8 hardened the randomness SEED: sigma is
Hash256(txid || C || B_{h-1} || LE32(h)), so every input is fixed the moment block h-1 is found
and re-rolling the draw costs a proof-of-work solution. It left the candidate SET alone. The
producer of block h-1 still chose F unilaterally and at zero marginal cost, because excluding a
competitor's convergence transaction is free and no consensus rule can compel inclusion (a node
cannot prove what was in someone's mempool). With CONVERGENCE_DRAW_MIN_TXS_FOR_RES == 1, one
self-owned convergence was a valid draw, so a miner won every block he produced at the minimum
1 ORU burn and RES allocation tracked hashrate rather than burn. See
docs/security/TASK-SECURITY-001-A8-DESIGN-REVIEW.md finding F-1.

WHAT A-9-C CHANGES. F is enumerated over blocks [h-W, h-1] instead of h-1 alone. This does not
reduce the share of F a large miner controls -- with hashrate alpha he still produces about
alpha*W of the window. It removes EXCLUSIVITY: the candidates in [h-W, h-2] are already buried
and he cannot remove them, so he has to outbid candidates he cannot censor.

WHAT THIS TEST PINS, end to end against a real chain:

1. A convergence transaction remains a candidate in draws at later heights, in blocks it had no
   part in. This is the property a censoring miner used to be able to deny, and the direct
   observable consequence of the fix.
2. It leaves F once the window has passed it by, so the window slides rather than accumulating.
   An accumulating set would make old burns eligible forever.
3. |F| and C are functions of the window, so a block carrying no convergence of its own can
   still resolve a draw and pay a winner.
4. No convergence transaction is ever paid twice, even though it now sits in up to W draws.
5. Every reported candidate really is inside the window, and the node's winner agrees with an
   independent recomputation over the reported set.
"""

from __future__ import annotations

from decimal import Decimal

from test_framework.rigor_block_builder import create_convergence_tx
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than_or_equal

# Deterministic witness v1 RES receive for the miner's body mint (`-miningresfeeaddr`).
DEVTEST_MINING_RES_FEE_ADDR = "dvt1pqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqmsufsz"


def _txid_le_bytes(txid_hex: str) -> bytes:
    """RPC txids are big-endian display order; the tie-break compares raw bytes."""
    return bytes.fromhex(txid_hex)[::-1]


def _expected_winner(fr: dict) -> str | None:
    """Recompute the winner from the reported set: argmin sigma/f by cross-multiplication.

    Independent of the node's own selection loop on purpose. If this disagrees with
    ``draw_winner_txid`` then either the RPC is not reporting the set the draw ran over, or the
    selection rule is not what RFC-010 says it is.
    """
    eligible = [c for c in fr["candidates"] if c.get("draw_eligible")]
    if not eligible:
        return None
    best = None
    for c in eligible:
        sigma = int(c["h"], 16)
        weight = int(Decimal(str(c["convergence_amount"])) * 100_000_000)
        tid_le = _txid_le_bytes(c["txid"])
        if best is None:
            best = (sigma, weight, tid_le, c["txid"])
            continue
        b_sigma, b_weight, b_tid_le, _ = best
        # sigma_c / f_c < sigma_b / f_b, evaluated without division.
        lhs, rhs = sigma * b_weight, b_sigma * weight
        if lhs < rhs or (lhs == rhs and tid_le < b_tid_le):
            best = (sigma, weight, tid_le, c["txid"])
    return best[3]


class ResConvergenceWindowTest(BitcoinTestFramework):
    def set_test_params(self):
        self.chain = "devtest"
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [[f"-miningresfeeaddr={DEVTEST_MINING_RES_FEE_ADDR}"]]
        self.wallet_names = ["default_wallet"]
        self._requires_wallet = True

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    # -- helpers ---------------------------------------------------------------

    def _window_size(self, node) -> int:
        """Read W off the node rather than hardcoding it, so the test tracks the constant."""
        fr = node.getconvergenceresult(str(node.getblockcount()))
        return int(fr["window_size"])

    def _mine_with_convergence(self, node, count: int) -> tuple[str, list[str]]:
        """Mine one block carrying `count` fresh convergence transactions."""
        txids = []
        for _ in range(count):
            tx = create_convergence_tx(node, Decimal("1.0"))
            txids.append(node.sendrawtransaction(tx.serialize_with_witness().hex(), 0, "1.0"))
        block_hash = self.generatetoaddress(node, 1, self.mining_addr)[0]
        in_block = node.getblock(block_hash, 1)["tx"]
        for txid in txids:
            assert txid in in_block, f"convergence tx {txid} did not make it into the block"
        return block_hash, txids

    def _mine_empty(self, node, count: int) -> list[str]:
        """Mine `count` blocks with no convergence transactions of their own."""
        return self.generatetoaddress(node, count, self.mining_addr)

    def _draw(self, node, block_hash: str) -> dict:
        fr = node.getconvergenceresult(block_hash)
        assert_equal(fr["hash"], block_hash)
        # A-8 invariants the window must not disturb.
        assert_equal(fr["sigma_seed"], block_hash)
        assert_equal(int(fr["payout_height"]), int(fr["height"]) + 1)
        return fr

    def _candidate_txids(self, fr: dict) -> set[str]:
        return {c["txid"] for c in fr["candidates"]}

    def _check_window_self_consistency(self, fr: dict, label: str) -> None:
        """Every candidate must sit inside the reported window, and the winner must be derivable."""
        first, last = int(fr["window_first_height"]), int(fr["window_last_height"])
        assert_equal(last, int(fr["height"]))
        assert_equal(last - first + 1, min(int(fr["window_size"]), last + 1))
        for c in fr["candidates"]:
            src = int(c["source_height"])
            assert first <= src <= last, f"[{label}] candidate at height {src} outside [{first},{last}]"
        assert_equal(int(fr["convergence_root_set_size"]), len(fr["candidates"]))

        expected = _expected_winner(fr)
        reported = fr["draw_winner_txid"]
        if expected is None:
            assert_equal(reported, "0" * 64)
        else:
            assert_equal(reported, expected)

    # -- the test --------------------------------------------------------------

    def run_test(self):
        node = self.nodes[0]
        # `create_convergence_tx` funds itself from `listunspent`, so the subsidy has to land in
        # the wallet. The RES leg goes to `-miningresfeeaddr` regardless of who mines.
        self.mining_addr = node.getnewaddress(label="window-miner", address_type="bech32")

        # Devtest pays 1000 ORU per block and coinbases mature after 100, so 200 blocks leaves
        # roughly 100 spendable coinbases — far more than these 1 ORU convergences need.
        self._mine_empty(node, 200)
        window = self._window_size(node)
        assert_greater_than_or_equal(window, 2)
        self.log.info(f"draw window W = {window}")

        # --- 1. a buried convergence stays a candidate in later blocks' draws ---------------
        conv_block, conv_txids = self._mine_with_convergence(node, 1)
        conv_txid = conv_txids[0]
        conv_height = int(node.getblock(conv_block, 1)["height"])

        fr_at_source = self._draw(node, conv_block)
        assert conv_txid in self._candidate_txids(fr_at_source), "own-block candidate missing from F"
        self._check_window_self_consistency(fr_at_source, "source")

        # Mine blocks that contain no convergence at all. Before A-9-C each of those blocks
        # committed an empty F and the burn was simply forgotten, which is exactly what let a
        # miner censor a competitor for free. It must now still be a candidate.
        empty_hashes = self._mine_empty(node, window - 1)
        for offset, block_hash in enumerate(empty_hashes, start=1):
            fr = self._draw(node, block_hash)
            assert conv_txid in self._candidate_txids(fr), (
                f"buried convergence dropped out of F after {offset} empty block(s); "
                "a miner could censor it by mining an empty block"
            )
            self._check_window_self_consistency(fr, f"buried+{offset}")
            # A block with no convergence of its own still resolves a real draw.
            assert_greater_than_or_equal(int(fr["convergence_root_set_size"]), 1)

        # --- 2. the window slides: the candidate leaves once it is W blocks behind ----------
        last_including = empty_hashes[-1]
        assert_equal(int(node.getblock(last_including, 1)["height"]), conv_height + window - 1)

        past_hash = self._mine_empty(node, 1)[0]
        fr_past = self._draw(node, past_hash)
        assert conv_txid not in self._candidate_txids(fr_past), (
            "candidate outlived its window; F accumulates instead of sliding, which would make "
            "every past burn eligible forever"
        )
        self._check_window_self_consistency(fr_past, "past-window")
        assert_equal(int(fr_past["window_first_height"]), conv_height + 1)

        # --- 3. C is a function of the window, not of one block -----------------------------
        # C is the null root below two candidates, so two is the smallest set that produces a
        # visible one. Put both in a single block, then read the NEXT block, which carries no
        # convergence of its own: before A-9-C its F was empty and its C necessarily null.
        _, pair_txids = self._mine_with_convergence(node, 2)
        heir_hash = self._mine_empty(node, 1)[0]
        fr_heir = self._draw(node, heir_hash)
        self._check_window_self_consistency(fr_heir, "heir")
        assert set(pair_txids) <= self._candidate_txids(fr_heir), (
            "a block with no convergence of its own did not inherit the window's candidates"
        )
        assert fr_heir["convergence_root"] != "0" * 64, (
            "C is null in a block whose window holds two candidates, so C is still being taken "
            "over the block alone rather than over the window"
        )

        # --- 4. no convergence is ever paid twice ------------------------------------------
        # A candidate now sits in up to W draws, so a missing exclusion surfaces as one txid being
        # paid twice. Whether any draw resolves at all is probabilistic — the inequality is
        # H < T·f and these are minimum-size burns — so this walk is a live-chain cross-check on
        # top of the deterministic `a9c_already_paid_*` unit cases, not a substitute for them.
        for _ in range(4):
            self._mine_with_convergence(node, 2)
        self._mine_empty(node, window)

        paid: dict[str, int] = {}
        tip = node.getblockcount()
        for height in range(conv_height, tip + 1):
            block_hash = node.getblockhash(height)
            fr = self._draw(node, block_hash)
            self._check_window_self_consistency(fr, f"h{height}")
            winner = fr["draw_winner_txid"]
            if winner == "0" * 64:
                continue
            assert winner not in paid, (
                f"convergence {winner} was paid at height {paid[winner]} and again at "
                f"{int(fr['payout_height'])}; a candidate sits in up to W draws, so the "
                "no-double-win exclusion is not being applied"
            )
            paid[winner] = int(fr["payout_height"])

        self.log.info(f"{len(paid)} distinct convergence(s) paid across {tip - conv_height + 1} draws")

        # A winner must have been a candidate at the height that drew it, and the burn it was
        # paid for must have happened inside that draw's window.
        for winner_txid, payout_height in paid.items():
            fr = self._draw(node, node.getblockhash(payout_height - 1))
            match = [c for c in fr["candidates"] if c["txid"] == winner_txid]
            assert len(match) == 1, f"winner {winner_txid} not in the set that drew it"
            assert match[0]["draw_eligible"], f"winner {winner_txid} was not eligible"


if __name__ == "__main__":
    ResConvergenceWindowTest().main()
