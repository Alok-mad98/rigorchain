#!/usr/bin/env bash
# P0-092: one GitHub Actions / local release-matrix job.
# Phases: record | build | smoke | package | verify | checksum | all
#
# Release engineering only. Does not modify Consensus Kernel, Transaction
# Kernel, Ledger, ConnectBlock, DisconnectBlock, CoinsDB, wallet schema,
# classifier, PoW, ASERT, protocol parameters, formal proofs, or Release
# Guardian security stages.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

# Never source /etc/os-release into this shell: it exports VERSION= and
# would overwrite the product release (observed: "22.04.5 LTS ...").
RELEASE_VERSION="${RIGOR_RELEASE_VERSION:-0.1.3}"
TARGET="${RIGOR_RELEASE_TARGET:-}"
PHASE="${1:-all}"
export RIGOR_RELEASE_VERSION="${RELEASE_VERSION}"

if [[ -z "${TARGET}" ]]; then
  uname_s="$(uname -s | tr '[:upper:]' '[:lower:]')"
  uname_m="$(uname -m)"
  case "${uname_s}" in
    linux) TARGET="linux-x86_64" ;;
    darwin)
      if [[ "${uname_m}" == "arm64" ]]; then
        TARGET="macos-arm64"
      else
        TARGET="macos-x86_64"
      fi
      ;;
    mingw*|msys*|cygwin*|windows*) TARGET="windows-x86_64" ;;
    *)
      echo "FAIL: cannot infer RIGOR_RELEASE_TARGET from ${uname_s}/${uname_m}"
      exit 1
      ;;
  esac
fi

export RIGOR_RELEASE_TARGET="${TARGET}"

OUT="${ROOT}/dist/release/v${RELEASE_VERSION}/${TARGET}"
PROV="${OUT}/BUILD_PROVENANCE.txt"
STAGE="${OUT}/stage"
BIN_DIR="${OUT}/bin"
IDENTITY_SRC="${ROOT}/tools/release/rigor_release_identity.cpp"

mkdir -p "${OUT}" "${STAGE}" "${BIN_DIR}"

die() { echo "FAIL: $*" >&2; exit 1; }

os_name() {
  case "${TARGET}" in
    windows-*) echo windows ;;
    linux-*) echo linux ;;
    macos-*) echo macos ;;
    *) die "unknown target ${TARGET}" ;;
  esac
}

arch_name() {
  case "${TARGET}" in
    *-x86_64) echo x86_64 ;;
    *-arm64) echo arm64 ;;
    *) die "unknown target ${TARGET}" ;;
  esac
}

find_cxx() {
  if [[ -n "${CXX:-}" ]] && command -v "${CXX}" >/dev/null 2>&1; then
    echo "${CXX}"
    return 0
  fi
  for c in g++ clang++ c++; do
    if command -v "${c}" >/dev/null 2>&1; then
      echo "${c}"
      return 0
    fi
  done
  if command -v cl.exe >/dev/null 2>&1; then
    echo "cl.exe"
    return 0
  fi
  if command -v cl >/dev/null 2>&1; then
    echo "cl"
    return 0
  fi
  return 1
}

git_commit() {
  git -C "${ROOT}" rev-parse HEAD 2>/dev/null || echo unknown
}

