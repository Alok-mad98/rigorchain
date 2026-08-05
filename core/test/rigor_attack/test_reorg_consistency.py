#!/usr/bin/env python3
"""Category E: reorg / multi-node — identical chain view => identical derived state."""

import unittest

from typing import List, Optional, Sequence, Tuple

from attack_sim import COIN, MAX_RES_MINT_SIM_SUPPLY, collect_convergence_winner_txid, assert_attack, verdict


convergenceTx = Tuple[bytes, int]
BlockSim = Tuple[Sequence[convergenceTx], bytes, int, bytes, int]


def winners_for_chain(blocks: Sequence[BlockSim]) -> List[Optional[bytes]]:
    out: List[Optional[bytes]] = []
    for convergence_txs, seed, height, T, rem_rng in blocks:
        out.append(collect_convergence_winner_txid(list(convergence_txs), seed, height, T, rem_rng))
    return out


class TestReorgConsistency(unittest.TestCase):
    """Adversary reorders history; honest replay of the same canonical chain must match."""

    def _demo_chain(self) -> List[BlockSim]:
        T = bytes.fromhex("ff" * 32)
        rem = MAX_RES_MINT_SIM_SUPPLY
        b1 = bytes.fromhex("01" * 32)
        b2 = bytes.fromhex("02" * 32)
        return [
            (
                [(bytes.fromhex("a1" * 32), 2 * COIN), (bytes.fromhex("a2" * 32), 2 * COIN + 1)],
                b1,
                10,
                T,
                rem,
            ),
            (
                [(bytes.fromhex("b1" * 32), 2 * COIN), (bytes.fromhex("b2" * 32), 2 * COIN + 1)],
                b2,
                11,
                T,
                rem,
            ),
        ]

    def test_replay_identical(self):
        chain = self._demo_chain()
        w1 = winners_for_chain(chain)
        w2 = winners_for_chain(chain)
        ok = w1 == w2
        assert_attack(ok, "non-deterministic winner derivation under identical chain replay")
        self.assertEqual(verdict(ok, "replay"), "PASS")

    def test_reorg_to_same_tip_matches(self):
        """Two views that share the same ordered tail produce the same suffix winners."""
        chain_a = self._demo_chain()
        # Prefix noise then same tail as chain_a (simulates resolved reorg to canonical tip).
        prefix: List[BlockSim] = [
            (
                [(bytes.fromhex("e1" * 32), 200_000_000), (bytes.fromhex("e2" * 32), 201_000_000)],
                bytes.fromhex("99" * 32),
                5,
                bytes.fromhex("ff" * 32),
                MAX_RES_MINT_SIM_SUPPLY,
            ),
        ]
        w_long = winners_for_chain(prefix + chain_a)[-len(chain_a) :]
        w_short = winners_for_chain(chain_a)
        ok = w_long == w_short
        assert_attack(
            ok,
            "reorg consistency failure: same tip blocks produced different winner suffix",
        )
        self.assertEqual(verdict(ok, "reorg"), "PASS")


if __name__ == "__main__":
    unittest.main()
