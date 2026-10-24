#!/bin/sh
# Behavioral checks for the Linux SOLO launcher. A stub miner records argv
# so the real binary is never started. Mining flags must stay intact.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
SRC="$ROOT/installer/linux/rigor-miner.sh"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

mkdir -p "$TMP/bin"
cat > "$TMP/bin/rigor-miner" <<'EOF'
#!/bin/sh
: > "$ARGS_FILE"
for a in "$@"; do
  printf '%s\n' "$a" >> "$ARGS_FILE"
done
echo STUB_MINER
exit 0
EOF
chmod +x "$TMP/bin/rigor-miner"

write_script() {
  python3 - "$SRC" "$TMP/rigor-miner.sh" <<'PY'
import pathlib, sys
src, dest = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
text = src.read_text()
text = text.replace("RPC_USER=CHANGE_ME", "RPC_USER=testuser", 1)
text = text.replace("RPC_PASS=CHANGE_ME", "RPC_PASS=secretpass", 1)
text = text.replace("COINBASE_ADDR=rg1qCHANGE_ME", "COINBASE_ADDR=rg1qdefaultaddr", 1)
dest.write_text(text)
PY
  chmod +x "$TMP/rigor-miner.sh"
}

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

# Unedited published script must refuse to start.
out=$("$SRC" 2>&1) && fail "unedited script started"
printf '%s\n' "$out" | grep -q 'RPC_USER' || fail "unedited script lost edit hint"
printf '%s\n' "$out" | grep -q secretpass && fail "unedited script printed a password"

write_script

# Placeholder payout still refused after credentials are filled.
out=$("$TMP/rigor-miner.sh" rg1qYOURADDRESS 2>&1) && fail "rg1qYOURADDRESS was accepted"
printf '%s\n' "$out" | grep -q STUB_MINER && fail "stub ran for rg1qYOURADDRESS"

# Wrong-network prefixes refused.
for bad in trg1qabc rgrt1qabc rg1pabc; do
  out=$("$TMP/rigor-miner.sh" "$bad" 2>&1) && fail "$bad was accepted"
  printf '%s\n' "$out" | grep -q STUB_MINER && fail "stub ran for $bad"
  printf '%s\n' "$out" | grep -qi 'rg1q' || fail "$bad reject message missing rg1q"
done

run_ok() {
  ARGS_FILE="$TMP/args.txt"
  export ARGS_FILE
  rm -f "$ARGS_FILE"
  out=$(ARGS_FILE="$ARGS_FILE" "$@" 2>&1) || fail "expected start: $*"
  printf '%s\n' "$out" | grep -q STUB_MINER || fail "stub did not run: $*"
  printf '%s\n' "$out" | grep -q secretpass && fail "launcher printed the RPC password"
  [ -f "$ARGS_FILE" ] || fail "stub wrote no args"
}

has() {
  grep -Fxq -- "$1" "$TMP/args.txt" || fail "missing argv: $1"
}

has_not() {
  if grep -Fxq -- "$1" "$TMP/args.txt"; then
    fail "unexpected argv: $1"
  fi
}

# Default start: first argument is payout, no -t.
run_ok "$TMP/rigor-miner.sh" rg1qfromarg
has -a
has rgrpow01
has -q
has --no-getwork
has --coinbase-addr=rg1qfromarg
has -o
has http://127.0.0.1:8761
has -u
has testuser
has -p
has secretpass
has_not -t
grep -q 18761 "$TMP/args.txt" && fail "RPC pointed at 18761"
grep -q solomining "$TMP/args.txt" && fail "launcher passed -solomining"

# README example: MINER_THREADS=2 becomes -t 2.
run_ok env MINER_THREADS=2 "$TMP/rigor-miner.sh" rg1qfromarg
has -t
has 2
has --coinbase-addr=rg1qfromarg

# Below 1 is raised to 1.
run_ok env MINER_THREADS=0 "$TMP/rigor-miner.sh" rg1qfromarg
has -t
has 1

echo MINER_LAUNCHER_BEHAVIOR_OK
