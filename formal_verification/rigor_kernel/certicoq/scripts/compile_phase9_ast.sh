#!/usr/bin/env bash
# Phase 9: compile clightgen AST + try transf_c_program in sidecar 8.19.
# Subshell only. Does not leak ccomp onto the host PATH.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=sidecar_lib.sh
. "${SCRIPT_DIR}/sidecar_lib.sh"

OPAMROOT="${CERTICOQ_ROOT}/.opam-root"
OPAMBIN="${CERTICOQ_ROOT}/bin/opam"
SWITCH=rigor-certicoq-8.19
GEN="${CERTICOQ_ROOT}/generated/clightgen-oru"
COMPCERT_R="${OPAMROOT}/${SWITCH}/lib/coq/user-contrib/compcert"
LOG="${GEN}/try_transf.log"

print_license_banner
assert_cone_coq_untouched
refuse_host_path_install

if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK: ccomp on host PATH" >&2
  exit 1
fi
if [ ! -f "${GEN}/check_oru_csyntax.v" ]; then
  echo "clightgen AST missing" >&2
  exit 1
fi

(
  export OPAMROOT
  export OPAMROOTISOK=1
  eval "$("${OPAMBIN}" env --switch="${SWITCH}" --set-switch)"
  if [ "$(command -v coqc)" = "/usr/bin/coqc" ]; then
    echo "SIDECAR_LEAK: Phase 9 AST used cone coqc" >&2
    exit 1
  fi
  echo "PHASE9_AST_COQC=$(command -v coqc)"
  echo "PHASE9_AST_COQ=$(coqc --print-version | awk '{print $1}')"
  cd "${GEN}"
  echo "PHASE9_AST_COMPILE=START"
  coqc -q -R "${COMPCERT_R}" compcert -R . CheckOruGen check_oru_csyntax.v
  echo "PHASE9_AST_COMPILE=PASS"
  coqc -q -R "${COMPCERT_R}" compcert -R . CheckOruGen TryTransf.v
  echo "PHASE9_TRYTRANSF_COMPILE=PASS"
  echo "PHASE9_VM_COMPUTE=START"
  # 3 minutes: if transf_c_program does not compute, record the gap.
  set +e
  timeout 180 coqtop -q -batch -R "${COMPCERT_R}" compcert -R . CheckOruGen \
    >"${LOG}" 2>&1 <<'EOF'
From CheckOruGen Require Import TryTransf.
Timeout 120 Eval vm_compute in compiled_attempt.
EOF
  RC=$?
  set -e
  echo "PHASE9_VM_COMPUTE_RC=${RC}"
  tail -30 "${LOG}" || true
)

echo "PARENT_CHECK: cone coqc still $(/usr/bin/coqc --print-version | awk '{print $1}')"
if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK: ccomp on host PATH after Phase 9 AST" >&2
  exit 1
fi
echo "PHASE9_AST_SCRIPT=DONE"
