#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Long-chain stability probe (devtest): RES mint cadence, draw-target drift, getblock fields.

Default length (800 blocks after bootstrap) balances CI time with stable cadence-error stats; use ``--blocks 2000+`` locally for deeper runs.

Stability metrics (deterministic chain; draw outcomes are pseudorandom from consensus RES mint rules):
  * **volatility** = pstdev(draw_target) / mean(draw_target); require < 0.3
  * **drawdown** = max peak-to-trough drop of draw_target (fraction of running peak); require < 50%
  * **cadence_error** = |mint_count − n/6| / (n/6); require < 20%

Optional ``--csv`` writes per-block rows for plotting (height vs target / rolling mint rate).
Wire note: ``getblock`` still returns legacy JSON keys ``arda_target``, ``arda_orbis_minted``, ``arda_winner``; this script reads those keys verbatim.
"""

from __future__ import annotations

import csv
import statistics
from collections import Counter
from typing import List, Optional

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_greater_than


def uint256_rpc_hex_to_int_le(h: str) -> int:
    """RPC ``uint256`` display hex → integer (little-endian wire order)."""
    return int.from_bytes(bytes.fromhex(h), byteorder="little", signed=False)


def max_drawdown_fraction(values: List[int]) -> float:
    """Largest fractional drop from a running maximum (classic drawdown on the target series)."""
    if not values:
        return 0.0
    peak = values[0]
    worst = 0.0
    for x in values:
        if x > peak:
            peak = x
        if peak > 0:
            dd = (peak - x) / peak
            if dd > worst:
                worst = dd
    return worst


def rolling_sum_window(xs: List[int], win: int) -> List[int]:
    """For each index i, sum of the last ``win`` elements (partial prefix if shorter)."""
    out: List[int] = []
    acc = 0
    q: List[int] = []
    for v in xs:
        q.append(v)
        acc += v
        if len(q) > win:
            acc -= q.pop(0)
        out.append(acc)
    return out


class RigorLongRunTest(BitcoinTestFramework):
    def set_test_params(self):
        self.chain = "devtest"
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [[]]
        self.wallet_names = ["default_wallet"]
        self._requires_wallet = True

    def add_options(self, parser):
        self.add_wallet_options(parser)
        parser.add_argument(
            "--blocks",
            dest="long_blocks",
            type=int,
            default=800,
            help="Number of blocks to mine after the bootstrap window (default 800; try 2000+ locally)",
        )
        parser.add_argument(
            "--csv",
            dest="long_csv",
            type=str,
            default="",
            help="If set, write per-block CSV (height, draw_target_hex, res_mint_flag, roll60_mint_count, roll60_rate) to this path",
        )

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        node = self.nodes[0]
        n_blocks = int(self.options.long_blocks)
        if n_blocks < 200:
            raise AssertionError("--blocks must be at least 200 for meaningful stats")
        if n_blocks > 20_000:
            raise AssertionError("--blocks capped at 20000 for safety")

        mining_addr = node.get_deterministic_witness_mining_address()
        self.log.info("Bootstrap: mine past coinbase maturity (101 blocks)")
        self.generatetoaddress(node, 101, mining_addr)

        self.log.info("Mining %d blocks in batches of 10; sampling getblock(..., 1) per block", n_blocks)
        body_res_mints = 0
        bits_list: List[int] = []
        heights: List[int] = []
        target_ints: List[int] = []
        per_block_mint: List[int] = []
        target_hexes: List[str] = []
        winners: List[str] = []

        batch = 10
        mined = 0
        while mined < n_blocks:
            chunk = min(batch, n_blocks - mined)
            hashes = self.generatetoaddress(node, chunk, mining_addr)
            for h in hashes:
                b = node.getblock(h, 1)
                m = int(b.get("arda_orbis_minted", 0))
                body_res_mints += m
                bits_list.append(int(b["bits"], 16))
                heights.append(int(b["height"]))
                target_hexes.append(str(b["arda_target"]))
                target_ints.append(uint256_rpc_hex_to_int_le(b["arda_target"]))
                per_block_mint.append(m)
                winners.append(b.get("arda_winner", "00" * 32))
            mined += len(hashes)
            if mined % 200 == 0 or mined == n_blocks:
                self.log.info("Progress: %d / %d blocks", mined, n_blocks)

        rate = body_res_mints / float(n_blocks)
        self.log.info("RES mint blocks: %d / %d (rate %.4f, expected ~0.1667)", body_res_mints, n_blocks, rate)
        assert_greater_than(rate, 0.02)

        t_mean = statistics.mean(target_ints)
        t_std = statistics.pstdev(target_ints) if len(target_ints) > 1 else 0.0
        t_min, t_max = min(target_ints), max(target_ints)
        ratio_tm = (t_max / t_min) if t_min > 0 else 1.0
        volatility = (t_std / t_mean) if t_mean > 0 else 0.0
        drawdown = max_drawdown_fraction(target_ints)
        expected_mints = n_blocks / 6.0
        cadence_error = abs(body_res_mints - expected_mints) / expected_mints if expected_mints > 0 else 0.0

        self.log.info(
            "draw_target: mean=%.4e std=%.4e max/min=%.4f volatility(std/mean)=%.4f drawdown=%.4f",
            float(t_mean),
            float(t_std),
            ratio_tm,
            volatility,
            drawdown,
        )
        self.log.info(
            "cadence_error(|actual-expected|/expected)=%.4f (expected_mints=%.2f actual=%d)",
            cadence_error,
            expected_mints,
            body_res_mints,
        )
        self.log.info(
            "bits (compact): unique=%d min=0x%x max=0x%x",
            len(set(bits_list)),
            min(bits_list),
            max(bits_list),
        )

        assert volatility < 0.3, f"draw_target volatility too high: {volatility}"
        assert drawdown < 0.5, f"draw_target max drawdown too high: {drawdown}"
        # Binomial counting noise: default 800 blocks keeps |mints − n/6|/(n/6) < 0.2 under normal tails.
        assert cadence_error < 0.2, f"RES mint cadence error too high: {cadence_error}"

        # No pathological collapse / blow-up of T in this integer view
        assert ratio_tm < 1_000_000, f"draw_target max/min extreme: {ratio_tm}"

        csv_path: Optional[str] = (self.options.long_csv or "").strip()
        if csv_path:
            roll = rolling_sum_window(per_block_mint, 60)
            with open(csv_path, "w", newline="", encoding="utf-8") as fp:
                w = csv.writer(fp)
                w.writerow(["height", "draw_target_hex", "res_mint_flag", "roll60_mint_count", "roll60_rate"])
                for i, h in enumerate(heights):
                    c60 = roll[i]
                    w.writerow(
                        [
                            h,
                            target_hexes[i],
                            per_block_mint[i],
                            c60,
                            f"{(c60 / 60.0):.8f}",
                        ]
                    )
            self.log.info("Wrote per-block CSV to %s (roll60_mint_count = mints in last ≤60 blocks)", csv_path)

        nz_win = [w for w in winners if w != "00" * 32]
        if nz_win:
            c = Counter(nz_win)
            self.log.info("Non-zero winner samples (wire `arda_winner`): %d distinct txids in window", len(c))
            self.log.info("Top 5 winner txid counts: %s", c.most_common(5))

        self.log.info("Long-run observation script finished")


if __name__ == "__main__":
    RigorLongRunTest().main()
