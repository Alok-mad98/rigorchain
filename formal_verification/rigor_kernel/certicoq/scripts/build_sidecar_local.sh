#!/usr/bin/env bash
# Optional host-local sidecar: private OPAMROOT, no shell rc, no PATH export.
# Prefer Docker. This exists so a machine without Docker can still research.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=sidecar_lib.sh
. "${SCRIPT_DIR}/sidecar_lib.sh"

OPAMROOT="${CERTICOQ_ROOT}/.opam-root"
OPAMBIN="${CERTICOQ_ROOT}/bin/opam"
SWITCH=rigor-certicoq-8.19
export OPAMROOT
export OPAMYES=1
export OPAMCOLOR=never
# Do not let opam rewrite ~/.profile or the cone PATH.
export OPAMROOTISOK=1
# 11 GiB host: keep parallel compile modest so CompCert/MetaCoq do not OOM.
export OPAMJOBS="${OPAMJOBS:-2}"
export MAKEFLAGS="${MAKEFLAGS:--j2}"

SIDECAR_OPT="${CERTICOQ_ROOT}/.opt"
mkdir -p "${CERTICOQ_ROOT}/bin" "${SIDECAR_OPT}"

# Coq needs gmp.h. Do not sudo. If the host has no -dev package, fetch
# the .deb into the sidecar prefix (gitignored), not into the cone.
sidecar_gmp_h() {
  if [ -f /usr/include/gmp.h ]; then
    echo /usr/include/gmp.h
  elif [ -f "${SIDECAR_OPT}/usr/include/gmp.h" ]; then
    echo "${SIDECAR_OPT}/usr/include/gmp.h"
  elif [ -f "${SIDECAR_OPT}/usr/include/x86_64-linux-gnu/gmp.h" ]; then
    echo "${SIDECAR_OPT}/usr/include/x86_64-linux-gnu/gmp.h"
  fi
}

if [ -z "$(sidecar_gmp_h)" ]; then
  echo "Fetching libgmp-dev into ${SIDECAR_OPT} (no sudo, not vendored)…"
  TMPD="$(mktemp -d)"
  (cd "${TMPD}" && apt-get download libgmp-dev)
  DEB="$(find "${TMPD}" -maxdepth 1 -name 'libgmp-dev*.deb' -print -quit)"
  if [ -z "${DEB}" ]; then
    echo "apt-get download libgmp-dev produced no .deb" >&2
    rm -rf "${TMPD}"
    exit 1
  fi
  dpkg-deb -x "${DEB}" "${SIDECAR_OPT}"
  rm -rf "${TMPD}"
fi
GMP_H="$(sidecar_gmp_h)"
if [ -z "${GMP_H}" ]; then
  echo "gmp.h still missing; cannot build sidecar Coq" >&2
  find "${SIDECAR_OPT}" -name 'gmp*.h' >&2 || true
  exit 1
fi
echo "SIDECAR_GMP_H=${GMP_H}"
# The -dev .deb ships libgmp.so -> libgmp.so.10.4.1 (not in the .deb) and a
# non-PIC libgmp.a. Point the .so at the host libgmp10 and drop the .a so
# zarith does not fail at shared-object link.
if [ -d "${SIDECAR_OPT}/usr/lib/x86_64-linux-gnu" ]; then
  if [ -e /usr/lib/x86_64-linux-gnu/libgmp.so.10 ]; then
    ln -sfn /usr/lib/x86_64-linux-gnu/libgmp.so.10 \
      "${SIDECAR_OPT}/usr/lib/x86_64-linux-gnu/libgmp.so"
  fi
  rm -f "${SIDECAR_OPT}/usr/lib/x86_64-linux-gnu/libgmp.a"
  export LIBRARY_PATH="${SIDECAR_OPT}/usr/lib/x86_64-linux-gnu${LIBRARY_PATH:+:${LIBRARY_PATH}}"
fi
if [ -d "${SIDECAR_OPT}/usr/include" ]; then
  if [ -f "${SIDECAR_OPT}/usr/include/x86_64-linux-gnu/gmp.h" ] \
     && [ ! -f "${SIDECAR_OPT}/usr/include/gmp.h" ]; then
    ln -sfn x86_64-linux-gnu/gmp.h "${SIDECAR_OPT}/usr/include/gmp.h"
  fi
  export CPATH="${SIDECAR_OPT}/usr/include/x86_64-linux-gnu:${SIDECAR_OPT}/usr/include${CPATH:+:${CPATH}}"
  export C_INCLUDE_PATH="${SIDECAR_OPT}/usr/include/x86_64-linux-gnu:${SIDECAR_OPT}/usr/include${C_INCLUDE_PATH:+:${C_INCLUDE_PATH}}"
fi

if [ ! -x "${OPAMBIN}" ]; then
  echo "Installing a private opam binary (not /usr/bin/opam)…"
  ARCH="$(uname -m)"
  case "${ARCH}" in
    x86_64) OPAM_ARCH=x86_64 ;;
    aarch64|arm64) OPAM_ARCH=arm64 ;;
    *) echo "unsupported arch ${ARCH}" >&2; exit 1 ;;
  esac
  TMP="$(mktemp)"
  curl -fsSL "https://github.com/ocaml/opam/releases/download/2.2.1/opam-2.2.1-${OPAM_ARCH}-linux" -o "${TMP}"
  chmod +x "${TMP}"
  mv "${TMP}" "${OPAMBIN}"
fi

if [ ! -d "${OPAMROOT}/config" ]; then
  "${OPAMBIN}" init --bare --disable-sandboxing --no-setup
fi

if ! "${OPAMBIN}" switch show >/dev/null 2>&1 \
   || [ "$("${OPAMBIN}" switch show)" != "${SWITCH}" ]; then
  if ! "${OPAMBIN}" switch list --short | grep -qx "${SWITCH}"; then
    "${OPAMBIN}" switch create "${SWITCH}" ocaml-base-compiler.4.14.2
  else
    "${OPAMBIN}" switch "${SWITCH}"
  fi
fi

# env is used only in this process tree.
eval "$("${OPAMBIN}" env --switch="${SWITCH}" --set-switch)"

# Coq/CertiCoq/CompCert packages are not on opam.ocaml.org.
if ! "${OPAMBIN}" repo list --short | grep -Eqx 'rocq-released|coq-released'; then
  "${OPAMBIN}" repo add rocq-released https://rocq-prover.org/opam/released
fi

"${OPAMBIN}" update
# Host has no sudo. GMP headers live in ${SIDECAR_OPT}; do not call apt.
"${OPAMBIN}" option depext=false || true
"${OPAMBIN}" pin add -n -y -k version coq-compcert 3.13.1
"${OPAMBIN}" pin add -n -y -k version coq-certicoq 0.9+8.19
"${OPAMBIN}" pin add -n -y -k version coq-equations 1.3+8.19
"${OPAMBIN}" install -y --assume-depexts --confirm-level=unsafe-yes \
  $(grep -v '^#' "${CERTICOQ_ROOT}/opam.pinned")

command -v ccomp
ccomp -version
echo "SIDECAR_CCOMP=PRESENT"
echo "SIDECAR_COQ=$(coqc --print-version | awk '{print $1}')"
echo "OPAMROOT=${OPAMROOT}"
echo "Remember: do not eval this switch into the cone shell."
