#!/usr/bin/env bash
# Copyright (c) 2026 The RigorChain developers
# Local runner for the three Convergence consensus locks. No compile, no Coq.
# Same three locks as CI; CI splits path filters so docs do not run vectors.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
python3 "$ROOT/tools/check-convergence-identity.py"
python3 "$ROOT/tools/check-convergence-predicate-shape.py"
python3 "$ROOT/tools/check-publication-boundary.py"
echo "CONSENSUS_LOCK=PASS"
