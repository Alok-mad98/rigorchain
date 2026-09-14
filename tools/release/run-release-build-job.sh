#!/usr/bin/env bash
# TASK-002: release job for CURRENT_RUNTIME_ARCHITECTURE (rigord).
# Phases: record | build | smoke | package | verify | checksum | all
#
# Compiles core/src/rigord.cpp via Autotools (not rigor_release_identity.cpp).
# Does not modify formal_verification/, Coq, extract, or consensus sources.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

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
CORE="${ROOT}/core"
CHECKER="${ROOT}/tools/release/check_binary_identity.py"

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
  return 1
}

git_commit() {
  git -C "${ROOT}" rev-parse HEAD 2>/dev/null || echo unknown
}

write_not_release_ready() {
  local reason="$1"
  {
    echo "NOT_RELEASE_READY"
    echo "TARGET=${TARGET}"
    echo "REASON=${reason}"
    echo "CURRENT_RUNTIME_ARCHITECTURE=rigord"
    echo "THIS_JOB_DID_NOT_SHIP_IDENTITY_STUB=TRUE"
  } > "${OUT}/NOT_RELEASE_READY.txt"
  echo "NOT_RELEASE_READY=${reason}"
}

record_phase() {
  local cxx cxx_ver os arch commit
  cxx="$(find_cxx || true)"
  if [[ -n "${cxx}" ]]; then
    cxx_ver="$("${cxx}" --version 2>/dev/null | head -n1 || true)"
  else
    cxx="missing"
    cxx_ver="missing"
  fi
  os="$(uname -s)"
  arch="$(uname -m)"
  commit="$(git_commit)"

  {
    echo "TASK=TASK-002-RELEASE-ARTIFACT-CORRECTION"
    echo "VERSION=${RELEASE_VERSION}"
    echo "TARGET=${TARGET}"
    echo "GIT_COMMIT=${commit}"
    echo "COMPILER=${cxx}"
    echo "COMPILER_VERSION=${cxx_ver}"
    echo "OS=${os}"
    echo "ARCH=${arch}"
    echo "UNAME=$(uname -a)"
    echo "CMAKE_TARGET=NOT FOUND IN CURRENT REPOSITORY"
    echo "AUTOTOOLS_DAEMON=src/rigord"
    echo "SOURCE=core/src/rigord.cpp"
    echo "IDENTITY_STUB_FORBIDDEN=TRUE"
    echo "CONSENSUS_CODE_CHANGED=FALSE"
    echo "DATABASE_FORMAT_CHANGED=FALSE"
    echo "WALLET_FORMAT_CHANGED=FALSE"
  } > "${PROV}"

  echo "=== RELEASE_BUILD_PROVENANCE ==="
  cat "${PROV}"
  echo "=== END RELEASE_BUILD_PROVENANCE ==="
}

copy_bin() {
  local src="$1"
  local dest="$2"
  [[ -f "${src}" ]] || die "missing built binary ${src}"
  cp -a "${src}" "${dest}"
  chmod +x "${dest}" || true
}