record_phase() {
  local cxx cxx_ver os arch commit
  cxx="$(find_cxx)" || die "C++ compiler not found"
  if [[ "${cxx}" == "cl" || "${cxx}" == "cl.exe" ]]; then
    cxx_ver="$("${cxx}" 2>&1 | head -n1 || true)"
  else
    cxx_ver="$("${cxx}" --version 2>/dev/null | head -n1)"
  fi
  os="$(uname -s)"
  arch="$(uname -m)"
  commit="$(git_commit)"

  {
    echo "TASK=TASK-RIGOR-RELEASE-BUILD-MATRIX-P0-092"
    echo "VERSION=${RELEASE_VERSION}"
    echo "TARGET=${TARGET}"
    echo "GIT_COMMIT=${commit}"
    echo "COMPILER=${cxx}"
    echo "COMPILER_VERSION=${cxx_ver}"
    echo "OS=${os}"
    echo "ARCH=${arch}"
    echo "UNAME=$(uname -a)"
    if [[ -f /etc/os-release ]]; then
      echo "DISTRO=$(. /etc/os-release && printf '%s\n' "${PRETTY_NAME:-${ID:-unknown}}")"
    elif [[ "$(uname -s)" == "Darwin" ]]; then
      echo "DISTRO=macOS $(sw_vers -productVersion 2>/dev/null || echo unknown)"
    else
      echo "DISTRO=${os}"
    fi
    echo "CONSENSUS_CODE_CHANGED=FALSE"
    echo "DATABASE_FORMAT_CHANGED=FALSE"
    echo "WALLET_FORMAT_CHANGED=FALSE"
  } > "${PROV}"

  echo "=== RELEASE_BUILD_PROVENANCE ==="
  cat "${PROV}"
  echo "=== END RELEASE_BUILD_PROVENANCE ==="
}

identity_bin() {
  local os
  os="$(os_name)"
  if [[ "${os}" == "windows" ]]; then
    echo "${BIN_DIR}/rigor-release-identity.exe"
  else
    echo "${BIN_DIR}/rigor-release-identity"
  fi
}

c_escape() {
  python3 -c 'import sys; print(sys.argv[1].replace("\\", "\\\\").replace("\"", "\\\""))' "$1"
}

build_phase() {
  [[ -f "${PROV}" ]] || record_phase
  local cxx commit os arch compiler_ver ident flags
  cxx="$(find_cxx)" || die "C++ compiler not found"
  commit="$(git_commit)"
  os="$(uname -s)"
  arch="$(uname -m)"
  if [[ "${cxx}" == "cl" || "${cxx}" == "cl.exe" ]]; then
    compiler_ver="$("${cxx}" 2>&1 | head -n1 || true)"
  else
    compiler_ver="$("${cxx}" --version 2>/dev/null | head -n1)"
  fi
  ident="$(identity_bin)"
  [[ -f "${IDENTITY_SRC}" ]] || die "missing ${IDENTITY_SRC}"

  echo "BUILD_CURRENT_SOURCES=START"
  echo "SOURCE=${IDENTITY_SRC}"
  echo "OUTPUT=${ident}"

  if [[ "${cxx}" == "cl" || "${cxx}" == "cl.exe" ]]; then
    (
      cd "${OUT}"
      "${cxx}" /nologo /EHsc /std:c++17 \
        "/DRIGOR_RELEASE_VERSION=\"${RELEASE_VERSION}\"" \
        "/DRIGOR_GIT_COMMIT=\"${commit}\"" \
        "/DRIGOR_BUILD_OS=\"${os}\"" \
        "/DRIGOR_BUILD_ARCH=\"${arch}\"" \
        "/DRIGOR_BUILD_COMPILER=\"$(c_escape "${compiler_ver}")\"" \
        "/DRIGOR_BUILD_TARGET=\"${TARGET}\"" \
        /Fe:"$(basename "${ident}")" \
        "${IDENTITY_SRC}"
    )
    mv -f "${OUT}/$(basename "${ident}")" "${ident}"
  else
    flags=(
      -std=c++17
      "-DRIGOR_RELEASE_VERSION=\"${RELEASE_VERSION}\""
      "-DRIGOR_GIT_COMMIT=\"${commit}\""
      "-DRIGOR_BUILD_OS=\"${os}\""
      "-DRIGOR_BUILD_ARCH=\"${arch}\""
      "-DRIGOR_BUILD_COMPILER=\"$(c_escape "${compiler_ver}")\""
      "-DRIGOR_BUILD_TARGET=\"${TARGET}\""
    )
    "${cxx}" "${flags[@]}" -o "${ident}" "${IDENTITY_SRC}"
  fi
  [[ -f "${ident}" ]] || die "compiler produced no output"
  chmod +x "${ident}" || true
  echo "BUILD_CURRENT_SOURCES=PASS"
}

