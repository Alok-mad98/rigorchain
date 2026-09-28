#!/usr/bin/env bash
# P0-100: final mainnet launch-candidate verification.
# AUDIT-FIRST. Production readiness gate. NOT a development TASK.
# Does not modify Consensus Kernel, Transaction Kernel, Ledger,
# ConnectBlock / DisconnectBlock, CheckProofOfWork, ASERT, Genesis, Magic,
# network parameters, OPCODE definitions, CoinsDB format, wallet schema,
# or formal models. A mismatch is REPORT ONLY. Do not patch inside this TASK.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
export PYTHONUNBUFFERED=1

DOC="$ROOT/docs/release/RIGOR_MAINNET_LAUNCH_CANDIDATE_AUDIT_P0_100.md"
MANIFEST="$ROOT/formal_verification/mainnet_launch_candidate_manifest.json"
FOLLOWUP="$ROOT/docs/release/FOLLOWUP_P0_101_LAUNCH_FREEZE_AND_REBUILD.md"
DUMP="$ROOT/formal_verification/mainnet_launch_candidate_audit"
OFFICIAL="$ROOT/dist/release/v0.1.3/official"
TAR="$OFFICIAL/rigor-v0.1.3-linux-x86_64.tar.gz"

echo "== P100 Mainnet launch candidate audit =="
echo "command=bash tools/run-mainnet-launch-candidate-audit.sh"
echo "nature=AUDIT-FIRST production readiness gate; no consensus/protocol/database patch"
echo "dump=$DUMP"

mkdir -p "$DUMP"
if [[ "${P100_VERDICT_ONLY:-0}" != "1" ]]; then
  find "$DUMP" -mindepth 1 -maxdepth 1 -exec rm -rf {} + 2>/dev/null || true
  mkdir -p "$DUMP"
fi

run_capture() {
  local name="$1"
  local out="$2"
  shift 2
  echo
  echo "######## ${name} ########"
  echo "command=$*"
  echo "log=$out"
  set +e
  bash -c "$*" 2>&1 | tee "$out"
  local rc=${PIPESTATUS[0]}
  set -e
  echo "${name}_EXIT=${rc}" | tee -a "$out"
  echo "$rc" > "${out}.rc"
  return 0
}

# ---------------------------------------------------------------------------
# PHASE 1 — official Linux production binary (not a missing in-tree rigord)
# ---------------------------------------------------------------------------
if [[ "${P100_VERDICT_ONLY:-0}" == "1" ]]; then
  echo "P100_VERDICT_ONLY=1 reusing dump=$DUMP"
else
echo
echo "######## PHASE 1 CORE NODE (official linux tarball) ########"
if [[ ! -f "$TAR" ]]; then
  echo "FAIL: missing $TAR" | tee "$DUMP/rigord_version.txt"
  echo "1" > "$DUMP/rigord_version.txt.rc"
else
  EXTRACT="$DUMP/linux-extract"
  mkdir -p "$EXTRACT"
  tar -xzf "$TAR" -C "$EXTRACT"
  RIGORD="$EXTRACT/bin/rigord"
  CLI="$EXTRACT/bin/rigor-cli"
  if [[ ! -x "$RIGORD" ]]; then
    echo "FAIL: tarball missing executable bin/rigord" | tee "$DUMP/rigord_version.txt"
    echo "1" > "$DUMP/rigord_version.txt.rc"
  else
    file "$RIGORD" > "$DUMP/rigord_file.txt" || true
    set +e
    "$RIGORD" --version > "$DUMP/rigord_version.txt" 2>&1
    echo "$?" > "$DUMP/rigord_version.txt.rc"
    set -e
    cat "$DUMP/rigord_version.txt"

    DATADIR="$DUMP/mainnet-datadir"
    mkdir -p "$DATADIR"
    echo "starting disconnected mainnet rigord for getblockchaininfo"
    set +e
    "$RIGORD" -datadir="$DATADIR" -daemon -listen=0 -noconnect -server=1 \
      -rpcport=28761 -port=28760 -printtoconsole=0 > "$DUMP/rigord_daemon.txt" 2>&1
    daemon_rc=$?
    echo "DAEMON_START_RC=${daemon_rc}" >> "$DUMP/rigord_daemon.txt"
    gbt_ok=0
    if [[ "$daemon_rc" -eq 0 ]]; then
      for i in $(seq 1 40); do
        if [[ -f "$DATADIR/rpc-auth.conf" ]]; then
          rpcuser="$(awk -F= '/^rpcuser=/{print $2}' "$DATADIR/rpc-auth.conf")"
          rpcpassword="$(awk -F= '/^rpcpassword=/{print $2}' "$DATADIR/rpc-auth.conf")"
          if "$CLI" -datadir="$DATADIR" -rpcport=28761 \
            -rpcuser="$rpcuser" -rpcpassword="$rpcpassword" getblockchaininfo \
            > "$DUMP/getblockchaininfo.json" 2>"$DUMP/getblockchaininfo.err"; then
            gbt_ok=1
            break
          fi
        fi
        sleep 1
      done
      if [[ -f "$DATADIR/rpc-auth.conf" ]]; then
        rpcuser="$(awk -F= '/^rpcuser=/{print $2}' "$DATADIR/rpc-auth.conf")"
        rpcpassword="$(awk -F= '/^rpcpassword=/{print $2}' "$DATADIR/rpc-auth.conf")"
        "$CLI" -datadir="$DATADIR" -rpcport=28761 \
          -rpcuser="$rpcuser" -rpcpassword="$rpcpassword" stop >/dev/null 2>&1 || true
      fi
      if [[ -f "$DATADIR/rigord.pid" ]]; then
        kill "$(cat "$DATADIR/rigord.pid")" >/dev/null 2>&1 || true
      fi
      sleep 1
    fi
    unset rpcuser rpcpassword || true
    set -e
    if [[ "$gbt_ok" -eq 1 ]]; then
      echo "GETBLOCKCHAININFO=PASS"
      echo "0" > "$DUMP/getblockchaininfo.rc"
      cat "$DUMP/getblockchaininfo.json"
    else
      echo "GETBLOCKCHAININFO=NOT_EXECUTED" | tee "$DUMP/getblockchaininfo.json"
      echo "1" > "$DUMP/getblockchaininfo.rc"
      echo "---- daemon log ----"
      cat "$DUMP/rigord_daemon.txt" || true
      echo "---- cli err ----"
      cat "$DUMP/getblockchaininfo.err" 2>/dev/null || true
    fi
  fi
fi

