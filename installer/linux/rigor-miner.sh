#!/bin/sh
cd "$(dirname "$(readlink -f "$0")")"

echo
echo "RIGOR v0.2 MAINNET SOLO miner"
echo "RPC http://127.0.0.1:8761"
echo

# Edit the three lines below.
RPC_USER=CHANGE_ME
RPC_PASS=CHANGE_ME
COINBASE_ADDR=rg1qCHANGE_ME
if [ -n "${1:-}" ]; then
  COINBASE_ADDR=$1
fi

if [ "$RPC_USER" = "CHANGE_ME" ] || [ "$RPC_PASS" = "CHANGE_ME" ] || [ -z "$COINBASE_ADDR" ] || [ "$COINBASE_ADDR" = "rg1qCHANGE_ME" ] || [ "$COINBASE_ADDR" = "rg1qYOURADDRESS" ]; then
  echo "Edit this file and change RPC_USER and RPC_PASS."
  echo "They must match <wallet-dir>/data/rpc-auth.conf or rigor.conf."
  echo "Set COINBASE_ADDR to your rg1q address, or pass that address as the first argument."
  echo "Do not run bin/rigor-miner directly until those values are set."
  echo
  exit 1
fi

if [ ! -x bin/rigor-miner ]; then
  echo "bin/rigor-miner is missing from this folder."
  exit 1
fi

case $COINBASE_ADDR in
  rg1q*|RG1Q*)
    ;;
  *)
    echo "Payout must be a MAINNET rg1q address."
    echo
    exit 1
    ;;
esac

THREAD_ARG=
if [ -n "${MINER_THREADS:-}" ]; then
  if [ "$MINER_THREADS" -lt 1 ] 2>/dev/null; then
    MINER_THREADS=1
  fi
  THREAD_ARG="-t $MINER_THREADS"
fi

echo "Payout: $COINBASE_ADDR"
echo

# shellcheck disable=SC2086
bin/rigor-miner -a rgrpow01 -q $THREAD_ARG --no-getwork --coinbase-addr="$COINBASE_ADDR" -o "http://127.0.0.1:8761" -u "$RPC_USER" -p "$RPC_PASS"
EC=$?
echo
echo "Miner exited with code $EC."
exit "$EC"