smoke_phase() {
  local ident out
  ident="$(identity_bin)"
  [[ -x "${ident}" || -f "${ident}" ]] || die "identity binary missing; run build first"
  echo "SMOKE_TEST=START"
  out="$("${ident}")"
  printf '%s\n' "${out}"
  echo "${out}" | grep -qx "VERSION=${RELEASE_VERSION}" || die "smoke: VERSION mismatch"
  echo "${out}" | grep -qx "TARGET=${TARGET}" || die "smoke: TARGET mismatch"
  echo "${out}" | grep -qx "RIGOR_RELEASE_IDENTITY=PASS" || die "smoke: identity token missing"
  echo "${out}" | grep -qx "CONSENSUS_CODE_CHANGED=FALSE" || die "smoke: consensus flag missing"
  echo "SMOKE_TEST=PASS"
}

linux_tar_name() { echo "rigor-v${RELEASE_VERSION}-linux-x86_64.tar.gz"; }
windows_exe_name() { echo "rigor-v${RELEASE_VERSION}-windows-x86_64.exe"; }
windows_zip_name() { echo "rigor-v${RELEASE_VERSION}-windows-x86_64.zip"; }
macos_dmg_name() {
  echo "rigor-v${RELEASE_VERSION}-macos-$(arch_name).dmg"
}

package_linux() {
  local tar_name ident pkg
  tar_name="$(linux_tar_name)"
  ident="$(identity_bin)"
  pkg="${STAGE}/pkg"
  rm -rf "${pkg}"
  mkdir -p "${pkg}/bin"
  cp -a "${ident}" "${pkg}/bin/rigor-release-identity"
  cp -a "${PROV}" "${pkg}/BUILD_PROVENANCE.txt"
  cat > "${pkg}/README.txt" <<EOF
RIGOR ${RELEASE_VERSION} Linux x86_64 release-matrix package
====================================================

Official name: ${tar_name}
Built from current sources on this runner (see BUILD_PROVENANCE.txt).

This package is the v${RELEASE_VERSION} official Linux artifact. It does not change
consensus, database format, or wallet schema.
EOF
  tar --sort=name --mtime="@0" --owner=0 --group=0 --numeric-owner \
    -C "${pkg}" -czf "${OUT}/${tar_name}" bin BUILD_PROVENANCE.txt README.txt
  echo "PACKAGED=${OUT}/${tar_name}"
}

package_windows() {
  local exe_name zip_name ident
  exe_name="$(windows_exe_name)"
  zip_name="$(windows_zip_name)"
  ident="$(identity_bin)"
  cp -a "${ident}" "${OUT}/${exe_name}"
  python3 - "${OUT}" "${exe_name}" "${zip_name}" "${PROV}" <<'PY'
import sys, zipfile
from pathlib import Path
out, exe, zname, prov = Path(sys.argv[1]), sys.argv[2], sys.argv[3], Path(sys.argv[4])
readme = out / "README.txt"
readme.write_text(
    "RIGOR 0.1.3 Windows x86_64 release-matrix package\n"
    f"Official names: {exe} and {zname}\n"
    "See BUILD_PROVENANCE.txt.\n",
    encoding="utf-8",
)
with zipfile.ZipFile(out / zname, "w", compression=zipfile.ZIP_DEFLATED) as zf:
    zf.write(out / exe, exe)
    zf.write(prov, "BUILD_PROVENANCE.txt")
    zf.write(readme, "README.txt")
print(f"PACKAGED={out / exe}")
print(f"PACKAGED={out / zname}")
PY
}

