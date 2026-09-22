#!/usr/bin/env bash
# Canonical official packager (v0.1.3 GitHub Release asset names).
#
# Does not push. Does not emit macOS DMGs. Does not modify RFC / consensus /
# Coq / claim flags / packed CLIENT_VERSION.
#
# Inputs (must already exist):
#   dist/release/v${VER}/staging/linux/bin/{rigord,rigor-cli,rigor-wallet,rigor-qt}
#   dist/release/v${VER}/staging/windows/rigor-v${VER}-windows-x86_64.exe
#   dist/release/v${VER}/staging/windows/{rigord.exe,rigor-cli.exe}
#
# Outputs:
#   dist/release/v${VER}/official/     (gitignored)
#   audit/wallet_packaging/official/   (visible copy)
#
# Version: RELEASE_VERSION=0.2  or  parsed from clientversion.h
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
# shellcheck source=official_packaging.inc.sh
source "${ROOT}/tools/release/official_packaging.inc.sh"

official_init

LINUX_BIN="${LINUX_BIN:-${STAGING}/linux/bin}"
WIN_EXE_SRC="${WIN_EXE_SRC:-${STAGING}/windows/${WIN_EXE_NAME}}"
WIN_PE="${WIN_PE:-${STAGING}/windows}"

die() { echo "FAIL: $*" >&2; exit 1; }

[[ -x "${LINUX_BIN}/rigord" && -x "${LINUX_BIN}/rigor-cli" && -x "${LINUX_BIN}/rigor-wallet" ]] \
  || die "missing Linux bins under ${LINUX_BIN} (run tools/release/stage-official-linux.sh)"
[[ -f "${LINUX_BIN}/rigor-qt" ]] || die "missing ${LINUX_BIN}/rigor-qt"
[[ -f "${WIN_EXE_SRC}" ]] || die "missing Windows NSIS ${WIN_EXE_SRC} (run tools/release/stage-official-windows.sh)"
magic="$(od -An -tx1 -N2 "${WIN_EXE_SRC}" | tr -d ' \n')"
[[ "${magic}" == "4d5a" ]] || die "Windows installer is not MZ"
for pe in rigord.exe rigor-cli.exe; do
  [[ -f "${WIN_PE}/${pe}" ]] || die "missing ${WIN_PE}/${pe}"
done
python3 - "${ROOT}" "${WIN_EXE_NAME}" "${LINUX_TAR_NAME}" <<'PY'
import sys
from pathlib import Path
sys.path.insert(0, str(Path(sys.argv[1]) / "tools" / "release"))
from official_packaging import is_forbidden_legacy_name
for n in sys.argv[2:]:
    if is_forbidden_legacy_name(n):
        raise SystemExit(f"FAIL: forbidden name {n}")
print("OFFICIAL_NAMING=PASS")
PY

rm -rf "${OFFICIAL}"
mkdir -p "${OFFICIAL}" "${VISIBLE}"

pkg="$(mktemp -d)"
mkdir -p "${pkg}/bin/internal"
for b in rigord rigor-cli rigor-wallet rigor-qt; do
  src="${LINUX_BIN}/${b}"
  [[ -f "${src}" ]] || die "missing ${src}"
  if [[ -L "${src}" ]] || head -c 2 "${src}" | grep -q '^#!'; then
    die "${src} is a wrapper; need the ELF binary"
  fi
  cp -a "${src}" "${pkg}/bin/internal/${b}"
done
# GUI binary is one object; wallet and qt names both point at it.
if [[ "$(sha256sum "${pkg}/bin/internal/rigor-wallet" | awk '{print $1}')" != "$(sha256sum "${pkg}/bin/internal/rigor-qt" | awk '{print $1}')" ]]; then
  cp -a "${pkg}/bin/internal/rigor-wallet" "${pkg}/bin/internal/rigor-qt"
fi
strip --strip-unneeded "${pkg}/bin/internal/"* 2>/dev/null || true
write_official_wrapper() {
  local dest="$1"
  local inner="$2"
  cat > "${dest}" <<EOF
#!/bin/sh
here=\$(dirname "\$(readlink -f "\$0")")
root=\$(dirname "\$here")
mkdir -p "\$root/data"
exec "\$here/internal/${inner}" -server -datadir="\$root/data" -rpcallowip=127.0.0.1 "\$@"
EOF
  chmod +x "${dest}"
}
write_official_wrapper "${pkg}/bin/rigord" rigord
write_official_wrapper "${pkg}/bin/rigor-cli" rigor-cli
write_official_wrapper "${pkg}/bin/rigor-wallet" rigor-wallet
write_official_wrapper "${pkg}/bin/rigor-qt" rigor-qt

cat > "${pkg}/README.txt" <<EOF
RIGOR v${RELEASE_VERSION} Linux x86_64
Channel: ${CHANNEL}

