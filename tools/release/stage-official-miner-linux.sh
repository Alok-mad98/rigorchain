#!/usr/bin/env bash
# Stage Linux SOLO miner (minerd → rigor-miner). Wallet staging is untouched.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
# shellcheck source=official_packaging.inc.sh
source "${ROOT}/tools/release/official_packaging.inc.sh"
official_miner_init

die() { echo "FAIL: $*" >&2; exit 1; }

MINER_SRC="${ROOT}/miner"
[[ -x "${MINER_SRC}/minerd" ]] || die "missing ${MINER_SRC}/minerd (cd miner && make)"
strings "${MINER_SRC}/minerd" | grep -q 'lottery-era GBT field %s is not a shipping template field' \
  || die "${MINER_SRC}/minerd is stale (still RFC-019 lottery string or unrebuild)"
if strings "${MINER_SRC}/minerd" | grep -q 'is not RFC-019'; then
  die "${MINER_SRC}/minerd still contains retired RFC-019 error text — rebuild"
fi

OUT="${MINER_STAGING}/linux"
rm -rf "${OUT}"
mkdir -p "${OUT}/bin"
cp -a "${MINER_SRC}/minerd" "${OUT}/bin/rigor-miner"
strip --strip-unneeded "${OUT}/bin/rigor-miner" 2>/dev/null || true
chmod +x "${OUT}/bin/rigor-miner"
ln -sfn rigor-miner "${OUT}/bin/minerd"

cat > "${OUT}/README.txt" <<EOF
RIGOR Miner v${RELEASE_VERSION} Linux x86_64
Channel: ${CHANNEL}

Official name: ${MINER_LINUX_TAR_NAME}
This package is the SOLO miner. It is not the wallet.

Start the node / wallet first so RPC is on 127.0.0.1:8761, then:

  ./bin/rigor-miner -a rgrpow01 -o http://127.0.0.1:8761 \\
    -u RPCUSER -p RPCPASS \\
    --coinbase-addr=rg1q... \\
    -t 4

Payout must be ORU witness v0 (rg1q…). RES is not mined.
The miner copies getblocktemplate coinbase bytes (RFC-020: 128 ORU
per block for h ≥ 1, no halving) and only searches SHA3-256 RGRPOW01
nonces.

Packed CLIENT_VERSION remains 25.0.0.
refinement_proved=false
LEVEL_5_CLAIMED=false
CPP_VERIFIED=false
EOF

echo "STAGE_MINER_LINUX=PASS"
ls -la "${OUT}/bin"