package_macos() {
  local dmg_name ident vol work
  dmg_name="$(macos_dmg_name)"
  ident="$(identity_bin)"
  command -v hdiutil >/dev/null 2>&1 || die "hdiutil missing (macOS GitHub runner required; WSL cannot emit DMG)"
  work="${STAGE}/dmgroot"
  rm -rf "${work}"
  mkdir -p "${work}"
  cp -a "${ident}" "${work}/rigor-release-identity"
  cp -a "${PROV}" "${work}/BUILD_PROVENANCE.txt"
  cat > "${work}/README.txt" <<EOF
RIGOR ${RELEASE_VERSION} macOS $(arch_name) release-matrix package
Official name: ${dmg_name}
EOF
  rm -f "${OUT}/${dmg_name}"
  hdiutil create -volname "RIGOR ${RELEASE_VERSION}" -srcfolder "${work}" -ov -format UDZO "${OUT}/${dmg_name}"
  echo "PACKAGED=${OUT}/${dmg_name}"
}

package_phase() {
  echo "PACKAGE_ARTIFACT=START"
  case "$(os_name)" in
    linux) package_linux ;;
    windows) package_windows ;;
    macos) package_macos ;;
  esac
  echo "PACKAGE_ARTIFACT=PASS"
}

expected_names() {
  python3 - "${ROOT}" "${TARGET}" <<'PY'
import sys
from pathlib import Path
sys.path.insert(0, str(Path(sys.argv[1]) / "tools" / "release"))
from official_names import TARGETS
for name in TARGETS[sys.argv[2]]["artifacts"]:
    print(name)
PY
}

verify_phase() {
  echo "VERIFY_FILENAME=START"
  python3 - "${ROOT}" "${TARGET}" "${OUT}" <<'PY'
import sys
from pathlib import Path
root = Path(sys.argv[1])
target = sys.argv[2]
out = Path(sys.argv[3])
sys.path.insert(0, str(root / "tools" / "release"))
from official_names import TARGETS, is_official_name, is_forbidden_legacy_name, OLD_LINUX_ARTIFACT
row = TARGETS[target]
missing = []
for name in row["artifacts"]:
    path = out / name
    if not path.is_file():
        missing.append(name)
        print(f"FAIL: missing official artifact {name}")
        continue
    if not is_official_name(name):
        print(f"FAIL: {name} is not an official v0.1.3 name")
        raise SystemExit(1)
    if is_forbidden_legacy_name(name):
        print(f"FAIL: forbidden legacy name {name}")
        raise SystemExit(1)
    print(f"FILENAME_OK={name}")
if (out / OLD_LINUX_ARTIFACT).exists():
    print(f"FAIL: retired name present: {OLD_LINUX_ARTIFACT}")
    raise SystemExit(1)
if missing:
    raise SystemExit(1)
print("VERIFY_FILENAME=PASS")
print("ARTIFACT_NAMING=PASS")
PY
}

write_sha256() {
  python3 - "$1" "$2" <<'PY'
import hashlib, sys
from pathlib import Path
src, dest = Path(sys.argv[1]), Path(sys.argv[2])
digest = hashlib.sha256(src.read_bytes()).hexdigest()
dest.write_text(f"{digest}  {src.name}\n", encoding="utf-8")
print(f"SHA256 {src.name} {digest}")
PY
}

checksum_phase() {
  echo "SHA256_MANIFEST=START"
  local name
  while IFS= read -r name; do
    [[ -f "${OUT}/${name}" ]] || die "checksum: missing ${name}"
    write_sha256 "${OUT}/${name}" "${OUT}/${name}.sha256"
  done < <(expected_names)
  (
    cd "${OUT}"
    : > SHA256SUMS
    while IFS= read -r name; do
      cat "${name}.sha256" >> SHA256SUMS
    done < <(expected_names)
  )
  echo "SHA256_MANIFEST=PASS"
  echo "SHA256SUMS=${OUT}/SHA256SUMS"
  cat "${OUT}/SHA256SUMS"
}

case "${PHASE}" in
  record) record_phase ;;
  build) build_phase ;;
  smoke) smoke_phase ;;
  package) package_phase ;;
  verify) verify_phase ;;
  checksum) checksum_phase ;;
  all)
    record_phase
    build_phase
    smoke_phase
    package_phase
    verify_phase
    checksum_phase
    echo "RELEASE_BUILD_JOB=PASS"
    echo "TARGET=${TARGET}"
    ;;
  *)
    die "unknown phase ${PHASE}"
    ;;
esac