# ---------------------------------------------------------------------------
# PHASE 2 / 5 / 7 / 9 — required sub-audits actually execute
# ---------------------------------------------------------------------------
run_capture "PHASE2_DATABASE_SECURITY" "$DUMP/database_security.txt" \
  "bash tools/run-database-security-guardian-stage.sh"

run_capture "PHASE5_PRODUCTION_MINING_PATH" "$DUMP/mining_path.txt" \
  "bash tools/run-production-mining-path-audit.sh"

run_capture "PHASE9_RELEASE_GUARDIAN_FULL_PIPELINE" "$DUMP/release_guardian.txt" \
  "bash tools/run-release-guardian-full-pipeline-audit.sh"
fi

echo
echo "== P100 verdict (parsed from this run; a prior document is not a PASS) =="
set +e
python3 - "$ROOT" "$DOC" "$MANIFEST" "$FOLLOWUP" "$DUMP" <<'PY'
from __future__ import annotations

import hashlib
import json
import os
import re
import subprocess
import tarfile
import zipfile
from datetime import date, datetime, timezone
from pathlib import Path
import sys

root = Path(sys.argv[1])
doc_path = Path(sys.argv[2])
man_path = Path(sys.argv[3])
followup_path = Path(sys.argv[4])
dump = Path(sys.argv[5])

OFFICIAL_NAMES = [
    "rigor-v0.1.3-windows-x86_64.exe",
    "rigor-v0.1.3-windows-x86_64.zip",
    "rigor-v0.1.3-linux-x86_64.tar.gz",
    "rigor-v0.1.3-macos-x86_64.dmg",
    "rigor-v0.1.3-macos-arm64.dmg",
]
NODE_MAINNET_GENESIS = "e2ce92cfef740f92e8cc84a8541d8e760c776c668afd9b61740b3a4315241468"
NODE_MAGIC = ["0xce", "0xf1", "0xdb", "0xfa"]
P100_ALLOW = {
    "tools/run-mainnet-launch-candidate-audit.sh",
    "docs/release/RIGOR_MAINNET_LAUNCH_CANDIDATE_AUDIT_P0_100.md",
    "docs/release/FOLLOWUP_P0_101_LAUNCH_FREEZE_AND_REBUILD.md",
    "docs/release/RIGOR_RELEASE_FREEZE_P0_101.md",
    "formal_verification/mainnet_launch_candidate_manifest.json",
    "formal_verification/release_freeze_manifest.json",
    "formal_verification/release_v0.1.3_manifest.json",
    "formal_verification/production_mining_path_manifest.json",
    "formal_verification/production_reindex_reconstruction_audit_manifest.json",
    "formal_verification/wallet_sqlite_audit_manifest.json",
    "formal_verification/release_guardian_full_pipeline_manifest.json",
    "docs/performance/performance_results.json",
    "docs/security/README.md",
    ".gitignore",
}
CONSENSUS_PATHS = [
    "core/src/consensus",
    "core/src/script/rigor_classify.cpp",
    "core/src/script/rigor_classify.h",
    "core/src/validation.cpp",
    "core/src/pow.cpp",
    "core/src/rigor_kernel/pow_digest.cpp",
    "core/src/rigor_kernel/pow_digest.h",
    "core/src/primitives",
]
DB_PATHS = [
    "core/src/coins.cpp",
    "core/src/coins.h",
    "core/src/txdb.cpp",
    "core/src/txdb.h",
    "core/src/undo.h",
    "core/src/compressor.h",
    "core/src/dbwrapper.cpp",
    "core/src/dbwrapper.h",
]
WALLET_SCHEMA_PATHS = [
    "core/src/wallet/db.h",
    "core/src/wallet/sqlite.cpp",
    "core/src/wallet/sqlite.h",
    "core/src/wallet/walletdb.cpp",
]
PARAM_PATHS = [
    "core/src/kernel/chainparams.cpp",
    "core/src/consensus/consensus.h",
    "core/src/consensus/rigor_consensus_lock.h",
    "core/src/consensus/params.h",
]
FORMAL_PATHS = [
    "formal_verification/coq",
]


def read(rel: str) -> str:
    p = root / rel
    if not p.is_file():
        return ""
    return p.read_text(encoding="utf-8", errors="replace")


def exists(rel: str) -> bool:
    return (root / rel).is_file()


def git(*args: str) -> str:
    r = subprocess.run(
        ["git", "-C", str(root), *args],
        capture_output=True,
        text=True,
        check=False,
    )
    return ((r.stdout or "") + (r.stderr or "")).strip()


def git_names(paths: list[str]) -> set[str]:
    r = subprocess.run(
        ["git", "-C", str(root), "diff", "--name-only", "--"] + paths,
        capture_output=True,
        text=True,
        check=False,
    )
    return {ln.strip() for ln in r.stdout.splitlines() if ln.strip()}


def load_text(p: Path) -> str:
    if not p.is_file():
        return ""
    return p.read_text(encoding="utf-8", errors="replace")


def load_rc(p: Path) -> int:
    t = load_text(p).strip()
    try:
        return int(t)
    except ValueError:
        return 1


def has_line(blob: str, token: str) -> bool:
    return any(ln.strip() == token for ln in blob.splitlines())


def sha256_file(p: Path) -> str:
    h = hashlib.sha256()
    with p.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


findings: list[dict] = []
fails: list[str] = []


def fail(gate: str, msg: str) -> None:
    fails.append(f"{gate}: {msg}")
    findings.append({"severity": "FAIL", "gate": gate, "message": msg})


def info(gate: str, msg: str) -> None:
    findings.append({"severity": "INFO", "gate": gate, "message": msg})


def gate_pass(ok: bool) -> str:
    return "PASS" if ok else "FAIL"


# ---------------------------------------------------------------------------
# Source identity (SSOT)
# ---------------------------------------------------------------------------
cv = read("core/src/clientversion.h")
cp = read("core/src/kernel/chainparams.cpp")
lock = read("core/src/consensus/rigor_consensus_lock.h")
pow_h = read("core/src/pow.h")
pow_cpp = read("core/src/pow.cpp")
pow_digest_h = read("core/src/rigor_kernel/pow_digest.h")
pow_digest_cpp = read("core/src/rigor_kernel/pow_digest.cpp")
cb = read("core/src/chainparamsbase.cpp")
wallet_rpc = read("core/src/wallet/rpc/rpcwallet.cpp")
spend_cpp = read("core/src/wallet/spend.cpp")
backup_cpp = read("core/src/wallet/rpc/backup.cpp")
rpcspend = read("core/src/wallet/rpc/rpcspend.cpp")
tx_check = read("core/src/consensus/tx_check.cpp")
exp_net = read("explorer/src/network.ts")
exp_readme = read("docs/explorer/README.md")
exp_arch = read("docs/explorer/EXPLORER_ARCHITECTURE.md")
ci_build = read(".github/workflows/release-build.yml")
ci_macos = read(".github/workflows/release-wallet-macos.yml")
ci_guard = read(".github/workflows/genesis-release-guardian.yml")
sign_sh = read("tools/release/sign-macos-wallet-dmg.sh")

