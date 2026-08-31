#!/usr/bin/env bash
# P0-063 Consensus Kernel adversarial fuzz smoke.
# Crash => CONSENSUS_KERNEL_CRASH_FOUND
# CPP != extracted/pinned oracle => CONSENSUS_KERNEL_ORACLE_DISCREPANCY
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/core/src"
OUT="${TMPDIR:-/tmp}/consensus_kernel_adversarial_smoke"
VECTORS="$ROOT/formal_verification/adversarial_security_vectors.json"
OCAML="$ROOT/formal_verification/extracted/consensus_kernel_cli"
CXX="${CXX:-$(command -v g++ || command -v c++)}"

mkdir -p "$(dirname "$OUT")"

"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DCONSENSUS_KERNEL_ADVERSARIAL_MAIN \
  -I"$ROOT/fuzz/consensus_kernel_adversarial" \
  -I"$SRC" \
  -o "$OUT" \
  "$ROOT/fuzz/consensus_kernel_adversarial/consensus_kernel_adversarial.cpp" \
  "$ROOT/tools/consensus_kernel_refinement_log_stub.cpp" \
  "$SRC/consensus/rigor_issuance.cpp" \
  "$SRC/consensus/rigor_supply.cpp" \
  "$SRC/script/rigor_classify.cpp" \
  "$SRC/script/script.cpp" \
  "$SRC/uint256.cpp" \
  "$SRC/util/strencodings.cpp" \
  "$SRC/consensus/convergence_logic.cpp" \
  "$SRC/consensus_rigor/sigma_hash.cpp" \
  "$SRC/crypto/sha256.cpp" \
  "$SRC/arith_uint256.cpp" \
  -Wl,--gc-sections

set +e
"$OUT" smoke
rc=$?
set -e
if [[ $rc -ne 0 ]]; then
  echo "CONSENSUS_KERNEL_CRASH_FOUND"
  exit 1
fi

python3 - "$OUT" "$VECTORS" "$OCAML" <<'PY'
import json, subprocess, sys
from pathlib import Path

cpp, vectors, ocaml = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
v = json.loads(vectors.read_text(encoding="utf-8"))
for row in v["issuance"]:
    h = row["height"]
    out = subprocess.check_output([str(cpp), "issuance", str(h)], text=True)
    got = json.loads(out)
    if got["oru_subsidy"] != row["oru_subsidy"] or got["res_schedule"] != row["res_schedule"] or got["epoch"] != row["epoch"]:
        print("CONSENSUS_KERNEL_ORACLE_DISCREPANCY")
        print(f"  height={h} cpp={got} oracle={row}")
        sys.exit(1)
    if ocaml.is_file():
        raw = subprocess.check_output([str(ocaml), "issuance", str(h)], text=True).split()
        epoch, oru, sched = int(raw[0]), int(raw[1]), int(raw[2])
        if epoch != got["epoch"] or oru != got["oru_subsidy"] or sched != got["res_schedule"]:
            print("CONSENSUS_KERNEL_ORACLE_DISCREPANCY")
            print(f"  height={h} cpp={got} extracted={[epoch, oru, sched]}")
            sys.exit(1)
print("ORACLE_MATCH=PASS")
print("DISCREPANCY_FOUND=NO")
PY

echo "FUZZ_BASELINE=PASS"
echo "CRASH_FOUND=NO"
