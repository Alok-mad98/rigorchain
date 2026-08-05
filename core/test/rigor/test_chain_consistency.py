#!/usr/bin/env python3
import unittest

from rigor_model import convergenceCandidate, deterministic_reindex_result


class TestChainConsistency(unittest.TestCase):
    """Layer 5: chain consistency invariants only."""

    def test_reindex_and_two_nodes_produce_identical_results(self):
        convergence_sets = [
            [convergenceCandidate(bytes.fromhex("01" * 32)), convergenceCandidate(bytes.fromhex("02" * 32))],
            [convergenceCandidate(bytes.fromhex("03" * 32))],  # no winner (<2)
            [convergenceCandidate(bytes.fromhex("04" * 32)), convergenceCandidate(bytes.fromhex("05" * 32))],
        ]
        seeds = [bytes.fromhex("aa" * 32), bytes.fromhex("bb" * 32), bytes.fromhex("cc" * 32)]
        heights = [100, 101, 102]

        node_a = deterministic_reindex_result(convergence_sets, seeds, heights)
        node_b = deterministic_reindex_result(convergence_sets, seeds, heights)
        reindex_again = deterministic_reindex_result(convergence_sets, seeds, heights)

        self.assertEqual(node_a, node_b)
        self.assertEqual(node_a, reindex_again)


if __name__ == "__main__":
    unittest.main()