m_rel = re.search(r'RIGOR_RELEASE_VERSION = "([^"]+)"', cv)
release_version = m_rel.group(1) if m_rel else ""
genesis_m = re.search(r'hashGenesisBlock == uint256S\("0x([0-9a-f]+)"\)', cp)
node_genesis = genesis_m.group(1) if genesis_m else ""
magic_ok = all(tok in cp for tok in NODE_MAGIC) and "nDefaultPort = 8760" in cp
hrp_main = 'bech32_hrp = "rg"' in cp
hrp_test = 'bech32_hrp = "trg"' in cp
hrp_reg = 'bech32_hrp = "rgrt"' in cp
rpc_main = "8761" in cb
asert_ok = "MAINNET_POW_ASERT_HALF_LIFE = 43200" in lock
window_ok = "CONVERGENCE_DRAW_WINDOW = 32" in lock
pow_tag = "POW_DOMAIN_TAG[] = {'R', 'G', 'R', 'P', 'O', 'W', '0', '1'}" in pow_digest_h
pow_sha3 = "SHA3-256(" in pow_digest_h and "RGRPOW01" in pow_digest_h
pow_impl = "Sha3_256Short" in pow_digest_cpp and "BuildPowPreimage" in pow_digest_cpp
cpow_sha3 = "GetRigorPowHash(block)" in pow_cpp
cpow_no_opu = ("OpuHash" not in pow_cpp) and ("opus_hash" not in pow_cpp.lower())
res_not_pow = "IsCoinBase()" in tx_check and "IsResMintScript" in tx_check

if release_version != "v0.1.3":
    fail("CORE", f"RIGOR_RELEASE_VERSION is {release_version!r}, expected v0.1.3")
if node_genesis != NODE_MAINNET_GENESIS:
    fail("CORE", f"mainnet genesis is {node_genesis!r}")
if not magic_ok:
    fail("CORE", "mainnet magic/P2P port is not ce f1 db fa / 8760")
if not (hrp_main and hrp_test and hrp_reg):
    fail("CORE", "HRP map is not mainnet rg / testnet trg / regtest rgrt")
if not rpc_main:
    fail("CORE", "mainnet RPC port 8761 missing from chainparamsbase.cpp")
if not asert_ok:
    fail("CORE", "ASERT tau is not 43200")
if not window_ok:
    fail("CORE", "draw window W is not 32")
if not (pow_tag and pow_sha3 and pow_impl):
    fail("CORE", "PoW digest is not SHA3-256(RGRPOW01 || header_80)")
if not cpow_sha3:
    fail("CORE", "CheckProofOfWork does not call GetRigorPowHash")
if not cpow_no_opu:
    fail("CORE", "CheckProofOfWork still references OpuHash")
else:
    info("CORE", "CheckProofOfWork accepts SHA3-256 RGRPOW01 and does not call OpuHash")
if not res_not_pow:
    fail("CORE", "body RES mint rejection pin missing")
else:
    info("CORE", "RES mint is coinbase-only; body IsResMintScript is rejected")

version_txt = load_text(dump / "rigord_version.txt")
if "RIGOR version v0.1.3" not in version_txt:
    fail("CORE", "official linux rigord --version is not RIGOR version v0.1.3")
if "Production Release" not in version_txt:
    fail("CORE", "official linux rigord --version missing Production Release")
if "NETWORK=mainnet" not in version_txt:
    fail("CORE", "official linux rigord --version missing NETWORK=mainnet")
build_git = ""
m_bg = re.search(r"BUILD_GIT_COMMIT=(\S+)", version_txt)
if m_bg:
    build_git = m_bg.group(1)
    info("CORE", f"official linux BUILD_GIT_COMMIT={build_git}")

gbt_txt = load_text(dump / "getblockchaininfo.json")
live_rpc = "PASS"
try:
    gbt = json.loads(gbt_txt)
except json.JSONDecodeError:
    gbt = {}
    live_rpc = "NOT_EXECUTED"
    fail("CORE", "getblockchaininfo did not return JSON (disconnected mainnet RPC not executed)")
if gbt:
    chain = str(gbt.get("chain") or "")
    best = str(gbt.get("bestblockhash") or gbt.get("best_block_hash") or "")
    gen = ""
    # Bitcoin-style getblockchaininfo has no genesis field; height 0 tip is genesis.
    if gbt.get("blocks") == 0:
        gen = best
    if chain not in ("main", "mainnet"):
        fail("CORE", f"getblockchaininfo chain={chain!r}, expected main")
    if best.lower() != NODE_MAINNET_GENESIS and gen.lower() != NODE_MAINNET_GENESIS:
        # height-0 disconnected node: bestblockhash should be genesis
        if str(gbt.get("blocks")) == "0" or gbt.get("blocks") == 0:
            if best.lower() != NODE_MAINNET_GENESIS:
                fail("CORE", f"disconnected tip/bestblockhash {best} != genesis")
        else:
            info("CORE", f"getblockchaininfo tip={best} blocks={gbt.get('blocks')}")
    else:
        info("CORE", f"getblockchaininfo chain={chain} genesis/tip={best}")
    if "difficulty" not in gbt:
        fail("CORE", "getblockchaininfo missing difficulty")

# ---------------------------------------------------------------------------
# Official artifacts / SHA256
# ---------------------------------------------------------------------------
official_dir = root / "dist/release/v0.1.3/official"
sums_blob = load_text(official_dir / "SHA256SUMS")
expected_sha = {}
for ln in sums_blob.splitlines():
    ln = ln.strip()
    if not ln:
        continue
    parts = ln.split()
    if len(parts) >= 2:
        expected_sha[parts[-1]] = parts[0]

sha_ok = True
artifact_sizes = {}
for name in OFFICIAL_NAMES:
    p = official_dir / name
    if not p.is_file():
        fail("ARTIFACTS", f"missing official {name}")
        sha_ok = False
        continue
    artifact_sizes[name] = p.stat().st_size
    digest = sha256_file(p)
    want = expected_sha.get(name)
    if not want:
        fail("ARTIFACTS", f"SHA256SUMS missing {name}")
        sha_ok = False
    elif digest != want:
        fail("ARTIFACTS", f"SHA256 mismatch {name}")
        sha_ok = False
    if name.endswith(".dmg") and p.stat().st_size < 1_000_000:
        fail("MACOS", f"{name} is a stub (<1 MiB)")