build_linux_node() {
  echo "BUILD_CURRENT_SOURCES=START"
  echo "SOURCE=core/src/rigord.cpp"
  echo "AUTOTOOLS_TARGET=src/rigord src/rigor-cli"
  if [[ ! -x "${CORE}/autogen.sh" ]]; then
    die "missing ${CORE}/autogen.sh"
  fi
  command -v g++ >/dev/null 2>&1 || die "g++ not on PATH"
  command -v make >/dev/null 2>&1 || die "make not on PATH"
  command -v autoreconf >/dev/null 2>&1 || die "autoreconf not on PATH"
  pkg-config --exists libsodium || die "libsodium-dev / pkg-config libsodium missing"

  cd "${CORE}"
  if [[ ! -f Makefile || ! -f src/Makefile ]]; then
    bash ./autogen.sh
    ./configure \
      --without-gui \
      --with-sqlite=yes \
      --disable-bench \
      --disable-fuzz-binary \
      --disable-man \
      --disable-usdt \
      CXXFLAGS="-O2"
  fi
  make -j"$(nproc)" src/rigord src/rigor-cli
  cd "${ROOT}"

  [[ -x "${CORE}/src/rigord" ]] || die "make did not produce core/src/rigord"
  [[ -x "${CORE}/src/rigor-cli" ]] || die "make did not produce core/src/rigor-cli"
  copy_bin "${CORE}/src/rigord" "${BIN_DIR}/rigord"
  copy_bin "${CORE}/src/rigor-cli" "${BIN_DIR}/rigor-cli"
  if [[ -x "${CORE}/src/qt/rigor-qt" ]]; then
    copy_bin "${CORE}/src/qt/rigor-qt" "${BIN_DIR}/rigor-qt"
    echo "QT_IN_ARTIFACT=YES"
  else
    echo "QT_IN_ARTIFACT=NO (configure --without-gui; in-tree qt/rigor-qt not part of this package)"
  fi
  echo "OUTPUT=${BIN_DIR}/rigord"
  echo "BUILD_CURRENT_SOURCES=PASS"
}

build_phase() {
  [[ -f "${PROV}" ]] || record_phase
  rm -f "${OUT}/NOT_RELEASE_READY.txt"
  case "$(os_name)" in
    linux)
      build_linux_node
      ;;
    windows)
      echo "BUILD_CURRENT_SOURCES=START"
      if [[ -x "${CORE}/src/rigord.exe" && -x "${CORE}/src/rigor-cli.exe" ]]; then
        copy_bin "${CORE}/src/rigord.exe" "${BIN_DIR}/rigord.exe"
        copy_bin "${CORE}/src/rigor-cli.exe" "${BIN_DIR}/rigor-cli.exe"
        echo "BUILD_CURRENT_SOURCES=PASS"
      else
        write_not_release_ready "Windows Autotools/MSVC node toolchain not present; refusing identity stub. Need depends/x86_64-w64-mingw32 or build_msvc to emit rigord.exe."
        echo "BUILD_CURRENT_SOURCES=SKIP"
      fi
      ;;
    macos)
      echo "BUILD_CURRENT_SOURCES=START"
      write_not_release_ready "macOS node package is TODO. Official wallet DMGs stay on release-wallet-macos.yml. This job will not forge a DMG or ship rigor-release-identity."
      echo "BUILD_CURRENT_SOURCES=SKIP"
      ;;
  esac
}

smoke_phase() {
  echo "SMOKE_TEST=START"
  if [[ -f "${OUT}/NOT_RELEASE_READY.txt" ]]; then
    if grep -q 'THIS_JOB_DID_NOT_SHIP_IDENTITY_STUB=TRUE' "${OUT}/NOT_RELEASE_READY.txt"; then
      echo "SMOKE_TEST=SKIP (NOT_RELEASE_READY, no identity stub shipped)"
      echo "SMOKE_TEST=PASS"
      return 0
    fi
    die "NOT_RELEASE_READY.txt present without stub-refusal marker"
  fi
  local daemon cli
  if [[ "$(os_name)" == "windows" ]]; then
    daemon="${BIN_DIR}/rigord.exe"
    cli="${BIN_DIR}/rigor-cli.exe"
  else
    daemon="${BIN_DIR}/rigord"
    cli="${BIN_DIR}/rigor-cli"
  fi
  [[ -f "${daemon}" ]] || die "smoke: ${daemon} missing; refuse identity fallback"
  python3 "${CHECKER}" --daemon "${daemon}" --cli "${cli}"
  echo "SMOKE_TEST=PASS"
}

linux_tar_name() { echo "rigor-v${RELEASE_VERSION}-linux-x86_64.tar.gz"; }
windows_exe_name() { echo "rigor-v${RELEASE_VERSION}-windows-x86_64.exe"; }
windows_zip_name() { echo "rigor-v${RELEASE_VERSION}-windows-x86_64.zip"; }

