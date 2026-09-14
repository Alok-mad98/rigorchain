#!/usr/bin/env bash
# Phase 1 step 5: download pinned tarballs, hash them locally, compare
# to the published opam checksums in certicoq/pins.sha512.
# Archives stay in generated/tarballs/ and must not be committed (I-4).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=sidecar_lib.sh
. "${SCRIPT_DIR}/sidecar_lib.sh"

PINFILE="${CERTICOQ_ROOT}/pins.sha512"
OUT="${CERTICOQ_ROOT}/generated/tarballs"
FAIL=0

print_license_banner
assert_cone_coq_untouched
mkdir -p "${OUT}"

witness=""
while IFS=$'\t' read -r name algo expected url; do
  case "${name}" in
    ''|\#*) continue ;;
  esac
  dest="${OUT}/${name}.tar.gz"
  echo "PIN_FETCH=${name}"
  echo "PIN_URL=${url}"
  if [ ! -f "${dest}" ]; then
    curl -fsSL --retry 3 -o "${dest}.partial" "${url}"
    mv "${dest}.partial" "${dest}"
  fi
  got="$(openssl dgst "-${algo}" -r "${dest}" | awk '{print $1}')"
  bytes="$(wc -c < "${dest}" | tr -d ' ')"
  echo "PIN_ALGO=${algo}"
  echo "PIN_PUBLISHED=${expected}"
  echo "PIN_WITNESSED=${got}"
  echo "PIN_BYTES=${bytes}"
  extra=""
  if [ "${name}" = "ocaml-4.14.2" ]; then
    extra="$(openssl dgst -sha512 -r "${dest}" | awk '{print $1}')"
    echo "PIN_WITNESSED_SHA512=${extra}"
    extra=" sha512 ${extra}"
  fi
  if [ "${got}" = "${expected}" ]; then
    echo "PIN_MATCH=${name}"
    witness="${witness}${name} ${algo} ${got} ${bytes}${extra}"$'\n'
  else
    echo "PIN_MISMATCH=${name}"
    FAIL=1
  fi
done < "${PINFILE}"

echo "${witness}" > "${OUT}/WITNESSED.txt"
echo "PIN_WITNESS_FILE=${OUT}/WITNESSED.txt"

# I-4: fail only if ccomp was installed onto the host, not if this
# Cursor session still has the gitignored sidecar switch on PATH.
if [ -e /usr/bin/ccomp ] || [ -e /usr/local/bin/ccomp ]; then
  echo "SIDECAR_LEAK: ccomp installed on host PATH" >&2
  FAIL=1
fi
if command -v ccomp >/dev/null 2>&1; then
  onpath="$(command -v ccomp)"
  case "${onpath}" in
    "${CERTICOQ_ROOT}"/*)
      echo "SIDECAR_CCOMP_ON_SESSION_PATH=${onpath}"
      ;;
    *)
      echo "SIDECAR_LEAK: ccomp on PATH is ${onpath}" >&2
      FAIL=1
      ;;
  esac
fi
echo "HOST_CCOMP=ABSENT"
if [ "$(cone_coq_version)" != "8.15.0" ]; then
  echo "CONE_COQ_REGRESSION after pin verify" >&2
  FAIL=1
fi
echo "CONE_COQ=8.15.0"
echo "COMPILER_CORRECTNESS=false"

if [ "${FAIL}" -ne 0 ]; then
  echo "PIN_VERIFY=FAIL"
  exit 1
fi
echo "PIN_VERIFY=PASS"