Official name: ${LINUX_TAR_NAME}
Datadir: <this-tree>/data
Config:  <this-tree>/data/rigor.conf
Do not run bin/internal/* directly.

Run:
  ./bin/rigord --version
  ./bin/rigor-wallet --version

Packed CLIENT_VERSION remains 25.0.0.
refinement_proved=false
LEVEL_5_CLAIMED=false
CPP_VERIFIED=false
EOF

tar --sort=name --mtime="@0" --owner=0 --group=0 --numeric-owner \
  -C "${pkg}" -czf "${OFFICIAL}/${LINUX_TAR_NAME}" .
rm -rf "${pkg}"
echo "PACKAGED=${OFFICIAL}/${LINUX_TAR_NAME}"

cp -a "${WIN_EXE_SRC}" "${OFFICIAL}/${WIN_EXE_NAME}"

python3 - "${OFFICIAL}" "${WIN_EXE_NAME}" "${WIN_ZIP_NAME}" "${WIN_PE}" "${RELEASE_VERSION}" "${CHANNEL}" <<'PY'
import sys, zipfile
from pathlib import Path
stage, exe, zname, pe_dir, ver, channel = (
    Path(sys.argv[1]), sys.argv[2], sys.argv[3], Path(sys.argv[4]), sys.argv[5], sys.argv[6]
)
readme = stage / "WINDOWS_README.txt"
readme.write_text(
    f"RIGOR v{ver} Windows x86_64\n"
    f"Channel: {channel}\n\n"
    f"Official installer: {exe}\n"
    f"Official zip: {zname}\n"
    "Datadir: folder named data next to rigor-wallet.exe\n"
    "Config:  <install>\\\\data\\\\rigor.conf\n\n"
    "Packed CLIENT_VERSION remains 25.0.0.\n"
    "refinement_proved=false\n"
    "LEVEL_5_CLAIMED=false\n"
    "CPP_VERIFIED=false\n",
    encoding="utf-8",
)
members = [(stage / exe, exe), (readme, "README.txt")]
for name in ("rigord.exe", "rigor-cli.exe"):
    src = pe_dir / name
    if not (src.is_file() and src.read_bytes()[:2] == b"MZ"):
        raise SystemExit(f"FAIL: missing PE {src}")
    members.append((src, name))
    print(f"ZIP_MEMBER={name}")
# Reproducible zip: fixed DOS timestamp (matches linux tar --mtime=@0 intent).
stamp = (1980, 1, 1, 0, 0, 0)
with zipfile.ZipFile(stage / zname, "w", compression=zipfile.ZIP_DEFLATED) as zf:
    for src, name in members:
        info = zipfile.ZipInfo(name, date_time=stamp)
        data = src.read_bytes()
        # preserve execute bit on PEs
        info.external_attr = 0o100644 << 16
        zf.writestr(info, data, compress_type=zipfile.ZIP_DEFLATED)
print(f"PACKAGED={stage / exe}")
print(f"PACKAGED={stage / zname}")
PY

echo "MACOS_ARTIFACT_PENDING=${MACOS_X64_DMG}"
echo "MACOS_ARTIFACT_PENDING=${MACOS_ARM_DMG}"
echo "HINT=WSL cannot emit Mach-O; GitHub-hosted macos-15-intel / macos-15"

python3 - "${OFFICIAL}" "${RELEASE_VERSION}" "${VISIBLE}" "${CHANNEL}" "${ROOT}" <<'PY'
import hashlib, json, shutil
from datetime import date
from pathlib import Path
import subprocess, sys
sys.path.insert(0, str(Path(sys.argv[5]) / "tools" / "release"))
from official_packaging import (
    NAMING_FOLLOWS,
    PACKED_CLIENT_VERSION,
    macos_pending,
    official_artifacts,
    present_without_macos,
)

stage, ver, visible, channel = Path(sys.argv[1]), sys.argv[2], Path(sys.argv[3]), sys.argv[4]
required_present = list(present_without_macos(ver))
pending = list(macos_pending(ver))
sha = {}
lines = []
for name in required_present:
    path = stage / name
    if not path.is_file():
        raise SystemExit(f"FAIL: missing {path}")
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    sha[name] = digest
    (stage / f"{name}.sha256").write_text(f"{digest}  {name}\n", encoding="utf-8")
    lines.append(f"{digest}  {name}")
    print(f"SHA256 {name} {digest}")
(stage / "SHA256SUMS").write_text("\n".join(lines) + "\n", encoding="utf-8")
names = list(official_artifacts(ver))
(stage / "SHA256SUMS.required").write_text("\n".join(names) + "\n", encoding="utf-8")
(stage / "official_artifact_names.txt").write_text("\n".join(names) + "\n", encoding="utf-8")
commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
short = subprocess.check_output(["git", "rev-parse", "--short=12", "HEAD"], text=True).strip()
manifest = {
    "project": "RIGOR",
    "version": ver,
    "channel": channel,
    "do_not_push": True,
    "github_release_created": False,
    "tag": f"v{ver}",
    "title": f"RIGOR v{ver}",
    "url": f"https://github.com/rigorchain/rigorchain/releases/tag/v{ver}",
    "naming_follows": NAMING_FOLLOWS,
    "alphaplus_release": False,
    "artifacts": ["windows-x86_64", "linux-x86_64", "macos-x86_64", "macos-arm64"],
    "official_artifacts": names,
    "present": required_present + ["SHA256SUMS"],
    "macos_pending": pending,
    "wsl_cannot_generate_macos_binaries": True,
    "sha256": sha,
    "freeze_commit": commit,
    "linux_build_git_commit": short,
    "packed_client_version": PACKED_CLIENT_VERSION,
    "refinement_proved": False,
    "LEVEL_5_CLAIMED": False,
    "CPP_VERIFIED": False,
    "consensus_code_changed": False,
    "nature": "Release engineering / distribution packaging. NOT a consensus TASK. NOT a public push.",
    "date": date.today().isoformat(),
    "staging": f"dist/release/v{ver}/official",
    "visible_copy": "audit/wallet_packaging/official",
}
(stage / f"release_v{ver}_manifest.json").write_text(
    json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
)
visible.mkdir(parents=True, exist_ok=True)
for old in visible.iterdir():
    if old.is_file():
        old.unlink()
for p in stage.iterdir():
    if p.is_file():
        shutil.copy2(p, visible / p.name)
print("MANIFEST=PASS")
print("VISIBLE_COPY=PASS")
PY

echo "PACKAGE_OFFICIAL=PASS"
echo "DO_NOT_PUSH=1"
echo "OFFICIAL=${OFFICIAL}"
echo "VISIBLE=${VISIBLE}"
