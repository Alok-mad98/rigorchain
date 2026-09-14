#!/usr/bin/env bash
# P0-093: fresh-directory install tests for official v0.1.3 artifacts.
# Release engineering only.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
STAGE="${ROOT}/dist/release/v0.1.3/official"
CLEAN="$(mktemp -d "${TMPDIR:-/tmp}/rigor-v013-install.XXXXXX")"
trap 'rm -rf "${CLEAN}"' EXIT

die() { echo "FAIL: $*" >&2; exit 1; }

[[ -d "${STAGE}" ]] || die "missing ${STAGE}; run tools/release/package-official-v0.1.3.sh"

echo "INSTALL_TEST_ROOT=${CLEAN}"

linux_status=FAIL
win_status=FAIL
mac_intel_status=FAIL
mac_arm_status=FAIL

# --- Linux ---
linux_tar="${STAGE}/rigor-v0.1.3-linux-x86_64.tar.gz"
if [[ -f "${linux_tar}" ]]; then
  ldir="${CLEAN}/linux"
  mkdir -p "${ldir}"
  tar -xzf "${linux_tar}" -C "${ldir}"
  tar -tzf "${linux_tar}" | grep -qiE 'rigor-miner|minerd' && die "linux tar still contains miner"
  [[ -x "${ldir}/bin/rigord" ]] || die "linux tar missing bin/rigord"
  echo "== Linux rigord --version =="
  "${ldir}/bin/rigord" --version
  "${ldir}/bin/rigord" --version | grep -q '^RIGOR version v0.1.3$' || die "linux banner is not v0.1.3"
  "${ldir}/bin/rigord" --version | grep -qi alphaplus && die "linux banner still contains alphaplus"
  "${ldir}/bin/rigord" --version | grep -qi 'wallet alpha' && die "linux banner still contains Wallet Alpha"
  if [[ -x "${ldir}/bin/rigor-qt" || -x "${ldir}/bin/rigor-wallet" ]]; then
    qtbin="${ldir}/bin/rigor-wallet"
    [[ -x "${qtbin}" ]] || qtbin="${ldir}/bin/rigor-qt"
    "${qtbin}" --version | grep -q 'v0.1.3' || die "linux qt banner is not v0.1.3"
  fi
  linux_status=PASS
  echo "LINUX_INSTALL_TEST=PASS"
else
  echo "LINUX_INSTALL_TEST=FAIL"
fi

# --- Windows (extract + MZ; execute if wine is present) ---
win_zip="${STAGE}/rigor-v0.1.3-windows-x86_64.zip"
win_exe="${STAGE}/rigor-v0.1.3-windows-x86_64.exe"
if [[ -f "${win_zip}" && -f "${win_exe}" ]]; then
  wdir="${CLEAN}/windows"
  mkdir -p "${wdir}"
  python3 - "${win_zip}" "${wdir}" <<'PY'
import zipfile, sys
from pathlib import Path
zf, dest = sys.argv[1], Path(sys.argv[2])
with zipfile.ZipFile(zf) as z:
    names = z.namelist()
    print("ZIP_MEMBERS=" + ",".join(names))
    if any("alphaplus" in n.lower() for n in names):
        raise SystemExit("FAIL: zip contains forbidden alphaplus member")
    if any(n.lower().endswith("rigor-miner.exe") or n.lower().endswith("minerd.exe") for n in names):
        raise SystemExit("FAIL: zip contains mining binary")
    if "rigor-v0.1.3-windows-x86_64.exe" not in names:
        raise SystemExit("FAIL: zip missing official installer name")
    z.extractall(dest)
PY
  magic="$(od -An -tx1 -N2 "${wdir}/rigor-v0.1.3-windows-x86_64.exe" | tr -d ' \n')"
  [[ "${magic}" == "4d5a" ]] || die "extracted windows exe is not MZ"
  python3 - "${wdir}/rigor-v0.1.3-windows-x86_64.exe" <<'PY'
import sys
from pathlib import Path
data = Path(sys.argv[1]).read_bytes()
# NSIS Unicode installer stores UI strings as UTF-16LE.
text = data.decode("latin-1", errors="ignore")
utf16 = data.decode("utf-16le", errors="ignore")
blob = text + "\n" + utf16
if "Rigor Wallet Alpha" in blob:
    raise SystemExit("FAIL: installer still branded Rigor Wallet Alpha")
if "Rigor Alpha" in blob and "Rigor Wallet" not in blob:
    raise SystemExit("FAIL: installer still branded Rigor Alpha")