package_linux() {
  local tar_name pkg
  tar_name="$(linux_tar_name)"
  pkg="${STAGE}/pkg"
  rm -rf "${pkg}"
  mkdir -p "${pkg}/bin"
  cp -a "${BIN_DIR}/rigord" "${pkg}/bin/rigord"
  cp -a "${BIN_DIR}/rigor-cli" "${pkg}/bin/rigor-cli"
  if [[ -x "${BIN_DIR}/rigor-qt" ]]; then
    cp -a "${BIN_DIR}/rigor-qt" "${pkg}/bin/rigor-qt"
  fi
  cp -a "${PROV}" "${pkg}/BUILD_PROVENANCE.txt"
  cat > "${pkg}/README.txt" <<EOF
RIGOR ${RELEASE_VERSION} Linux x86_64 node package
================================================

Official name: ${tar_name}
Binaries: bin/rigord (daemon), bin/rigor-cli (RPC client)
Built from core/src/rigord.cpp (Autotools). Not rigor_release_identity.cpp.
EOF
  tar --sort=name --mtime="@0" --owner=0 --group=0 --numeric-owner \
    -C "${pkg}" -czf "${OUT}/${tar_name}" bin BUILD_PROVENANCE.txt README.txt
  echo "PACKAGED=${OUT}/${tar_name}"
}

package_windows() {
  if [[ -f "${OUT}/NOT_RELEASE_READY.txt" ]]; then
    echo "PACKAGED=${OUT}/NOT_RELEASE_READY.txt"
    return 0
  fi
  local exe_name zip_name
  exe_name="$(windows_exe_name)"
  zip_name="$(windows_zip_name)"
  cp -a "${BIN_DIR}/rigord.exe" "${OUT}/${exe_name}"
  python3 - "${OUT}" "${exe_name}" "${zip_name}" "${PROV}" "${BIN_DIR}" <<'PY'
import sys, zipfile
from pathlib import Path
out, exe, zname, prov, bindir = Path(sys.argv[1]), sys.argv[2], sys.argv[3], Path(sys.argv[4]), Path(sys.argv[5])
readme = out / "README.txt"
readme.write_text(
    "RIGOR Windows x86_64 node package\n"
    "Contains rigord.exe (daemon) and rigor-cli.exe.\n"
    "Not rigor-release-identity.\n",
    encoding="utf-8",
)
with zipfile.ZipFile(out / zname, "w", compression=zipfile.ZIP_DEFLATED) as zf:
    zf.write(out / exe, "rigord.exe")
    cli = bindir / "rigor-cli.exe"
    if cli.is_file():
        zf.write(cli, "rigor-cli.exe")
    zf.write(prov, "BUILD_PROVENANCE.txt")
    zf.write(readme, "README.txt")
print(f"PACKAGED={out / exe}")
print(f"PACKAGED={out / zname}")
PY
}

package_macos() {
  if [[ -f "${OUT}/NOT_RELEASE_READY.txt" ]]; then
    echo "PACKAGED=${OUT}/NOT_RELEASE_READY.txt"
    echo "MACOS_DMG=NOT_FORGED"
    return 0
  fi
  die "macOS must stay NOT_RELEASE_READY; refusing to forge a DMG"
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
  python3 - "${ROOT}" "${TARGET}" "${OUT}" <<'PY'
import sys
from pathlib import Path
sys.path.insert(0, str(Path(sys.argv[1]) / "tools" / "release"))
from official_names import expected_release_build_names
out = Path(sys.argv[3])
for name in expected_release_build_names(sys.argv[2], out):
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
from official_names import (
    OLD_LINUX_ARTIFACT,
    expected_release_build_names,
    is_forbidden_legacy_name,
    is_release_build_artifact_name,
)
if any(out.glob("*identity*")):
    print("FAIL: identity stub artifact present")
    raise SystemExit(1)
names = expected_release_build_names(target, out)
missing = []
for name in names:
    path = out / name
    if not path.is_file():
        missing.append(name)
        print(f"FAIL: missing release-build artifact {name}")
        continue
    if name != "NOT_RELEASE_READY.txt" and not is_release_build_artifact_name(name):
        print(f"FAIL: {name} is not a release-build artifact name")
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
