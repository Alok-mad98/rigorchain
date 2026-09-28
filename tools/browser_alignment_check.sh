#!/usr/bin/env bash
# P0-096 remediation: explorer/browser identity must match v0.1.3 node SSOT.
# Display / config only. Does not modify consensus, genesis, or protocol parameters.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NET="$ROOT/explorer/src/network.ts"
COMP="$ROOT/explorer/src/compliance.ts"
HTML="$ROOT/explorer/public/index.html"
ENV="$ROOT/explorer/.env.example"

NODE_GENESIS="e2ce92cfef740f92e8cc84a8541d8e760c776c668afd9b61740b3a4315241468"
NODE_TESTNET_GENESIS="0aca439758fba7fa5190b7e1cf37f9447516ef4cade07a49e7d484ea5b67b9ce"
FOREIGN_GENESIS="1c2d643638c3db29c241f4daca5bbe90f8439f7b03d22a686247214ed52ad758"

errors=0
fail() { echo "FAIL: $*"; errors=$((errors + 1)); }

echo "== browser_alignment_check =="

for p in "$NET" "$COMP" "$HTML"; do
  [[ -f "$p" ]] || fail "missing $p"
done

set +e
python3 - "$NET" "$COMP" "$HTML" "$ENV" "$NODE_GENESIS" "$NODE_TESTNET_GENESIS" "$FOREIGN_GENESIS" <<'PY'
import sys
from pathlib import Path

net, comp, html = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
env = Path(sys.argv[4])
genesis, testnet, foreign = sys.argv[5], sys.argv[6], sys.argv[7]
errs = []

def read(p: Path) -> str:
    return p.read_text(encoding="utf-8", errors="replace") if p.is_file() else ""

n, c, h, e = read(net), read(comp), read(html), read(env)
src = n + "\n" + c

if genesis not in n:
    errs.append(f"explorer network.ts missing mainnet genesis {genesis}")
if "v0.1.3" not in n:
    errs.append("explorer network.ts missing product identity v0.1.3")
if "v0.1.1-alphaplus" in n or "v0.1.1-alphaplus" in h:
    errs.append("explorer still ships v0.1.1-alphaplus product string")
if foreign in n or foreign in c or foreign in h:
    errs.append(f"explorer still references foreign pre-P0-038 genesis {foreign}")
if testnet not in src:
    errs.append(f"explorer missing node testnet genesis {testnet}")
if "signet: 38761" in n:
    errs.append("explorer still advertises unshipped signet RPC 38761")
if 'mainnet: "rg"' not in n and 'mainnet: "rg"' not in n.replace("'", '"'):
    if 'return "rg"' not in n and '"rg"' not in n:
        errs.append("explorer mainnet HRP is not rg (node SSOT; protocol lock BECH32_HRP)")
# Per-network HRP: regtest must be rgrt, not a blanket trg default.
if "rgrt" not in n:
    errs.append("explorer missing regtest HRP rgrt")
if "dvt" not in n:
    errs.append("explorer missing devtest HRP dvt")
if 'return "trg"' in n and "rgrt" not in n:
    errs.append("explorer still defaults all non-mainnet HRP to trg")
if "v0.1.3" not in h:
    errs.append("explorer UI index.html is not v0.1.3")
if "AlphaPlus Consensus Freeze" in h:
    errs.append("explorer UI still badges AlphaPlus Consensus Freeze")
if e and "alphaplus" in e.lower() and "v0.1.3" not in e:
    errs.append("explorer .env.example still AlphaPlus-only")
if e and "/var/lib/rigor/testnet" in e:
    errs.append("explorer .env.example still defaults to testnet datadir")

if errs:
    for x in errs:
        print("FAIL:", x)
    raise SystemExit(1)
print("BROWSER_ALIGNMENT_CHECK=PASS")
print(f"MAINNET_GENESIS={genesis}")
print("PRODUCT=v0.1.3")
print("MAINNET_HRP=rg")
print("TESTNET_HRP=trg")
print("REGTEST_HRP=rgrt")
PY
py_rc=$?
set -e
if [[ "$py_rc" -ne 0 ]]; then
  errors=$((errors + 1))
fi

if [[ "$errors" -ne 0 ]]; then
  echo "BROWSER_ALIGNMENT=FAIL"
  exit 1
fi
echo "BROWSER_ALIGNMENT=PASS"
