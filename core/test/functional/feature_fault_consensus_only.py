#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Standalone entry: consensus fault injection (FAULT-003) only.

Runs ``FaultConsensusInjectionTest`` from ``feature_fault_injection_consensus.py``.
Full suite (FAULT-001 … FAULT-008) remains ``feature_fault_injection.py``.
"""

from feature_fault_injection_consensus import FaultConsensusInjectionTest

if __name__ == "__main__":
    FaultConsensusInjectionTest().main()
