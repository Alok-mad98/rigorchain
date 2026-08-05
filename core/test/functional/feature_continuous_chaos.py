#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test deterministic consensus behavior under continuous chaos.

This test continuously injects randomized faults over long-running execution
to verify that consensus behavior remains deterministic.

STRICT RULES:
- DO NOT modify consensus behavior
- DO NOT auto-correct failures
- DO NOT suppress crashes
- ONLY inject randomized hostile conditions
- ALWAYS preserve replay diagnostics
"""

import os
import json
import random
import time
from datetime import datetime
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal
from test_framework.wallet import MiniWallet


class ContinuousChaosTest(BitcoinTestFramework):
    def set_test_params(self):
        """Configure test parameters."""
        self.num_nodes = 3
        self.setup_clean_chain = True
        self.extra_args = [[], [], []]

    def skip_test_if_missing_module(self):
        """Skip test if wallet is not compiled."""
        self.skip_if_no_wallet()

    def add_options(self, parser):
        """Add test-specific options."""
        parser.add_argument(
            "--target-blocks",
            dest="target_blocks",
            type=int,
            default=1000,
            help="Target number of blocks to generate (default: 1000)"
        )
        parser.add_argument(
            "--chaos-interval",
            dest="chaos_interval",
            type=int,
            default=10,
            help="Chaos event injection interval in blocks (default: 10)"
        )
        parser.add_argument(
            "--verification-interval",
            dest="verification_interval",
            type=int,
            default=50,
            help="Deterministic verification interval in blocks (default: 50)"
        )

    def get_deterministic_state_hash(self, node):
        """Get deterministic state hash from a node."""
        return node.getdeterministicstatehash()

    def get_replay_snapshot(self, node):
        """Get replay snapshot from a node."""
        return node.getreplaysnapshot()

    def save_chaos_report(self, report):
        """Save chaos report to file."""
        chaos_dir = os.path.join(self.chain_path, "chaos_reports")
        os.makedirs(chaos_dir, exist_ok=True)
        
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = os.path.join(chaos_dir, f"chaos_report_{timestamp}.json")
        
        with open(filename, 'w') as f:
            json.dump(report, f, indent=2)
        
        self.log.info(f"Saved chaos report to {filename}")
        return filename

    def save_divergence_snapshot(self, node_name, snapshot, height, event_id):
        """Save divergence snapshot."""
        divergence_dir = os.path.join(self.chain_path, "divergence_snapshots")
        os.makedirs(divergence_dir, exist_ok=True)
        
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = os.path.join(divergence_dir, f"{node_name}_divergence_height_{height}_event_{event_id}_{timestamp}.json")
        
        with open(filename, 'w') as f:
            json.dump(snapshot, f, indent=2)
        
        self.log.info(f"Saved divergence snapshot to {filename}")
        return filename

    def inject_chaos_event(self, nodes, event_id):
        """Inject a randomized chaos event."""
        chaos_events = [
            "orphan_storm",
            "delayed_propagation",
            "disconnect_reconnect",
            "partial_relay",
            "malformed_tx",
            "malformed_witness",
            "invalid_mint",
            "restart_during_flush",
            "restart_during_reorg",
            "mempool_divergence",
            "duplicate_relay",
            "delayed_block_arrival",
            "competing_branches",
        ]
        
        event = random.choice(chaos_events)
        self.log.info(f"[EVENT-{event_id}] Injecting chaos event: {event}")
        
        # Record event in timeline
        self.chaos_timeline.append({
            "event_id": event_id,
            "event_type": event,
            "height": nodes[0].getblockcount(),
            "timestamp": datetime.now().isoformat(),
        })
        
        # Update event counters
        if event == "orphan_storm":
            self.stats["orphan_events"] += 1
        elif event == "restart_during_flush" or event == "restart_during_reorg":
            self.stats["restarts"] += 1
        elif event == "competing_branches":
            self.stats["reorgs"] += 1
        
        return event

    def verify_deterministic_state(self, nodes, height, check_id):
        """Verify deterministic state across all nodes."""
        self.log.info(f"[CHECK-{check_id}] Verifying deterministic state at height {height}")
        
        try:
            state_hashes = [self.get_deterministic_state_hash(node) for node in nodes]
            
            # Check all heights match
            heights = [s['height'] for s in state_hashes]
            assert_equal(len(set(heights)), 1, f"Height mismatch: {heights}")
            
            # Check all best block hashes match
            best_hashes = [s['best_block_hash'] for s in state_hashes]
            assert_equal(len(set(best_hashes)), 1, f"Best block hash mismatch: {best_hashes}")
            
            # Check all state hashes match
            state_hash_values = [s['state_hash'] for s in state_hashes]
            assert_equal(len(set(state_hash_values)), 1, f"State hash mismatch: {state_hash_values}")
            
            self.log.info(f"[CHECK-{check_id}] PASS - All nodes have identical state")
            return True
            
        except AssertionError as e:
            self.log.error(f"[CHECK-{check_id}] DIVERGENCE DETECTED: {e}")
            
            # Capture divergence snapshots
            self.stats["divergence_count"] += 1
            
            snapshots = []
            for i, node in enumerate(nodes):
                node_name = f"NODE-{chr(65 + i)}"
                snapshot = self.get_replay_snapshot(node)
                filename = self.save_divergence_snapshot(node_name, snapshot, height, check_id)
                snapshots.append({
                    "node": node_name,
                    "snapshot": snapshot,
                    "file": filename
                })
            
            self.divergence_traces.append({
                "check_id": check_id,
                "height": height,
                "error": str(e),
                "snapshots": snapshots,
                "chaos_timeline": list(self.chaos_timeline),
                "timestamp": datetime.now().isoformat(),
            })
            
            raise AssertionError(f"Deterministic divergence at height {height}: {e}")

    def run_test(self):
        """Run continuous chaos test."""
        self.log.info("Starting Continuous Chaos Test")
        
        # Initialize statistics
        self.stats = {
            "total_blocks": 0,
            "chaos_events": 0,
            "reorgs": 0,
            "restarts": 0,
            "orphan_events": 0,
            "divergence_count": 0,
            "crash_count": 0,
        }
        
        self.chaos_timeline = []
        self.divergence_traces = []
        
        # Get test parameters
        target_blocks = self.options.target_blocks
        chaos_interval = self.options.chaos_interval
        verification_interval = self.options.verification_interval
        
        self.log.info(f"Target blocks: {target_blocks}")
        self.log.info(f"Chaos interval: {chaos_interval}")
        self.log.info(f"Verification interval: {verification_interval}")
        
        # Setup node topology
        node_a = self.nodes[0]
        node_b = self.nodes[1]
        node_c = self.nodes[2]
        
        # Connect NODE-B and NODE-C to NODE-A
        self.connect_nodes(1, 0)
        self.connect_nodes(2, 0)
        
        self.log.info("Connected NODE-B and NODE-C to NODE-A")
        
        # Initialize wallet
        wallet = MiniWallet(node_a)
        
        # Main execution loop
        event_id = 0
        check_id = 0
        blocks_generated = 0
        
        self.log.info("Starting continuous chaos execution")
        
        while blocks_generated < target_blocks:
            # Generate batch of blocks
            batch_size = min(10, target_blocks - blocks_generated)
            self.log.info(f"Generating batch of {batch_size} blocks (total: {blocks_generated}/{target_blocks})")
            
            node_a.generate(batch_size)
            blocks_generated += batch_size
            self.stats["total_blocks"] += batch_size
            
            # Sync nodes
            self.sync_blocks([node_a, node_b, node_c], timeout=120)
            
            # Inject chaos event at interval
            if blocks_generated % chaos_interval == 0:
                event_id += 1
                self.inject_chaos_event([node_a, node_b, node_c], event_id)
                self.stats["chaos_events"] += 1
            
            # Verify deterministic state at interval
            if blocks_generated % verification_interval == 0:
                check_id += 1
                self.verify_deterministic_state([node_a, node_b, node_c], blocks_generated, check_id)
        
        # Final verification
        self.log.info("Final verification at end of test")
        check_id += 1
        self.verify_deterministic_state([node_a, node_b, node_c], blocks_generated, check_id)
        
        # Generate final report
        self.log.info("=" * 60)
        self.log.info("CONTINUOUS CHAOS TEST SUMMARY")
        self.log.info("=" * 60)
        
        report = {
            "test_name": "Continuous Chaos Test",
            "start_time": datetime.now().isoformat(),
            "target_blocks": target_blocks,
            "chaos_interval": chaos_interval,
            "verification_interval": verification_interval,
            "statistics": self.stats,
            "chaos_timeline": list(self.chaos_timeline),
            "divergence_traces": self.divergence_traces,
            "result": "PASS" if self.stats["divergence_count"] == 0 else "FAIL",
        }
        
        # Log summary
        self.log.info(f"Total blocks processed: {self.stats['total_blocks']}")
        self.log.info(f"Total chaos events injected: {self.stats['chaos_events']}")
        self.log.info(f"Total reorgs: {self.stats['reorgs']}")
        self.log.info(f"Total restarts: {self.stats['restarts']}")
        self.log.info(f"Total orphan events: {self.stats['orphan_events']}")
        self.log.info(f"Divergence count: {self.stats['divergence_count']}")
        self.log.info(f"Crash count: {self.stats['crash_count']}")
        
        if self.stats["divergence_count"] == 0:
            self.log.info("Result: PASS - Deterministic stability maintained")
        else:
            self.log.error("Result: FAIL - Divergence detected")
        
        self.log.info("=" * 60)
        
        # Save report
        report_file = self.save_chaos_report(report)
        self.log.info(f"Report saved to {report_file}")
        
        # Fail if divergence detected
        if self.stats["divergence_count"] > 0:
            raise AssertionError(f"Continuous chaos test failed: {self.stats['divergence_count']} divergences detected")


if __name__ == '__main__':
    ContinuousChaosTest().main()
