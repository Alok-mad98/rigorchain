#!/usr/bin/env bash
# Historical v0.1.3 assembler. Current packaging: tools/release/build-official.sh
# (docs/release/OFFICIAL_PACKAGING.md). Do not use this script for v0.2+.
#
# P0-093: assemble official rigor-v0.1.3 artifacts from current production
# binaries. Release engineering only. Does not modify consensus, database
# format, wallet schema, or Release Guardian stages.
#
# Forbidden as official names: rigor-v0.1.2-alphaplus-*
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

RELEASE_VERSION="0.1.3"
STAGE="${ROOT}/dist/release/v${RELEASE_VERSION}/official"
GHREL="${ROOT}/.github/releases/v${RELEASE_VERSION}"
SRC="${ROOT}/core/src"

die() { echo "FAIL: $*" >&2; exit 1; }

mkdir -p "${STAGE}" "${GHREL}"

python3 - "${ROOT}" <<'PY'
import sys
from pathlib import Path
sys.path.insert(0, str(Path(sys.argv[1]) / "tools" / "release"))
from official_names import OFFICIAL_ARTIFACTS, OLD_LINUX_ARTIFACT, is_forbidden_legacy_name
assert not is_forbidden_legacy_name("rigor-v0.1.3-linux-x86_64.tar.gz")
assert is_forbidden_legacy_name(OLD_LINUX_ARTIFACT)
print("OFFICIAL_NAMING=PASS")
print("ALPHAPLUS_RELEASE=false")
for n in OFFICIAL_ARTIFACTS:
    print(f"OFFICIAL_NAME={n}")
PY

# --- Linux: current-tree node binaries, official name, no alphaplus basename ---
need=(
  "${SRC}/rigord"
  "${SRC}/rigor-cli"
)
for p in "${need[@]}"; do
  [[ -x "$p" ]] || die "missing $p"
done

pkg="$(mktemp -d)"
mkdir -p "${pkg}/bin"
cp -a "${SRC}/rigord" "${pkg}/bin/rigord"
cp -a "${SRC}/rigor-cli" "${pkg}/bin/rigor-cli"
if [[ -x "${SRC}/qt/rigor-qt" ]]; then
  cp -a "${SRC}/qt/rigor-qt" "${pkg}/bin/rigor-wallet"
  cp -a "${SRC}/qt/rigor-qt" "${pkg}/bin/rigor-qt"
fi
# Official wallet/node package does not include mining software.
for b in "${pkg}/bin/"*; do
  strip --strip-unneeded "$b" 2>/dev/null || true
done

cat > "${pkg}/README.txt" <<EOF
RIGOR v${RELEASE_VERSION} Linux x86_64
Channel: stable
ALPHAPLUS_RELEASE=false

Official name: rigor-v0.1.3-linux-x86_64.tar.gz
Datadir: ~/.rigor
Config:  ~/.rigor/rigor.conf

Run:
  ./bin/rigord --version
  ./bin/rigor-cli --version
EOF

linux_name="rigor-v0.1.3-linux-x86_64.tar.gz"
[[ "${linux_name}" != *alphaplus* ]] || die "linux name contains alphaplus"
tar --sort=name --mtime="@0" --owner=0 --group=0 --numeric-owner \
  -C "${pkg}" -czf "${STAGE}/${linux_name}" .
rm -rf "${pkg}"
echo "PACKAGED=${STAGE}/${linux_name}"

# --- Windows: production NSIS installer with v0.1.3 display identity ---
win_src_exe=""
for cand in \
  "${ROOT}/dist/alpha/windows/rigor-v0.1.3-windows-x86_64.exe" \
  "${ROOT}/dist/alpha/windows/rigor-v0.1.3-windows.exe" \
  "${ROOT}/dist/alphaplus/windows/rigor-v0.1.3-windows.exe"
do
  if [[ -f "${cand}" ]]; then
    win_src_exe="${cand}"
    break
  fi
done
[[ -n "${win_src_exe}" ]] || die "missing Windows NSIS installer; build with bash scripts/alpha/build_alpha_windows_nsis.sh"
magic="$(od -An -tx1 -N2 "${win_src_exe}" | tr -d ' \n')"
[[ "${magic}" == "4d5a" ]] || die "Windows source is not MZ"

