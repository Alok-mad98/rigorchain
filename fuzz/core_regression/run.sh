#!/usr/bin/env bash
# Core libFuzzer targets (core/src/test/fuzz) in two stages.
#
#   smoke     every target: replay inputs/<target>/*, then fuzz SMOKE_SECONDS (default 60)
#   extended  one target (TARGET=...): replay, then fuzz EXTENDED_SECONDS (default 1800)
#
# Usage: run.sh <smoke|extended> <path-to-fuzz-binary> [corpus-dir]
# Any non-zero exit from the fuzz binary fails the run. Artifacts go to $OUT/artifacts.
#
# ASan binaries crash at startup, before any input runs, on kernels with
# vm.mmap_rnd_bits > 28. Lower it (CI does), or set FUZZ_WRAPPER="setarch x86_64 -R"
# where sysctl is not available.
set -euo pipefail

STAGE="${1:?stage: smoke|extended}"
FUZZ_BIN="${2:?path to fuzz binary}"
CORPUS_ROOT="${3:-}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${OUT:-${TMPDIR:-/tmp}/rigor-core-fuzz}"
SMOKE_SECONDS="${SMOKE_SECONDS:-60}"
EXTENDED_SECONDS="${EXTENDED_SECONDS:-1800}"

TARGETS=(
  pow
  pow_transition
  rigor_asert
  rigor_pow_target
  rigor_opuhash_header
  rigor_scriptpubkey
  rigor_mint
  rigor_supply
  rigor_reorg
  res_checktxinputs
  rigor_p2p_capability
  process_message_rigorcap
)

[[ -x "${FUZZ_BIN}" ]] || { echo "FUZZ_STATUS=FAIL missing binary ${FUZZ_BIN}"; exit 1; }
read -r -a WRAP <<< "${FUZZ_WRAPPER:-}"
mkdir -p "${OUT}/artifacts" "${OUT}/corpus"

run_target() {
  local t="$1" seconds="$2"
  local art="${OUT}/artifacts/${t}/" corpus="${OUT}/corpus/${t}"
  mkdir -p "${art}" "${corpus}"
  if [[ -n "${CORPUS_ROOT}" && -d "${CORPUS_ROOT}/${t}" ]]; then
    cp -n "${CORPUS_ROOT}/${t}"/* "${corpus}/" 2>/dev/null || true
  fi
  if compgen -G "${HERE}/inputs/${t}/*" >/dev/null; then
    echo "== ${t}: replay regression inputs"
    FUZZ="${t}" "${WRAP[@]}" "${FUZZ_BIN}" -runs=0 "${HERE}/inputs/${t}"/*
    cp -n "${HERE}/inputs/${t}"/* "${corpus}/"
  fi
  echo "== ${t}: fuzz ${seconds}s"
  set +e
  FUZZ="${t}" "${WRAP[@]}" "${FUZZ_BIN}" -max_total_time="${seconds}" -artifact_prefix="${art}" \
    -print_final_stats=1 "${corpus}" > "${OUT}/${t}.log" 2>&1
  local rc=$?
  set -e
  tail -n 20 "${OUT}/${t}.log"
  if [[ ${rc} -ne 0 ]]; then
    echo "FUZZ_STATUS=FAIL target=${t} rc=${rc}"
    return 1
  fi
  echo "FUZZ_TARGET_PASS=${t} $(grep -m1 -o 'stat::number_of_executed_units: [0-9]*' "${OUT}/${t}.log" || true)"
}

case "${STAGE}" in
  smoke)
    for t in "${TARGETS[@]}"; do run_target "${t}" "${SMOKE_SECONDS}"; done
    ;;
  extended)
    run_target "${TARGET:?TARGET required for extended stage}" "${EXTENDED_SECONDS}"
    ;;
  list)
    printf '%s\n' "${TARGETS[@]}"
    exit 0
    ;;
  *)
    echo "unknown stage ${STAGE}"; exit 2
    ;;
esac
echo "FUZZ_STATUS=PASS stage=${STAGE}"
