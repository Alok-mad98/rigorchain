#!/usr/bin/env bash
# M26–M28 — ORU domain isolation / economic refinement
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# Fees mint an extra ORU term.
run_mutation M26 subsidy_is_only_creation_source Economic/ORU/ORUTransition.v \
  '       oru_stock pre + ovf_subsidy f + ovf_incoming f
         - ovf_outgoing f - ovf_burn f |}.' \
  '       oru_stock pre + ovf_subsidy f + ovf_incoming f
         - ovf_outgoing f - ovf_burn f + ovf_fees f |}.' \
  Economic/ORU/ORUSoundness || true

# ORU transfer secretly increases stock.
run_mutation M27 res_creation_separate_from_oru_transfer Economic/ORU/ORUTransition.v \
  '  ovf_incoming f - ovf_outgoing f - ovf_burn f.' \
  '  ovf_incoming f - ovf_outgoing f - ovf_burn f + 1.' \
  Economic/DomainSeparation || true

# Break economic refinement observation.
run_mutation M28 economic_refinement_preserves_conservation Refinement/EconomicState.v \
  '  ec_oru c = ea_oru a.' \
  '  ec_oru c <> ea_oru a.' \
  Refinement/EconomicRefinement || true
