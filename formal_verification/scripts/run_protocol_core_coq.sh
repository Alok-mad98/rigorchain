#!/usr/bin/env bash
# Compile the protocol-core Coq cone (Coq 8.15.x) and Print Assumptions
# on the load-bearing monetary / order-independence set with --strict.
#
# Not a C++ refinement. Honesty flags stay false.
# Do not apt-install Ubuntu `coq` — it is not pinned to 8.15.
#
# Usage:
#   bash formal_verification/scripts/run_protocol_core_coq.sh
#   bash formal_verification/scripts/run_protocol_core_coq.sh --fresh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
COQ_DIR="$ROOT/formal_verification/coq"
PROJECT="$COQ_DIR/_CoqProject.protocol-core"
FRESH=0
if [[ "${1:-}" == "--fresh" ]]; then
  FRESH=1
fi

ver="$(coqc --version | head -n1)"
echo "== ${ver} =="
case "${ver}" in
  *8.15.*) ;;
  *)
    echo "error: protocol-core requires Coq 8.15.x, got: ${ver}" >&2
    echo "Do not apt-install Ubuntu coq." >&2
    exit 1
    ;;
esac

cd "$COQ_DIR"
coq_makefile -f "$PROJECT" -o Makefile.protocol-core

# Shared .vo with the full-tree Makefile.coq. If that tree was built with a
# different Coq install, those objects are not reusable.
if [[ "${FRESH}" == "1" ]]; then
  echo "cleaning protocol-core objects (--fresh)"
  make -f Makefile.protocol-core clean
elif [[ -f Makefile.coq.conf ]]; then
  full_lib="$(grep -E '^COQMF_COQLIB=' Makefile.coq.conf | head -n1 || true)"
  core_lib="$(grep -E '^COQMF_COQLIB=' Makefile.protocol-core.conf | head -n1 || true)"
  if [[ -n "${full_lib}" && -n "${core_lib}" && "${full_lib}" != "${core_lib}" ]]; then
    echo "full-tree Coq install differs from protocol-core; cleaning shared .vo"
    echo "  ${full_lib}"
    echo "  ${core_lib}"
    make -f Makefile.protocol-core clean
  fi
fi

make -f Makefile.protocol-core -j"$(nproc)"

echo "== Print Assumptions (protocol-core load-bearing, --strict) =="
python3 "$ROOT/formal_verification/tools/audit_assumptions.py" \
  --project "$PROJECT" \
  --modules ResSchedule.v \
  --modules OrderIndependence.v \
  --modules ReplayReorgCore/ResMintObligation.v \
  --modules ReplayReorgCore/DualPoolConservation.v \
  --modules ReplayReorgCore/SupplyConservation.v \
  --modules ReplayReorgCore/OpaqueBoundary.v \
  --strict

echo "protocol-core Coq: PASS"