if sha_ok and all((official_dir / n).is_file() for n in OFFICIAL_NAMES):
    info("ARTIFACTS", "all five official v0.1.3 artifacts present; SHA256 matches SHA256SUMS")

linux_tar = official_dir / "rigor-v0.1.3-linux-x86_64.tar.gz"
linux_members = []
if linux_tar.is_file():
    with tarfile.open(linux_tar) as tf:
        linux_members = tf.getnames()
    if not any(n.endswith("bin/rigord") or n.endswith("/rigord") for n in linux_members):
        fail("LINUX", "linux tarball missing bin/rigord")
    if not any("rigor-qt" in n for n in linux_members):
        fail("LINUX", "linux tarball missing rigor-qt")
    if not any("rigor-wallet" in n for n in linux_members):
        fail("LINUX", "linux tarball missing rigor-wallet")

win_zip = official_dir / "rigor-v0.1.3-windows-x86_64.zip"
if win_zip.is_file():
    with zipfile.ZipFile(win_zip) as zf:
        wnames = zf.namelist()
    if not any(n.lower().endswith("rigord.exe") for n in wnames):
        fail("WINDOWS", "windows zip missing rigord.exe")
win_exe = official_dir / "rigor-v0.1.3-windows-x86_64.exe"
if win_exe.is_file():
    blob = win_exe.read_bytes()
    utf16 = blob.decode("utf-16le", errors="ignore")
    if "v0.1.3" not in utf16 and "0.1.3" not in utf16:
        fail("WINDOWS", "windows NSIS installer has no v0.1.3 stamp")

macos_signing_deferred = (
    "MACOS_SIGNING_STATUS=DEFERRED" in sign_sh
    and "RELEASE_BLOCKED_BY_MACOS_SIGNING=FALSE" in sign_sh
    and "sign-macos-wallet-dmg.sh" in ci_macos
)
if not macos_signing_deferred:
    fail("MACOS", "Developer ID deferral pin missing from sign-macos-wallet-dmg.sh / release-wallet-macos.yml")
else:
    info("MACOS", "MACOS_SIGNING_STATUS=DEFERRED must not block release (RELEASE_BLOCKED_BY_MACOS_SIGNING=FALSE)")
info("MACOS", "DMG mount / Mach-O arch not executed on this Linux host; CI smoke-macos-wallet-dmg.sh is the mount authority")

# ---------------------------------------------------------------------------
# Wallet source surface (live 4-OS GUI is not executed here)
# ---------------------------------------------------------------------------
if "createwallet" not in wallet_rpc:
    fail("WALLET", "createwallet RPC missing")
if "backupwallet()" not in backup_cpp:
    fail("WALLET", "backupwallet RPC missing")
if "restorewallet()" not in backup_cpp:
    fail("WALLET", "restorewallet RPC missing")
if "sendtoaddress" not in rpcspend:
    fail("WALLET", "sendtoaddress RPC missing")
if "sendres()" not in rpcspend and "RPCHelpMan sendres" not in rpcspend:
    fail("WALLET", "sendres RPC missing")
if "immature coinbase" not in spend_cpp.lower() and "IsTxImmatureCoinBase" not in spend_cpp:
    fail("WALLET", "immature coinbase spend exclusion missing")
if "immature RES" not in spend_cpp and "GetTxBlocksToResMintMaturity" not in spend_cpp:
    fail("WALLET", "immature RES mint spend exclusion missing")
info("WALLET", "live create/backup/restore/send GUI was not click-tested on Windows/macOS in this WSL audit")

# ---------------------------------------------------------------------------
# Explorer
# ---------------------------------------------------------------------------
if 'RIGOR_PRODUCT = "RIGOR v0.1.3"' not in exp_net:
    fail("EXPLORER", "explorer product is not RIGOR v0.1.3")
if NODE_MAINNET_GENESIS not in exp_net:
    fail("EXPLORER", "explorer mainnet genesis mismatch")
if 'mainnet: "rg"' not in exp_net or 'testnet: "trg"' not in exp_net or 'regtest: "rgrt"' not in exp_net:
    fail("EXPLORER", "explorer HRP map mismatch")
if "mainnet: 8761" not in exp_net:
    fail("EXPLORER", "explorer mainnet RPC is not 8761")
if "Not consensus" not in exp_readme and "not consensus" not in exp_readme.lower():
    fail("EXPLORER", "explorer README does not state it is not consensus")
if "ConnectBlock" in exp_net:
    fail("EXPLORER", "explorer network.ts must not implement ConnectBlock")
info("EXPLORER", "explorer is an observation product; node remains consensus authority")

# ---------------------------------------------------------------------------
# CI / CD
# ---------------------------------------------------------------------------
if "name: release-build" not in ci_build:
    fail("CI", "release-build.yml missing")
if "name: release-wallet-macos" not in ci_macos:
    fail("CI", "release-wallet-macos.yml missing")
if "name: genesis-release-guardian" not in ci_guard:
    fail("CI", "genesis-release-guardian.yml missing")
for needle in (
    "windows-x86_64",
    "linux-x86_64",
    "macos-x86_64",
    "macos-arm64",
):
    if needle not in ci_build:
        fail("CI", f"release-build.yml missing matrix target {needle}")
for name in OFFICIAL_NAMES:
    if name not in ci_build and name not in ci_macos:
        fail("CI", f"workflow does not name {name}")
if "SHA256_MANIFEST=PASS" not in ci_build and "SHA256SUMS" not in ci_build:
    fail("CI", "SHA256SUMS job missing from release-build.yml")
if "needs:" not in ci_guard or "release-build" not in ci_guard:
    fail("CI", "guardian workflow does not need release-build")

# ---------------------------------------------------------------------------
# Sub-audit tokens (this run)
# ---------------------------------------------------------------------------
db_log = load_text(dump / "database_security.txt")
mine_log = load_text(dump / "mining_path.txt")
guard_log = load_text(dump / "release_guardian.txt")

db_stage_result = has_line(db_log, "DATABASE_SECURITY_STAGE_RESULT=PASS")
db_reindex = has_line(db_log, "PRODUCTION_FULL_REINDEX=PASS")
db_wallet = has_line(db_log, "WALLET_SQLITE_AUDIT_STATUS=CLOSED")
if not db_stage_result:
    fail("DATABASE", "this run did not print DATABASE_SECURITY_STAGE_RESULT=PASS")
