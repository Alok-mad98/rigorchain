#!/usr/bin/env bash
# Assemble official miner packages (parallel to wallet; never mixed into wallet tar).
#
# Inputs:
#   dist/release/v${VER}/staging/miner/linux/{README.txt,bin/rigor-miner}
#   dist/release/v${VER}/staging/miner/windows/rigor-v${VER}-miner-windows-x86_64.{exe,zip}
#
# Outputs:
#   dist/release/v${VER}/miner/
#   audit/miner_packaging/official/
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
# shellcheck source=official_packaging.inc.sh
source "${ROOT}/tools/release/official_packaging.inc.sh"
official_miner_init

die() { echo "FAIL: $*" >&2; exit 1; }

LINUX_SRC="${MINER_STAGING}/linux"
WIN_SRC="${MINER_STAGING}/windows"
[[ -x "${LINUX_SRC}/bin/rigor-miner" ]] || die "missing Linux miner (run tools/release/stage-official-miner-linux.sh)"
[[ -f "${LINUX_SRC}/README.txt" ]] || die "missing ${LINUX_SRC}/README.txt"
[[ -f "${WIN_SRC}/${MINER_WIN_EXE_NAME}" ]] || die "missing ${WIN_SRC}/${MINER_WIN_EXE_NAME}"
[[ -f "${WIN_SRC}/${MINER_WIN_ZIP_NAME}" ]] || die "missing ${WIN_SRC}/${MINER_WIN_ZIP_NAME}"

python3 - "${ROOT}" "${MINER_WIN_EXE_NAME}" "${MINER_WIN_ZIP_NAME}" "${MINER_LINUX_TAR_NAME}" <<'PY'
import sys
from pathlib import Path
sys.path.insert(0, str(Path(sys.argv[1]) / "tools" / "release"))
from official_packaging import is_forbidden_legacy_name
for n in sys.argv[2:]:
    if is_forbidden_legacy_name(n):
        raise SystemExit(f"FAIL: forbidden name {n}")
print("MINER_OFFICIAL_NAMING=PASS")
PY

rm -rf "${MINER_OFFICIAL}"
mkdir -p "${MINER_OFFICIAL}" "${MINER_VISIBLE}"

tar -C "${LINUX_SRC}" -czf "${MINER_OFFICIAL}/${MINER_LINUX_TAR_NAME}" README.txt bin
python3 - "${MINER_OFFICIAL}/${MINER_LINUX_TAR_NAME}" <<'PY'
import sys, tarfile
with tarfile.open(sys.argv[1], "r:gz") as tf:
    names = tf.getnames()
forbidden = [n for n in names if "wallet" in n.lower() or n.endswith("rigord") or "rigor-qt" in n]
if forbidden:
    raise SystemExit("FAIL: miner linux tar contains wallet/node files: " + ", ".join(forbidden))
if "bin/rigor-miner" not in names:
    raise SystemExit("FAIL: miner linux tar missing bin/rigor-miner")
print("MINER_LINUX_TAR_LAYOUT=PASS")
PY

cp -a "${WIN_SRC}/${MINER_WIN_EXE_NAME}" "${MINER_OFFICIAL}/"
cp -a "${WIN_SRC}/${MINER_WIN_ZIP_NAME}" "${MINER_OFFICIAL}/"

python3 - "${ROOT}" "${RELEASE_VERSION}" "${CHANNEL}" "${MINER_OFFICIAL}" <<'PY'
import hashlib, json, sys
from datetime import datetime, timezone
from pathlib import Path

root = Path(sys.argv[1])
ver = sys.argv[2]
channel = sys.argv[3]
out = Path(sys.argv[4])
sys.path.insert(0, str(root / "tools" / "release"))
from official_packaging import miner_artifacts, PACKED_CLIENT_VERSION

files = [
    f"rigor-v{ver}-miner-windows-x86_64.exe",
    f"rigor-v{ver}-miner-windows-x86_64.zip",
    f"rigor-v{ver}-miner-linux-x86_64.tar.gz",
]
sums = []
digest = {}
for name in files:
    p = out / name
    if not p.is_file():
        raise SystemExit(f"FAIL: missing {p}")
    h = hashlib.sha256(p.read_bytes()).hexdigest()
    sums.append(f"{h}  {name}")
    digest[name] = h
(out / "SHA256SUMS").write_text("\n".join(sums) + "\n", encoding="utf-8")
manifest = {
    "product": "rigor-miner",
    "version": f"v{ver}",
    "channel": channel,
    "packed_client_version": PACKED_CLIENT_VERSION,
    "refinement_proved": False,
    "LEVEL_5_CLAIMED": False,
    "CPP_VERIFIED": False,
    "rfc": "RFC-020",
    "wallet_package_includes_miner": False,
    "artifacts": miner_artifacts(ver),
    "sha256": digest,
    "built_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
}
(out / f"release_v{ver}_miner_manifest.json").write_text(
    json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
)
print("MINER_MANIFEST=PASS")
PY

rm -rf "${MINER_VISIBLE}"
mkdir -p "${MINER_VISIBLE}"
cp -a "${MINER_OFFICIAL}/." "${MINER_VISIBLE}/"
echo "PACKAGE_OFFICIAL_MINER=PASS"
echo "VISIBLE=${MINER_VISIBLE}"
ls -la "${MINER_OFFICIAL}"
cat "${MINER_OFFICIAL}/SHA256SUMS"