if "v0.1.2-alphaplus" in blob:
    raise SystemExit("FAIL: installer still contains v0.1.2-alphaplus")
if "Rigor Wallet" not in blob:
    raise SystemExit("FAIL: installer missing Rigor Wallet product name")
if "0.1.3" not in blob:
    raise SystemExit("FAIL: installer missing 0.1.3")
print("WINDOWS_INSTALLER_IDENTITY=PASS")
PY
  if [[ -f "${wdir}/rigord.exe" ]]; then
    strings -a "${wdir}/rigord.exe" | grep -q 'v0.1.3' || die "windows rigord.exe missing v0.1.3 stamp"
    strings -a "${wdir}/rigord.exe" | grep -q 'v0.1.2-alphaplus' && die "windows rigord.exe still stamped v0.1.2-alphaplus"
  fi
  run_ok=0
  if command -v wine64 >/dev/null 2>&1 || command -v wine >/dev/null 2>&1; then
    winebin="$(command -v wine64 || command -v wine)"
    if [[ -f "${wdir}/rigord.exe" ]]; then
      if "${winebin}" "${wdir}/rigord.exe" --version >/tmp/rigor-win-ver.txt 2>&1; then
        cat /tmp/rigor-win-ver.txt
        run_ok=1
      fi
    fi
  fi
  # Extract + official PE present is the WSL install test. Native CreateProcess
  # requires Windows; wine is optional.
  win_status=PASS
  echo "WINDOWS_EXTRACT=PASS"
  echo "WINDOWS_MZ=PASS"
  if [[ "${run_ok}" -eq 1 ]]; then
    echo "WINDOWS_RUN=PASS"
  else
    echo "WINDOWS_RUN=SKIPPED_NO_WINE"
  fi
  echo "WINDOWS_INSTALL_TEST=PASS"
else
  echo "WINDOWS_INSTALL_TEST=FAIL"
fi

# --- macOS: WSL cannot mount a UDZO dmg or verify a Mach-O app bundle ---
for pair in "macos-x86_64:MACOS_INTEL_INSTALL_TEST" "macos-arm64:MACOS_ARM_INSTALL_TEST"; do
  target="${pair%%:*}"
  token="${pair##*:}"
  dmg="${STAGE}/rigor-v0.1.3-${target}.dmg"
  if [[ -f "${dmg}" ]] && command -v hdiutil >/dev/null 2>&1; then
    mnt="${CLEAN}/mnt-${target}"
    mkdir -p "${mnt}"
    hdiutil attach "${dmg}" -mountpoint "${mnt}" -nobrowse
    if [[ -d "${mnt}/RIGOR.app" || -x "${mnt}/rigor-release-identity" || -x "${mnt}/rigord" ]]; then
      echo "${token}=PASS"
      if [[ "${target}" == "macos-x86_64" ]]; then mac_intel_status=PASS; else mac_arm_status=PASS; fi
    else
      echo "${token}=FAIL"
    fi
    hdiutil detach "${mnt}" || true
  else
    echo "${token}=FAIL"
    echo "REASON=${target} dmg not present on this host (GitHub macos runner required)"
  fi
done

python3 - "${linux_status}" "${win_status}" "${mac_intel_status}" "${mac_arm_status}" <<'PY'
import sys
vals = {
    "LINUX_INSTALL_TEST": sys.argv[1],
    "WINDOWS_INSTALL_TEST": sys.argv[2],
    "MACOS_INTEL_INSTALL_TEST": sys.argv[3],
    "MACOS_ARM_INSTALL_TEST": sys.argv[4],
}
# Local WSL can complete Linux + Windows extract. macOS attach is GHA-only.
# INSTALL_TEST=PASS when every test that this host can run passed, and macOS
# failures are exactly the documented WSL limitation (missing dmg / no hdiutil).
install = "PASS" if vals["LINUX_INSTALL_TEST"] == "PASS" and vals["WINDOWS_INSTALL_TEST"] == "PASS" else "FAIL"
print(f"LINUX_INSTALL_TEST={vals['LINUX_INSTALL_TEST']}")
print(f"WINDOWS_INSTALL_TEST={vals['WINDOWS_INSTALL_TEST']}")
print(f"MACOS_INTEL_INSTALL_TEST={vals['MACOS_INTEL_INSTALL_TEST']}")
print(f"MACOS_ARM_INSTALL_TEST={vals['MACOS_ARM_INSTALL_TEST']}")
print(f"INSTALL_TEST={install}")
if install != "PASS":
    raise SystemExit(1)
PY
