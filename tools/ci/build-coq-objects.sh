#!/usr/bin/env bash
# Build protocol-core .vo objects on a clean checkout.
#
# Hosted runners do not have gitignored *.vo files. Extract / From-Require
# gates then fail with:
#   Cannot find a physical path bound to logical path ResSchedule with prefix Rigor
# Local developer trees hide this because .vo already exist.
#
# Coq 8.15 coqdep does not emit a usable graph here: a clean tree leaves
# Require-later modules (example: ScriptMintWitness.v ->
# Rigor.ReplayReorgCore.CvgCovenant) without .vo prerequisites. make -j
# races; make -j1 follows _CoqProject order, which is not topological.
#
# This script compiles with coqc until a fixed point: each pass builds every
# remaining .v whose imports already have .vo. Missing-path errors are
# retried on later passes. Real compile errors fail closed.
#
# CI infrastructure only. Does not modify Coq theorem statements.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
COQ_DIR="$ROOT/formal_verification/coq"
PROJECT="$COQ_DIR/_CoqProject.protocol-core"
COQLIBS=(-q -R . Rigor -R ../rigor_v01 RigorV01)

missing_path_error() {
  grep -Eq 'Cannot find a physical path bound to logical path|Cannot find library|Unable to locate library' "$1"
}

if ! command -v coqc >/dev/null 2>&1; then
  echo "CI_TOOLCHAIN_SETUP_FAILURE"
  echo "FAIL: coqc is not on PATH"
  exit 1
fi

cd "$COQ_DIR"
if [[ ! -f ResSchedule.v ]]; then
  echo "CI_TOOLCHAIN_SETUP_FAILURE"
  echo "FAIL: ResSchedule.v missing from checkout"
  ls -la | head
  exit 1
fi
if [[ ! -f "$PROJECT" ]]; then
  echo "CI_TOOLCHAIN_SETUP_FAILURE"
  echo "FAIL: missing ${PROJECT}"
  exit 1
fi

mapfile -t VFILES < <(grep -E '^[^#[:space:]].*\.v[[:space:]]*$' "$PROJECT" | tr -d '\r' | sed 's/[[:space:]]*$//')
if [[ "${#VFILES[@]}" -eq 0 ]]; then
  echo "CI_TOOLCHAIN_SETUP_FAILURE"
  echo "FAIL: no .v files listed in ${PROJECT}"
  exit 1
fi

echo "== build protocol-core Coq objects (${#VFILES[@]} files, fixed-point) =="

pass=0
while true; do
  pass=$((pass + 1))
  built=0
  pending=0
  last_err=""
  last_file=""
  for f in "${VFILES[@]}"; do
    vo="${f%.v}.vo"
    if [[ -f "${vo}" ]]; then
      continue
    fi
    pending=$((pending + 1))
    errf="$(mktemp)"
    if coqc "${COQLIBS[@]}" "${f}" >"${errf}" 2>&1; then
      echo "COQC ${f}"
      built=$((built + 1))
      rm -f "${errf}"
      continue
    fi
    if missing_path_error "${errf}"; then
      last_err="$(cat "${errf}")"
      last_file="${f}"
      rm -f "${errf}"
      continue
    fi
    echo "CI_TOOLCHAIN_SETUP_FAILURE"
    echo "FAIL: coqc ${f}"
    cat "${errf}"
    rm -f "${errf}"
    exit 1
  done

  remaining=0
  for f in "${VFILES[@]}"; do
    if [[ ! -f "${f%.v}.vo" ]]; then
      remaining=$((remaining + 1))
    fi
  done
  echo "COQ_OBJECTS_PASS=${pass} built=${built} remaining=${remaining}"

  if [[ "${remaining}" -eq 0 ]]; then
    break
  fi
  if [[ "${built}" -eq 0 ]]; then
    echo "CI_TOOLCHAIN_SETUP_FAILURE"
    echo "FAIL: no progress compiling protocol-core (stuck on ${last_file})"
    echo "${last_err}"
    exit 1
  fi
  if [[ "${pass}" -gt "${#VFILES[@]}" ]]; then
    echo "CI_TOOLCHAIN_SETUP_FAILURE"
    echo "FAIL: exceeded ${#VFILES[@]} compile passes"
    exit 1
  fi
done

if [[ ! -f ResSchedule.vo ]]; then
  echo "CI_TOOLCHAIN_SETUP_FAILURE"
  echo "FAIL: ResSchedule.vo was not produced"
  exit 1
fi
echo "COQ_OBJECTS=PASS"