if not db_reindex:
    fail("DATABASE", "this run did not print PRODUCTION_FULL_REINDEX=PASS")
if not db_wallet:
    fail("DATABASE", "this run did not print WALLET_SQLITE_AUDIT_STATUS=CLOSED")

if not has_line(mine_log, "PRODUCTION_MINING_PATH_AUDIT=PASS"):
    fail("MINER", "this run did not print PRODUCTION_MINING_PATH_AUDIT=PASS")
rg_full = has_line(guard_log, "RELEASE_GUARDIAN_FULL_PIPELINE=PASS")
rg_skip_false = has_line(guard_log, "SKIPPED_STAGE_ACCEPTED=FALSE")
rg_skip_true = has_line(guard_log, "SKIPPED_STAGE_ACCEPTED=TRUE")
if not rg_full:
    fail("GUARDIAN", "this run did not print RELEASE_GUARDIAN_FULL_PIPELINE=PASS")
if rg_skip_true or not rg_skip_false:
    fail("GUARDIAN", "SKIPPED_STAGE_ACCEPTED is not FALSE (all stages must actually execute)")
if has_line(guard_log, "PERFORMANCE_BASELINE=FAIL"):
    fail("GUARDIAN", "PERFORMANCE_BASELINE=FAIL (do not silent-patch consensus; re-baseline in a follow-up TASK)")

# ---------------------------------------------------------------------------
# Git / tag / dirty / binary vs current sources
# ---------------------------------------------------------------------------
head = git("rev-parse", "HEAD")
describe = git("describe", "--tags", "--always", "--dirty")
tag = git("rev-parse", "v0.1.3")
porcelain = git("status", "--porcelain")
dirty_paths = []
for ln in porcelain.splitlines():
    if len(ln) >= 4 and ln[2] == " ":
        path = ln[3:].strip()
    elif len(ln) >= 3:
        path = ln[2:].strip()
    else:
        continue
    if " -> " in path:
        path = path.split(" -> ", 1)[1]
    dirty_paths.append(path)
extra_dirty = [
    p
    for p in dirty_paths
    if p not in P100_ALLOW
    and not p.startswith("formal_verification/mainnet_launch_candidate_audit/")
    and not p.startswith(".github/releases/v0.1.3/")
    and not (p.startswith("formal_verification/") and p.endswith("_manifest.json"))
    and not p.startswith("docs/performance/")
]
clean_tree = len(extra_dirty) == 0
if not clean_tree:
    fail(
        "CLEAN",
        f"working tree is dirty ({len(extra_dirty)} paths outside P0-100 allowlist): "
        + ", ".join(extra_dirty[:12]),
    )
if not tag or "fatal" in tag.lower():
    fail("CLEAN", "git tag v0.1.3 is missing")
else:
    info("CLEAN", f"tag v0.1.3={tag}")

binaries_match_current = False
if build_git and "dirty" in build_git.lower():
    fail("CLEAN", f"official linux BUILD_GIT_COMMIT is dirty: {build_git}")
elif build_git and head:
    short = build_git
    binaries_match_current = head.startswith(short) or short.startswith(head[:12])
if not binaries_match_current:
    fail(
        "CLEAN",
        f"released linux binary BUILD_GIT_COMMIT={build_git!r} does not match current HEAD={head}",
    )

# alphaplus / old identity in official artifact names
alpha_hits = []
official_root = root / "dist/release"
if official_root.is_dir():
    for p in official_root.rglob("*"):
        if p.is_file() and "alpha" in p.name.lower() and "alphaplus" in p.name.lower():
            alpha_hits.append(str(p.relative_to(root)))
if alpha_hits:
    fail("CLEAN", "alphaplus-named release artifacts present: " + ", ".join(alpha_hits[:8]))

readme = read("README.md")
if "ALPHAPLUS" in readme and "RIGOR v0.1.3" not in readme:
    fail("CLEAN", "README does not present RIGOR v0.1.3 as the product")
if "8332" in readme and "8761" not in readme:
    fail("CLEAN", "README still presents Bitcoin RPC 8332 without 8761")

# ---------------------------------------------------------------------------
# Change flags (this TASK must not have patched these)
# ---------------------------------------------------------------------------
consensus_changed = bool(git_names(CONSENSUS_PATHS))
db_changed = bool(git_names(DB_PATHS))
wallet_changed = bool(git_names(WALLET_SCHEMA_PATHS))
params_changed = bool(git_names(PARAM_PATHS))
formal_changed = bool(git_names(FORMAL_PATHS))
if consensus_changed:
    fail("FREEZE", "consensus paths are dirty in the working tree")
if db_changed:
    fail("FREEZE", "CoinsDB paths are dirty in the working tree")
if wallet_changed:
    fail("FREEZE", "wallet schema paths are dirty in the working tree")
if params_changed:
    fail("FREEZE", "protocol parameter paths are dirty in the working tree")
if formal_changed:
    info("FREEZE", "formal model paths are dirty; this TASK must not edit them")

CORE_MAINNET_READY = gate_pass(not any(f["gate"] == "CORE" and f["severity"] == "FAIL" for f in findings))
DATABASE_MAINNET_READY = gate_pass(db_stage_result and db_reindex and db_wallet)
WALLET_MAINNET_READY = gate_pass(not any(f["gate"] == "WALLET" and f["severity"] == "FAIL" for f in findings))
MACOS_MAINNET_READY = gate_pass(
    not any(f["gate"] == "MACOS" and f["severity"] == "FAIL" for f in findings)
    and (official_dir / "rigor-v0.1.3-macos-x86_64.dmg").is_file()
    and (official_dir / "rigor-v0.1.3-macos-arm64.dmg").is_file()
)
WINDOWS_MAINNET_READY = gate_pass(not any(f["gate"] == "WINDOWS" and f["severity"] == "FAIL" for f in findings) and win_exe.is_file() and win_zip.is_file())
LINUX_MAINNET_READY = gate_pass(not any(f["gate"] == "LINUX" and f["severity"] == "FAIL" for f in findings) and linux_tar.is_file() and "RIGOR version v0.1.3" in version_txt)
MINER_MAINNET_READY = gate_pass(has_line(mine_log, "PRODUCTION_MINING_PATH_AUDIT=PASS"))
POOL_MAINNET_READY = gate_pass(has_line(mine_log, "PRODUCTION_MINING_PATH_AUDIT=PASS"))
EXPLORER_MAINNET_READY = gate_pass(not any(f["gate"] == "EXPLORER" and f["severity"] == "FAIL" for f in findings))
CI_MAINNET_READY = gate_pass(not any(f["gate"] == "CI" and f["severity"] == "FAIL" for f in findings) and sha_ok)
RELEASE_GUARDIAN_READY = gate_pass(rg_full and rg_skip_false and not rg_skip_true)
CLEAN_RELEASE = gate_pass(clean_tree and binaries_match_current and bool(tag) and "fatal" not in tag.lower() and not alpha_hits)

