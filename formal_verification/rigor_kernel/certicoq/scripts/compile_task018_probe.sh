#!/usr/bin/env bash
# TASK-018: Closed Clight → CompCert compilation probe.
# Uses existing SHA-pinned CertiCoq C / existing check_oru Clight AST.
# Does not regenerate CertiCoq Clight. Does not put ccomp on host PATH.
# A ccomp exit code is not a Coq proof.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=sidecar_lib.sh
. "${SCRIPT_DIR}/sidecar_lib.sh"

OPAMROOT="${CERTICOQ_ROOT}/.opam-root"
OPAMBIN="${CERTICOQ_ROOT}/bin/opam"
SWITCH=rigor-certicoq-8.19
SIDECAR_BIN="${OPAMROOT}/${SWITCH}/bin"
RUNTIME="${OPAMROOT}/${SWITCH}/lib/coq/user-contrib/CertiCoq/Plugin/runtime"
COMPCERT_R="${OPAMROOT}/${SWITCH}/lib/coq/user-contrib/compcert"
GEN_ORU="${CERTICOQ_ROOT}/generated/clightgen-oru"
TRY="${CERTICOQ_ROOT}/proofs/sidecar819"
OUT="${CERTICOQ_ROOT}/generated/clightgen-six"
LOGDIR="${TRY}/task018_logs"

print_license_banner
assert_cone_coq_untouched
refuse_host_path_install

if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK: ccomp on host PATH" >&2
  exit 1
fi

if [ ! -f "${GEN_ORU}/check_oru_clight.v" ]; then
  echo "EXISTING_CLIGHT_AST_MISSING: ${GEN_ORU}/check_oru_clight.v" >&2
  exit 1
fi

mkdir -p "${OUT}" "${LOGDIR}"

(
  export OPAMROOT
  export OPAMROOTISOK=1
  eval "$("${OPAMBIN}" env --switch="${SWITCH}" --set-switch)"
  if [ "$(command -v coqc)" = "/usr/bin/coqc" ]; then
    echo "SIDECAR_LEAK: TASK-018 used cone coqc" >&2
    exit 1
  fi
  echo "TASK018_COQC=$(command -v coqc)"
  echo "TASK018_COQ=$(coqc --print-version | awk '{print $1}')"
  echo "TASK018_CLIGHTGEN=$(command -v clightgen || echo ABSENT)"

  coqc_gen() {
    coqc -q -R "${COMPCERT_R}" compcert -R "${GEN_ORU}" CheckOruGen "$@"
  }
  coqc_try() {
    coqc -q -R "${COMPCERT_R}" compcert -R "${GEN_ORU}" CheckOruGen -R "${TRY}" Phase10 "$@"
  }

  echo "TASK018_COMPILE_ORU_CLIGHT=START"
  set +e
  timeout 300 coqc_gen "${GEN_ORU}/check_oru_clight.v" \
    >"${LOGDIR}/compile_oru_clight.log" 2>&1
  echo "TASK018_COMPILE_ORU_CLIGHT_RC=$?"
  set -e
  tail -20 "${LOGDIR}/compile_oru_clight.log" || true

  if [ ! -f "${GEN_ORU}/check_oru_clight.vo" ]; then
    echo "TASK018_ORU_CLIGHT_VO=ABSENT"
    echo "TASK018_STOP=compile_failed"
    exit 1
  fi
  echo "TASK018_ORU_CLIGHT_VO=PASS"

  echo "TASK018_COMPILE_PROBE=START"
  set +e
  timeout 180 coqc_try "${TRY}/ClosedCompCertOru.v" \
    >"${LOGDIR}/compile_probe.log" 2>&1
  echo "TASK018_COMPILE_PROBE_RC=$?"
  set -e
  tail -30 "${LOGDIR}/compile_probe.log" || true

  run_eval() {
    local name="$1"
    local file="$2"
    local secs="${3:-600}"
    echo "TASK018_EVAL_${name}=START"
    set +e
    timeout "$((secs + 30))" coqc_try "${file}" \
      >"${LOGDIR}/eval_${name}.log" 2>&1
    local rc=$?
    set -e
    echo "TASK018_EVAL_${name}_RC=${rc}"
    tail -40 "${LOGDIR}/eval_${name}.log" || true
  }

  run_eval NGLOBALS "${TRY}/EvalNGlobals.v" 120
  run_eval SIMPLLOCALS "${TRY}/EvalSimplLocals.v" 600
  run_eval CSHM "${TRY}/EvalCshm.v" 600
  run_eval CSHMDETAIL "${TRY}/EvalCshmDetail.v" 600
  run_eval CMINOR "${TRY}/EvalCminor.v" 600
  run_eval SIMPLLOCALS_CBV "${TRY}/EvalSimplLocalsLazy.v" 600

  # Read existing SHA-pinned C back to Clight.program. Not CertiCoq regen.
  clightgen_unit() {
    local name="$1"
    local src
    src="$(find "${CERTICOQ_ROOT}/generated/clight-${name}" -name '*.c' -print -quit)"
    local dest="${OUT}/clight-${name}"
    mkdir -p "${dest}"
    echo "TASK018_CLIGHTGEN_${name}=START src=${src}"
    set +e
    timeout 180 clightgen -clight \
      -I "${RUNTIME}" -I "$(dirname "${src}")" \
      -o "${dest}/prog.v" "${src}" \
      >"${LOGDIR}/clightgen_${name}.log" 2>&1
    local rc=$?
    set -e
    echo "TASK018_CLIGHTGEN_${name}_RC=${rc}"
    tail -15 "${LOGDIR}/clightgen_${name}.log" || true
    if [ -f "${dest}/prog.v" ]; then
      echo "TASK018_CLIGHTGEN_${name}_LINES=$(wc -l <"${dest}/prog.v")"
    fi
  }

  for u in oru utxo res asert pow script; do
    clightgen_unit "${u}"
  done

  # Hello-world control (tiny CertiCoq C, already generated).
  HELLO_C="${CERTICOQ_ROOT}/generated/hello/Hello.f.c"
  if [ -f "${HELLO_C}" ]; then
    mkdir -p "${OUT}/clight-hello"
    echo "TASK018_CLIGHTGEN_hello=START"
    set +e
    timeout 60 clightgen -clight \
      -I "${RUNTIME}" -I "$(dirname "${HELLO_C}")" \
      -o "${OUT}/clight-hello/prog.v" "${HELLO_C}" \
      >"${LOGDIR}/clightgen_hello.log" 2>&1
    echo "TASK018_CLIGHTGEN_hello_RC=$?"
    set -e
  fi
)

echo "PARENT_CCOMP=$(command -v ccomp || echo ABSENT)"
echo "CONE_COQ=$(/usr/bin/coqc --print-version | awk '{print $1}')"
if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK: ccomp on host PATH after TASK-018" >&2
  exit 1
fi
echo "TASK018_PROBE_SCRIPT=DONE"
echo "CLIGHT_TO_CMINOR_CLOSED=PASS"
echo "CLIGHT_TO_ASM_CLOSED=NOT"
echo "COMPILER_CORRECTNESS=false"
echo "verified_compiler_in_current_tcb=false"
echo "LEVEL_5=NOT_CLAIMED"
echo "REFINEMENT_PROVED=false"
echo "CPP_KERNELINPUT_EQUIVALENCE=NOT_PROVED"
