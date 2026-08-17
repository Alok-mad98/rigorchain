#!/usr/bin/env bash
# Gate GitHub Release assets: the three user downloads only.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
linux_ap="${ROOT}/dist/alphaplus/linux"
win="${ROOT}/dist/alphaplus/windows"
LINUX_TAR="${linux_ap}/rigor-v0.1.1-alphaplus-linux.tar.gz"
WIN_EXE="${win}/rigor-v0.1.1-alphaplus-windows.exe"
WIN_ZIP="${win}/rigor-v0.1.1-alphaplus-windows.zip"

die() { echo "VERIFY FAIL: $*" >&2; exit 1; }
pass() { echo "PASS  $*"; }

is_mz() {
  local f="$1"
  [[ -f "$f" ]] || return 1
  local magic
  magic="$(od -An -tx1 -N2 "$f" | tr -d ' \n')"
  [[ "$magic" == "4d5a" ]]
}
is_elf() {
  local f="$1"
  [[ -f "$f" ]] || return 1
  local magic
  magic="$(od -An -tx1 -N4 "$f" | tr -d ' \n')"
  [[ "$magic" == "7f454c46" ]]
}
min_bytes() {
  local f="$1" n="$2"
  [[ -f "$f" ]] || die "missing $f"
  local sz
  sz="$(stat -c '%s' "$f")"
  (( sz >= n )) || die "$f is ${sz} bytes; expected >= ${n}"
}

echo "== Linux =="
min_bytes "${LINUX_TAR}" $((10*1024*1024))
tmp="$(mktemp -d)"
tar -tzf "${LINUX_TAR}" > "${tmp}/linux.list"
grep -qE '(^|/)bin/rigor-wallet$' "${tmp}/linux.list" || die "linux tar missing bin/rigor-wallet"
grep -qE '(^|/)bin/rigord$' "${tmp}/linux.list" || die "linux tar missing bin/rigord"
grep -qE '(^|/)bin/rigor-cli$' "${tmp}/linux.list" || die "linux tar missing bin/rigor-cli"
grep -qE '(^|/)bin/rigor-miner$' "${tmp}/linux.list" || die "linux tar missing bin/rigor-miner"
if grep -qiE 'debug-start|rigor-wallet-boot' "${tmp}/linux.list"; then
  die "linux tar must not include debug-start or boot log"
fi
tar -xOf "${LINUX_TAR}" bin/rigor-wallet > "${tmp}/rigor-wallet"
is_elf "${tmp}/rigor-wallet" || die "linux rigor-wallet is not ELF"
pass "Linux tarball ELF payload"

echo "== Windows =="
min_bytes "${WIN_EXE}" $((10*1024*1024))
is_mz "${WIN_EXE}" || die "NSIS installer is not MZ"
is_elf "${WIN_EXE}" && die "NSIS installer is ELF (mispackaged)"
min_bytes "${WIN_ZIP}" $((8*1024*1024))

python3 - <<PY
import zipfile, sys
from pathlib import Path
zpath = Path("${WIN_ZIP}")
forbid = {
    "start-node.cmd", "start-peer2.cmd", "start-wallet.cmd",
    "fix-miningresfeeaddr.cmd", "debug-start.cmd", "rigor-wallet-boot.log",
}
with zipfile.ZipFile(zpath) as zf:
    names = set(zf.namelist())
    if "rigor-v0.1.1-alphaplus-windows.exe" not in names:
        print("VERIFY FAIL: zip missing rigor-v0.1.1-alphaplus-windows.exe", file=sys.stderr)
        raise SystemExit(1)
    extra = sorted(forbid & names)
    if extra:
        print("VERIFY FAIL: zip must not ship diagnostics/launchers:", ", ".join(extra), file=sys.stderr)
        raise SystemExit(1)
    data = zf.read("rigor-v0.1.1-alphaplus-windows.exe")
if data[:2] != b"MZ":
    print("VERIFY FAIL: zip windows.exe is not MZ", file=sys.stderr)
    raise SystemExit(1)
if b"NullsoftInst" not in data:
    print("VERIFY FAIL: zip windows.exe is not the NSIS setup wizard", file=sys.stderr)
    raise SystemExit(1)
print("PASS  Windows zip is NSIS setup named rigor-v0.1.1-alphaplus-windows.exe")
PY

rm -rf "${tmp}"
echo "VERIFY OK"