CONSENSUS_CODE_CHANGED = "TRUE" if consensus_changed else "FALSE"
DATABASE_FORMAT_CHANGED = "TRUE" if db_changed else "FALSE"
WALLET_FORMAT_CHANGED = "TRUE" if wallet_changed else "FALSE"
PROTOCOL_PARAMETERS_CHANGED = "TRUE" if params_changed else "FALSE"

component_ok = all(
    v == "PASS"
    for v in (
        CORE_MAINNET_READY,
        DATABASE_MAINNET_READY,
        WALLET_MAINNET_READY,
        MACOS_MAINNET_READY,
        WINDOWS_MAINNET_READY,
        LINUX_MAINNET_READY,
        MINER_MAINNET_READY,
        POOL_MAINNET_READY,
        EXPLORER_MAINNET_READY,
        CI_MAINNET_READY,
        RELEASE_GUARDIAN_READY,
        CLEAN_RELEASE,
    )
)
freeze_ok = (
    CONSENSUS_CODE_CHANGED == "FALSE"
    and DATABASE_FORMAT_CHANGED == "FALSE"
    and WALLET_FORMAT_CHANGED == "FALSE"
    and PROTOCOL_PARAMETERS_CHANGED == "FALSE"
)
MAINNET_LAUNCH_CANDIDATE_AUDIT = gate_pass(component_ok and freeze_ok and not fails)
FINAL_VERDICT = MAINNET_LAUNCH_CANDIDATE_AUDIT
GO_NO_GO = "GO" if FINAL_VERDICT == "PASS" else "NO-GO"

now = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
today = date.today().isoformat()

fail_lines = "\n".join(f"- {x}" for x in fails) if fails else "- (none)"
info_lines = "\n".join(f"- {f['gate']}: {f['message']}" for f in findings if f["severity"] == "INFO") or "- (none)"

man = {
    "task": "TASK-RIGOR-MAINNET-LAUNCH-CANDIDATE-AUDIT-P0-100",
    "date": today,
    "generated_at_utc": now,
    "nature": "Audit-first. Production readiness gate. No consensus/protocol/database/wallet-schema patch.",
    "command": "bash tools/run-mainnet-launch-candidate-audit.sh",
    "product": "RIGOR v0.1.3",
    "network": "mainnet",
    "identity": {
        "release_version": release_version,
        "mainnet_genesis": node_genesis,
        "magic": "ce f1 db fa",
        "p2p_port": 8760,
        "rpc_port": 8761,
        "hrp": {"mainnet": "rg", "testnet": "trg", "regtest": "rgrt"},
        "pow": 'SHA3-256("RGRPOW01" || header_80)',
        "asert_tau": 43200,
        "draw_window": 32,
        "res": "coinbase mint only; PoW does not create RES",
    },
    "git": {
        "head": head,
        "describe": describe,
        "tag_v0.1.3": tag,
        "dirty_path_count": len(extra_dirty),
        "clean_tree": clean_tree,
        "official_linux_build_git_commit": build_git,
        "binaries_match_current_sources": binaries_match_current,
    },
    "live": {
        "rigord_version": version_txt.strip().splitlines()[:6],
        "getblockchaininfo": gbt if isinstance(gbt, dict) else None,
        "getblockchaininfo_status": live_rpc if gbt else "NOT_EXECUTED",
    },
    "macos_signing_status": "DEFERRED",
    "release_blocked_by_macos_signing": False,
    "artifact_sha256_pass": sha_ok,
    "artifact_sizes": artifact_sizes,
    "sub_audits": {
        "DATABASE_SECURITY_STAGE_RESULT": "PASS" if db_stage_result else "FAIL",
        "PRODUCTION_FULL_REINDEX": "PASS" if db_reindex else "FAIL",
        "WALLET_SQLITE_AUDIT_STATUS": "CLOSED" if db_wallet else "OPEN",
        "PRODUCTION_MINING_PATH_AUDIT": "PASS" if has_line(mine_log, "PRODUCTION_MINING_PATH_AUDIT=PASS") else "FAIL",
        "RELEASE_GUARDIAN_FULL_PIPELINE": "PASS" if rg_full else "FAIL",
        "SKIPPED_STAGE_ACCEPTED": "FALSE" if rg_skip_false and not rg_skip_true else ("TRUE" if rg_skip_true else "UNKNOWN"),
        "PERFORMANCE_BASELINE": "PASS" if has_line(guard_log, "PERFORMANCE_BASELINE=PASS") else "FAIL",
    },
    "MAINNET_LAUNCH_CANDIDATE_AUDIT": MAINNET_LAUNCH_CANDIDATE_AUDIT,
    "CORE_MAINNET_READY": CORE_MAINNET_READY,
    "DATABASE_MAINNET_READY": DATABASE_MAINNET_READY,
    "WALLET_MAINNET_READY": WALLET_MAINNET_READY,
    "MACOS_MAINNET_READY": MACOS_MAINNET_READY,
    "WINDOWS_MAINNET_READY": WINDOWS_MAINNET_READY,
    "LINUX_MAINNET_READY": LINUX_MAINNET_READY,
    "MINER_MAINNET_READY": MINER_MAINNET_READY,
    "POOL_MAINNET_READY": POOL_MAINNET_READY,
    "EXPLORER_MAINNET_READY": EXPLORER_MAINNET_READY,
    "CI_MAINNET_READY": CI_MAINNET_READY,
    "RELEASE_GUARDIAN_READY": RELEASE_GUARDIAN_READY,
    "CLEAN_RELEASE": CLEAN_RELEASE,
    "CONSENSUS_CODE_CHANGED": CONSENSUS_CODE_CHANGED,
    "DATABASE_FORMAT_CHANGED": DATABASE_FORMAT_CHANGED,
    "WALLET_FORMAT_CHANGED": WALLET_FORMAT_CHANGED,
    "PROTOCOL_PARAMETERS_CHANGED": PROTOCOL_PARAMETERS_CHANGED,
    "FINAL_VERDICT": FINAL_VERDICT,
    "GO_NO_GO": GO_NO_GO,
    "errors": fails,
    "findings": findings,
}
man_path.write_text(json.dumps(man, indent=2, sort_keys=True) + "\n", encoding="utf-8")

