#!/usr/bin/env bash
# Report, for every mutation, the full set of proofs it kills.
#
# The declared expected-failure list of a mutation is a claim. This
# derives the ground truth: apply the mutant, then repeatedly admit
# whichever proof fails and recompile until the library builds. What
# was admitted along the way is exactly what the mutant invalidates.
#
# Used to assign expected-failure lists from measurement, and by
# scripts/check_mutation_coverage.py to gate Category A coverage.
set -uo pipefail
cd "$(dirname "$0")/.."
export MUTATION_KILL_SET_MODE=1
for s in mutation/mutate_*.sh; do bash "$s"; done
