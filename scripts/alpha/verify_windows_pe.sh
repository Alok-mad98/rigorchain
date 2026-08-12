#!/usr/bin/env bash
# Verify Windows PE staging for RIGOR Alpha
set -euo pipefail

STAGING="${1:-}"
if [[ -z "${STAGING}" || ! -d "${STAGING}" ]]; then
  echo "Usage: $0 <staging-dir>" >&2
  exit 2
fi

pass=0
fail=0
check() {
  local id="$1" ok="$2" note="${3:-}"
  if [[ "$ok" == "1" ]]; then
    echo "PASS  $id  $note"
    pass=$((pass + 1))
  else
    echo "FAIL  $id  $note"
    fail=$((fail + 1))
  fi
}

is_pe_mz() {
  local f="$1"
  [[ -f "$f" ]] || return 1
  local magic
  magic="$(od -An -tx1 -N2 "$f" | tr -d ' \n')"
  [[ "$magic" == "4d5a" ]]
}

# PE optional header machine = AMD64 (0x8664) at COFF+4 after PE\0\0
is_pe64() {
  local f="$1"
  python3 - "$f" <<'PY'
import struct, sys
path = sys.argv[1]
with open(path, "rb") as fh:
    data = fh.read(4096)
if data[:2] != b"MZ":
    sys.exit(1)
e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
if data[e_lfanew:e_lfanew+4] != b"PE\0\0":
    sys.exit(1)
machine = struct.unpack_from("<H", data, e_lfanew + 4)[0]
sys.exit(0 if machine == 0x8664 else 1)
PY
}

REQUIRED=(rigor-wallet.exe rigord.exe rigor-miner.exe)
for name in "${REQUIRED[@]}"; do
  f="${STAGING}/${name}"
  if is_pe_mz "$f"; then
    check "mz_${name}" 1
  else
    check "mz_${name}" 0 "missing or not MZ"
    continue
  fi
  if is_pe64 "$f"; then
    check "pe64_${name}" 1 "machine=AMD64"
  else
    check "pe64_${name}" 0 "not PE32+ AMD64"
  fi
  # file(1) signature
  ft="$(file -b "$f" 2>/dev/null || true)"
  if echo "$ft" | grep -qiE 'PE32\+|MS Windows|executable'; then
    check "file_${name}" 1 "$ft"
  else
    check "file_${name}" 0 "$ft"
  fi
done

# Optional CLI
if [[ -f "${STAGING}/rigor-cli.exe" ]]; then
  is_pe_mz "${STAGING}/rigor-cli.exe" && check "mz_rigor-cli.exe" 1 || check "mz_rigor-cli.exe" 0
fi

# Dependency hints (DLL imports) via objdump if available.
# depends Qt for mingw is built static (qt_config=static) — no Qt DLLs expected.
needs_qt_runtime=0
if command -v x86_64-w64-mingw32-objdump >/dev/null 2>&1; then
  for name in rigor-wallet.exe; do
    f="${STAGING}/${name}"
    [[ -f "$f" ]] || continue
    imports="$(x86_64-w64-mingw32-objdump -p "$f" 2>/dev/null | awk '/DLL Name:/{print $3}' | sort -u | tr '\n' ' ')"
    echo "INFO  imports_${name}: ${imports}"
    if echo "$imports" | grep -qiE 'Qt5|Qt6'; then
      needs_qt_runtime=1
      check "qt_link_${name}" 1 "dynamic Qt DLL import — qt-runtime required"
    else
      check "qt_link_${name}" 1 "no Qt DLL import (static Qt / depends) — qt-runtime optional"
    fi
  done
fi

# Qt runtime staging: required only for dynamic Qt links
if [[ "${needs_qt_runtime}" == "1" ]]; then
  if [[ -d "${STAGING}/qt-runtime" ]] || [[ -d "$(dirname "${STAGING}")/qt-runtime" ]]; then
    check "qt_runtime_dir" 1
  else
    check "qt_runtime_dir" 0 "windeployqt output not staged (required for dynamic Qt)"
  fi
else
  if [[ -d "${STAGING}/qt-runtime" ]]; then
    check "qt_runtime_dir" 1 "present (optional for static Qt)"
  else
    check "qt_runtime_dir" 1 "skipped — static Qt (depends)"
  fi
fi

# Miner must not require MinGW runtime DLLs on target Windows.
if command -v x86_64-w64-mingw32-objdump >/dev/null 2>&1 && [[ -f "${STAGING}/rigor-miner.exe" ]]; then
  miner_imports="$(x86_64-w64-mingw32-objdump -p "${STAGING}/rigor-miner.exe" 2>/dev/null | awk '/DLL Name:/{print $3}' | sort -u | tr '\n' ' ')"
  if echo "${miner_imports}" | grep -qiE 'libwinpthread|libgcc_s|libstdc\+\+'; then
    check "miner_no_mingw_dll" 0 "imports MinGW runtime: ${miner_imports}"
  else
    check "miner_no_mingw_dll" 1 "no libwinpthread/libgcc/libstdc++ DLL import"
  fi
fi

echo "---"
echo "PE verification: pass=${pass} fail=${fail}"
[[ "${fail}" -eq 0 ]]