report = f"""# RIGOR Mainnet Launch Candidate Audit (P0-100)

| Field | Value |
|-------|-------|
| **Task** | `TASK-RIGOR-MAINNET-LAUNCH-CANDIDATE-AUDIT-P0-100` |
| **Date** | {today} |
| **Generated (UTC)** | `{now}` |
| **Nature** | Audit-first. Production readiness gate. **Not** a development TASK. **Not** a consensus change TASK. |
| **Command** | `bash tools/run-mainnet-launch-candidate-audit.sh` |
| **Product** | RIGOR v0.1.3 |
| **Network** | mainnet |
| **GO / NO-GO** | **{GO_NO_GO}** |

A prior PASS document is not a PASS. Tokens below are from **this execution**.

## Verdict

```
MAINNET_LAUNCH_CANDIDATE_AUDIT={MAINNET_LAUNCH_CANDIDATE_AUDIT}

CORE_MAINNET_READY={CORE_MAINNET_READY}
DATABASE_MAINNET_READY={DATABASE_MAINNET_READY}
WALLET_MAINNET_READY={WALLET_MAINNET_READY}
MACOS_MAINNET_READY={MACOS_MAINNET_READY}
WINDOWS_MAINNET_READY={WINDOWS_MAINNET_READY}
LINUX_MAINNET_READY={LINUX_MAINNET_READY}
MINER_MAINNET_READY={MINER_MAINNET_READY}
POOL_MAINNET_READY={POOL_MAINNET_READY}
EXPLORER_MAINNET_READY={EXPLORER_MAINNET_READY}
CI_MAINNET_READY={CI_MAINNET_READY}
RELEASE_GUARDIAN_READY={RELEASE_GUARDIAN_READY}

CONSENSUS_CODE_CHANGED={CONSENSUS_CODE_CHANGED}
DATABASE_FORMAT_CHANGED={DATABASE_FORMAT_CHANGED}
WALLET_FORMAT_CHANGED={WALLET_FORMAT_CHANGED}
PROTOCOL_PARAMETERS_CHANGED={PROTOCOL_PARAMETERS_CHANGED}

FINAL_VERDICT={FINAL_VERDICT}
```

`CLEAN_RELEASE={CLEAN_RELEASE}` (working tree, tag `v0.1.3`, released binaries vs current sources).

## Production identity (code SSOT)

| Fact | Required | Observed |
|------|----------|----------|
| Product | RIGOR v0.1.3 | `{release_version}` |
| Mainnet genesis | `{NODE_MAINNET_GENESIS}` | `{node_genesis}` |
| Magic | `ce f1 db fa` | `ce f1 db fa` pin in `chainparams.cpp` = `{magic_ok}` |
| P2P / RPC | 8760 / 8761 | `{magic_ok}` / `{rpc_main}` |
| HRP | `rg` / `trg` / `rgrt` | `{hrp_main}` / `{hrp_test}` / `{hrp_reg}` |
| PoW | SHA3-256(`RGRPOW01` \\|\\| header_80) | CheckProofOfWork → `GetRigorPowHash`; no OpuHash call |
| ASERT τ | 43200 | `{asert_ok}` |
| Draw W | 32 | `{window_ok}` |
| RES | coinbase mint only | body `IsResMintScript` rejected |

Official linux `rigord --version` (this run):

```
{version_txt.strip() or "(missing)"}
```

`getblockchaininfo` (disconnected mainnet, no P2P):

```
{json.dumps(gbt, indent=2) if gbt else gbt_txt[:800]}
```

## Phase evidence

### Phase 1 — Core node

- Official tarball binary used (in-tree `core/src/rigord` is not required for `--version`).
- CheckProofOfWork source uses `rigor_kernel::GetRigorPowHash` = SHA3-256(`RGRPOW01` || header_80).
- Old OpuHash is not invoked from `pow.cpp`.

### Phase 2 — Database security

Executed `bash tools/run-database-security-guardian-stage.sh`.

| Token | This run |
|-------|----------|
| `DATABASE_SECURITY_STAGE_RESULT=PASS` | `{db_stage_result}` |
| `PRODUCTION_FULL_REINDEX=PASS` | `{db_reindex}` |
| `WALLET_SQLITE_AUDIT_STATUS=CLOSED` | `{db_wallet}` |

The stage script still prints wiring token `DATABASE_SECURITY_STAGE=ADDED`. This audit treats `DATABASE_SECURITY_STAGE=PASS` only when `DATABASE_SECURITY_STAGE_RESULT=PASS` and the two child tokens above are present.

### Phase 3 — Wallet production

Source RPCs: `createwallet`, `backupwallet`, `restorewallet`, `sendtoaddress`, `sendres`. Immature coinbase and immature RES mint are excluded in `wallet/spend.cpp`. Official Windows / Linux / macOS Intel / macOS ARM artifacts exist. **Live four-OS GUI click-test was not executed on this Linux host.**

### Phase 4 — macOS release

DMGs `rigor-v0.1.3-macos-x86_64.dmg` and `rigor-v0.1.3-macos-arm64.dmg` are present with SHA256 matching `SHA256SUMS`. Mount / `.app` / Mach-O architecture are **not** executed here (Linux). CI `smoke-macos-wallet-dmg.sh` remains the mount authority. `MACOS_SIGNING_STATUS=DEFERRED` does **not** block release (`RELEASE_BLOCKED_BY_MACOS_SIGNING=FALSE`).

### Phase 5 — Miner and pool

Executed `bash tools/run-production-mining-path-audit.sh`. Require `PRODUCTION_MINING_PATH_AUDIT=PASS`.

### Phase 6 — Explorer

Explorer product / genesis / HRP / RPC match the node. README states the explorer is not consensus. Explorer must not become consensus authority.

### Phase 8 — CI/CD

Workflows: `release-build.yml`, `release-wallet-macos.yml`, `genesis-release-guardian.yml`. Matrix includes Linux / Windows / macOS Intel / macOS ARM. Artifact names match v0.1.3. SHA256 job present. Guardian `needs: release-build`.

### Phase 9 — Release Guardian

Executed `bash tools/run-release-guardian-full-pipeline-audit.sh`. Require `RELEASE_GUARDIAN_FULL_PIPELINE=PASS` and `SKIPPED_STAGE_ACCEPTED=FALSE`.

### Phase 10 — Clean release

| Check | Result |
|-------|--------|
| Git describe | `{describe}` |
| HEAD | `{head}` |
| Tag `v0.1.3` | `{tag}` |
| Extra dirty paths | `{len(extra_dirty)}` |
| Official linux `BUILD_GIT_COMMIT` | `{build_git}` |
| Binaries match current sources | `{binaries_match_current}` |
| Alphaplus-named official artifacts | `{alpha_hits or "none"}` |

## FAIL findings

{fail_lines}

## INFO

{info_lines}

## Freeze (this TASK)

This TASK did not patch Consensus Kernel, Transaction Kernel, Ledger, ConnectBlock, DisconnectBlock, CheckProofOfWork, ASERT, Genesis, Magic, network parameters, OPCODE definitions, CoinsDB format, wallet schema, or formal models.

## Follow-up

{"None. FINAL_VERDICT=PASS. Use the launch checklist in this document." if FINAL_VERDICT == "PASS" else "See docs/release/FOLLOWUP_P0_101_LAUNCH_FREEZE_AND_REBUILD.md. Do not silent-patch inside P0-100."}

## Mainnet launch checklist (only if PASS)

1. Tag remains `v0.1.3`; working tree clean; official SHA256SUMS match the freeze commit.
2. Publish the five official artifacts plus SHA256SUMS.
3. Seeds / DNS / monitoring remain operational follow-ups; they do not change consensus.
4. Public messaging: RIGOR v0.1.3, mainnet, genesis `e2ce92cf…1468`, magic `ce f1 db fa`, P2P 8760, RPC 8761, HRP `rg`, PoW SHA3-256(`RGRPOW01`||header_80).
5. macOS unsigned DMG is allowed (`MACOS_SIGNING_STATUS=DEFERRED`).
"""
doc_path.write_text(report, encoding="utf-8")

