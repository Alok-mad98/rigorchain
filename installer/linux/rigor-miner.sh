#!/usr/bin/env bash

cd "$(dirname "$0")"

echo
echo "RIGOR MAINNET SOLO miner"
echo
echo "Edit only the 4 lines below. Save, then run this file."
RPC_USER="CHANGE_ME"
RPC_PASS="CHANGE_ME"
COINBASE_ADDR="rg1qCHANGE_ME"
MINER_THREADS="3"

if [ "$RPC_USER" = "CHANGE_ME" ] || \
   [ "$RPC_PASS" = "CHANGE_ME" ] || \
   [ "$COINBASE_ADDR" = "rg1qCHANGE_ME" ]; then
    echo
    echo "Open this file in a text editor. Change only RPC_USER, RPC_PASS, COINBASE_ADDR, MINER_THREADS."
    exit 1
fi

if [ ! -x "./bin/rigor-miner" ]; then
    echo
    echo "bin/rigor-miner is missing or not executable."
    exit 1
fi

echo
echo "Starting miner..."
echo "RPC http://127.0.0.1:8761  user=$RPC_USER"
echo "Algorithm rgrpow01"
echo "Payout $COINBASE_ADDR"
echo "Threads $MINER_THREADS"
echo

./bin/rigor-miner \
    -a rgrpow01 \
    --no-getwork \
    -o "http://127.0.0.1:8761" \
    -u "$RPC_USER" \
    -p "$RPC_PASS" \
    --coinbase-addr="$COINBASE_ADDR" \
    -t "$MINER_THREADS"

EC=$?

echo
echo "Miner exited $EC"
exit $EC