win_exe="rigor-v0.1.3-windows-x86_64.exe"
win_zip="rigor-v0.1.3-windows-x86_64.zip"
cp -a "${win_src_exe}" "${STAGE}/${win_exe}"
[[ "${win_exe}" != *alphaplus* ]] || die "windows exe name contains alphaplus"

python3 - "${STAGE}" "${win_exe}" "${win_zip}" "${ROOT}" <<'PY'
import sys, zipfile
from pathlib import Path
stage, exe, zname, root = Path(sys.argv[1]), sys.argv[2], sys.argv[3], Path(sys.argv[4])
readme = stage / "WINDOWS_README.txt"
readme.write_text(
    "RIGOR v0.1.3 Windows x86_64\n"
    "Channel: stable\n"
    "ALPHAPLUS_RELEASE=false\n\n"
    f"Official installer: {exe}\n"
    f"Official zip: {zname}\n"
    "Datadir: %APPDATA%\\RIGOR\n",
    encoding="utf-8",
)
# Portable node PE from the just-built staging dir (not leftover alphaplus PEs).
pe_extra = []
for name in ("rigord.exe", "rigor-cli.exe"):
    for src in (
        root / "dist" / "alpha" / "windows" / "staging" / name,
        root / "dist" / "alphaplus" / "windows" / name,
    ):
        if src.is_file() and src.read_bytes()[:2] == b"MZ":
            pe_extra.append((src, name))
            break
with zipfile.ZipFile(stage / zname, "w", compression=zipfile.ZIP_DEFLATED) as zf:
    zf.write(stage / exe, exe)
    zf.write(readme, "README.txt")
    for src, name in pe_extra:
        zf.write(src, name)
        print(f"ZIP_MEMBER={name}")
print(f"PACKAGED={stage / exe}")
print(f"PACKAGED={stage / zname}")
PY

# macOS DMGs: GitHub-hosted macOS only (WSL cannot emit them).
# Stage from release-wallet-macos.yml, never from release-build.yml:
#   bash tools/release/stage-official-macos-from-wallet-ci.sh
for dmg in rigor-v0.1.3-macos-x86_64.dmg rigor-v0.1.3-macos-arm64.dmg; do
  if [[ ! -f "${STAGE}/${dmg}" ]]; then
    echo "MACOS_ARTIFACT_PENDING_GHA=${dmg}"
    echo "HINT=bash tools/release/stage-official-macos-from-wallet-ci.sh"
  fi
done

# SHA256 for every official file that exists. Do not invent hashes.
python3 - "${STAGE}" "${GHREL}" <<'PY'
import hashlib
from pathlib import Path
import sys
stage, ghrel = Path(sys.argv[1]), Path(sys.argv[2])
required = [
    "rigor-v0.1.3-windows-x86_64.exe",
    "rigor-v0.1.3-windows-x86_64.zip",
    "rigor-v0.1.3-linux-x86_64.tar.gz",
    "rigor-v0.1.3-macos-x86_64.dmg",
    "rigor-v0.1.3-macos-arm64.dmg",
]
lines = []
present = []
missing = []
for name in required:
    path = stage / name
    if name.lower().find("alphaplus") >= 0:
        raise SystemExit(f"FAIL: forbidden alphaplus name {name}")
    if not path.is_file():
        missing.append(name)
        continue
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    sidecar = stage / f"{name}.sha256"
    sidecar.write_text(f"{digest}  {name}\n", encoding="utf-8")
    (ghrel / f"{name}.sha256").write_text(f"{digest}  {name}\n", encoding="utf-8")
    lines.append(f"{digest}  {name}")
    present.append(name)
    print(f"SHA256 {name} {digest}")
(stage / "SHA256SUMS").write_text("\n".join(lines) + "\n", encoding="utf-8")
(ghrel / "SHA256SUMS").write_text("\n".join(lines) + "\n", encoding="utf-8")
(ghrel / "SHA256SUMS.required").write_text("\n".join(required) + "\n", encoding="utf-8")
print(f"PRESENT={','.join(present)}")
if missing:
    print("MISSING=" + ",".join(missing))
print("SHA256_WRITTEN=PASS")
PY

echo "PACKAGE_OFFICIAL_V013=PASS"