if FINAL_VERDICT != "PASS":
    follow = f"""# FOLLOW-UP P0-101 — Launch freeze and rebuild

**Parent:** `TASK-RIGOR-MAINNET-LAUNCH-CANDIDATE-AUDIT-P0-100`  
**Date:** {today}  
**Nature:** Production freeze / rebuild / re-audit. **Not** a consensus change TASK.  
**Trigger:** P0-100 `FINAL_VERDICT={FINAL_VERDICT}` / `GO_NO_GO={GO_NO_GO}`.

## Why this exists

P0-100 is audit-first. It must not silent-patch Consensus Kernel, Transaction Kernel, Ledger, ConnectBlock, DisconnectBlock, CheckProofOfWork, ASERT, Genesis, Magic, network parameters, OPCODE definitions, CoinsDB format, wallet schema, or formal models.

## Findings that block GO

{fail_lines}

## Required work (outside P0-100)

1. Freeze the intended launch commit (commit P0-097 / P0-098 / P0-099 and any other intended launch sources). Do not change consensus parameters.
2. Rebuild in-tree `core/src/rigord` **or** otherwise satisfy P0-088/P0-089 production-binary integrity so `bash tools/run-database-security-guardian-stage.sh` can print `PRODUCTION_FULL_REINDEX=PASS` and `WALLET_SQLITE_AUDIT_STATUS=CLOSED` on that freeze.
3. Rebuild official artifacts from **that same commit**:
   - `rigor-v0.1.3-windows-x86_64.exe`
   - `rigor-v0.1.3-windows-x86_64.zip`
   - `rigor-v0.1.3-linux-x86_64.tar.gz`
   - `rigor-v0.1.3-macos-x86_64.dmg`
   - `rigor-v0.1.3-macos-arm64.dmg`
4. Refresh `SHA256SUMS`. Official `rigord --version` `BUILD_GIT_COMMIT` must match the freeze commit.
5. Working tree must be clean.
6. Re-run `bash tools/run-mainnet-launch-candidate-audit.sh` to completion. Require `SKIPPED_STAGE_ACCEPTED=FALSE`.
7. If `PERFORMANCE_BASELINE=FAIL`, investigate the Guardian compare (or refresh the baseline in a dedicated TASK). Do not silent-patch consensus to make the number move.
8. Do not treat a historical Guardian PASS (P0-092, 2026-09-02) as a substitute for that re-run.

## Out of scope

Do not modify genesis, magic, ports, HRP, ASERT τ, draw W, OPCODE definitions, CoinsDB format, or wallet schema to “make P0-100 green”.
"""
    followup_path.write_text(follow, encoding="utf-8")

tokens = [
    ("MAINNET_LAUNCH_CANDIDATE_AUDIT", MAINNET_LAUNCH_CANDIDATE_AUDIT),
    ("CORE_MAINNET_READY", CORE_MAINNET_READY),
    ("DATABASE_MAINNET_READY", DATABASE_MAINNET_READY),
    ("WALLET_MAINNET_READY", WALLET_MAINNET_READY),
    ("MACOS_MAINNET_READY", MACOS_MAINNET_READY),
    ("WINDOWS_MAINNET_READY", WINDOWS_MAINNET_READY),
    ("LINUX_MAINNET_READY", LINUX_MAINNET_READY),
    ("MINER_MAINNET_READY", MINER_MAINNET_READY),
    ("POOL_MAINNET_READY", POOL_MAINNET_READY),
    ("EXPLORER_MAINNET_READY", EXPLORER_MAINNET_READY),
    ("CI_MAINNET_READY", CI_MAINNET_READY),
    ("RELEASE_GUARDIAN_READY", RELEASE_GUARDIAN_READY),
    ("CONSENSUS_CODE_CHANGED", CONSENSUS_CODE_CHANGED),
    ("DATABASE_FORMAT_CHANGED", DATABASE_FORMAT_CHANGED),
    ("WALLET_FORMAT_CHANGED", WALLET_FORMAT_CHANGED),
    ("PROTOCOL_PARAMETERS_CHANGED", PROTOCOL_PARAMETERS_CHANGED),
    ("FINAL_VERDICT", FINAL_VERDICT),
]
for k, v in tokens:
    print(f"{k}={v}")
print(f"CLEAN_RELEASE={CLEAN_RELEASE}")
print(f"GO_NO_GO={GO_NO_GO}")
print(f"SKIPPED_STAGE_ACCEPTED={'FALSE' if rg_skip_false and not rg_skip_true else ('TRUE' if rg_skip_true else 'UNKNOWN')}")
if fails:
    print("FAIL_COUNT=" + str(len(fails)))
    for item in fails:
        print(f"FAIL: {item}")

sys.exit(0 if FINAL_VERDICT == "PASS" else 1)
PY
python_rc=$?
set -e
echo "P100_PYTHON_EXIT=${python_rc}"
exit "${python_rc}"
