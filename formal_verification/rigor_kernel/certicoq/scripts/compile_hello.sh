#!/usr/bin/env bash
# Phase 1 step 4: CertiCoq hello-world → generated C/Clight → sidecar ccomp.
# Not a kernel claim. Sidecar env stays inside this process.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=sidecar_lib.sh
. "${SCRIPT_DIR}/sidecar_lib.sh"

OPAMROOT="${CERTICOQ_ROOT}/.opam-root"
OPAMBIN="${CERTICOQ_ROOT}/bin/opam"
SWITCH=rigor-certicoq-8.19
OUT="${CERTICOQ_ROOT}/generated/hello"
SRC="${CERTICOQ_ROOT}/hello/Hello.v"
SIDECAR_BIN="${OPAMROOT}/${SWITCH}/bin"

print_license_banner
assert_cone_coq_untouched
refuse_host_path_install

if [ ! -x "${SIDECAR_BIN}/coqc" ] || [ ! -x "${SIDECAR_BIN}/ccomp" ]; then
  echo "sidecar missing; run toolchain_probe.sh / build_sidecar.sh first" >&2
  exit 1
fi
if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK: ccomp on host PATH" >&2
  exit 1
fi

rm -rf "${OUT}"
mkdir -p "${OUT}"
cp "${SRC}" "${OUT}/Hello.v"

# eval only in this process tree.
export OPAMROOT
export OPAMROOTISOK=1
eval "$("${OPAMBIN}" env --switch="${SWITCH}" --set-switch)"

echo "HELLO_COQC=$(command -v coqc)"
echo "HELLO_CCOMP=$(command -v ccomp)"
if [ "$(command -v coqc)" = "/usr/bin/coqc" ]; then
  echo "SIDECAR_LEAK: hello used cone coqc" >&2
  exit 1
fi

(
  cd "${OUT}"
  echo "CERTICOQ_COMPILE=START"
  coqc Hello.v
  echo "CERTICOQ_COMPILE=DONE"
  echo "=== generated files ==="
  ls -la
)

C_FILE="${OUT}/Hello.f.c"
if [ ! -f "${C_FILE}" ]; then
  echo "CERTICOQ_CLIGHT=ABSENT"
  echo "HELLO_WORLD=FAIL"
  exit 1
fi
echo "CERTICOQ_CLIGHT=${C_FILE}"
echo "CERTICOQ_CLIGHT=PRESENT"

RUNTIME="${OPAMROOT}/${SWITCH}/lib/coq/user-contrib/CertiCoq/Plugin/runtime"
CCOMP_FLAGS=(-I "${RUNTIME}" -I "${OUT}")
ASM="${OUT}/hello.s"
OBJ="${OUT}/hello.o"
if ccomp "${CCOMP_FLAGS[@]}" -S -o "${ASM}" "${C_FILE}"; then
  echo "CCOMP_ASM=${ASM}"
  echo "CCOMP_HELLO=PASS"
else
  echo "CCOMP_HELLO=FAIL"
  echo "generated C is not CompCert-C on this run; recorded, not papered over"
  echo "HELLO_WORLD=FAIL"
  exit 1
fi

if ccomp "${CCOMP_FLAGS[@]}" -c -o "${OBJ}" "${C_FILE}"; then
  echo "CCOMP_OBJ=${OBJ}"
else
  echo "CCOMP_OBJ=FAIL"
  echo "HELLO_WORLD=FAIL"
  exit 1
fi

# Isolation: the parent shell is not this process. Re-check cone.
if [ "$(cone_coq_version)" != "8.15.0" ]; then
  echo "CONE_COQ_REGRESSION after hello" >&2
  exit 1
fi
echo "CONE_COQ=8.15.0"
echo "COMPILER_CORRECTNESS=false"
echo "HELLO_WORLD=PASS"
echo "NOT_A_KERNEL_CLAIM=true"
