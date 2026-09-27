#!/usr/bin/env bash
# P0-099: production mining path audit + isolated dry-run.
# Audit-first. Miner/pool/test tooling only.
# Does not modify Consensus Kernel, CheckProofOfWork, ConnectBlock,
# ASERT, genesis, CoinsDB, wallet schema, or Release Guardian.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
export PYTHONUNBUFFERED=1

MANIFEST="$ROOT/formal_verification/production_mining_path_manifest.json"

echo "== P099 production mining path audit =="
echo "command=bash tools/run-production-mining-path-audit.sh"
echo "nature=audit-first mining path; consensus frozen"

python3 - "$ROOT" "$MANIFEST" <<'PY'
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
from datetime import date
from pathlib import Path

root = Path(sys.argv[1])
man_path = Path(sys.argv[2])


def read(rel: str) -> str:
    p = root / rel
    if not p.is_file():
        return ""
    return p.read_text(encoding="utf-8", errors="replace")


def exists(rel: str) -> bool:
    return (root / rel).is_file()


def git_names(paths: list[str]) -> set[str]:
    r = subprocess.run(
        ["git", "-C", str(root), "diff", "--name-only", "--"] + paths,
        capture_output=True,
        text=True,
        check=False,
    )
    return {ln.strip() for ln in r.stdout.splitlines() if ln.strip()}


def git_describe() -> tuple[str, str]:
    d = subprocess.run(
        ["git", "-C", str(root), "describe", "--tags", "--always", "--dirty"],
        capture_output=True,
        text=True,
        check=False,
    )
    h = subprocess.run(
        ["git", "-C", str(root), "rev-parse", "HEAD"],
        capture_output=True,
        text=True,
        check=False,
    )
    return (d.stdout or "").strip(), (h.stdout or "").strip()


def run(cmd: list[str], cwd: Path | None = None, timeout: int = 300) -> tuple[int, str]:
    r = subprocess.run(
        cmd,
        cwd=str(cwd or root),
        capture_output=True,
        text=True,
        check=False,
        timeout=timeout,
    )
    return r.returncode, (r.stdout or "") + (r.stderr or "")


findings: list[dict] = []
fails: list[str] = []


def fail(gate: str, msg: str) -> None:
    fails.append(f"{gate}: {msg}")
    findings.append({"severity": "FAIL", "gate": gate, "message": msg})


def info(gate: str, msg: str) -> None:
    findings.append({"severity": "INFO", "gate": gate, "message": msg})


# --- Frozen consensus / DB / wallet ---
CONSENSUS_PATHS = [
    "core/src/consensus",
    "core/src/validation.cpp",
    "core/src/pow.cpp",
    "core/src/pow.h",
    "core/src/rigor_kernel",
    "core/src/primitives",
    "core/src/kernel/chainparams.cpp",
]
DB_PATHS = [
    "core/src/coins.cpp",
    "core/src/coins.h",
    "core/src/txdb.cpp",
    "core/src/txdb.h",
    "core/src/undo.h",
    "core/src/compressor.h",
]
WALLET_PATHS = [
    "core/src/wallet/sqlite.cpp",
    "core/src/wallet/walletdb.cpp",
    "core/src/wallet/walletdb.h",
]
GUARDIAN = ["tools/release-guardian", "infra/ci"]

consensus_changed = sorted(git_names(CONSENSUS_PATHS))
db_changed = sorted(git_names(DB_PATHS))
wallet_changed = sorted(git_names(WALLET_PATHS))
if consensus_changed:
    fail("CONSENSUS_CODE_CHANGED", ", ".join(consensus_changed))
if db_changed:
    fail("DATABASE_FORMAT_CHANGED", ", ".join(db_changed))
if wallet_changed:
    fail("WALLET_FORMAT_CHANGED", ", ".join(wallet_changed))

CONSENSUS_CODE_CHANGED = "TRUE" if consensus_changed else "FALSE"
DATABASE_FORMAT_CHANGED = "TRUE" if db_changed else "FALSE"
WALLET_FORMAT_CHANGED = "TRUE" if wallet_changed else "FALSE"

pow_digest = read("core/src/rigor_kernel/pow_digest.h")
pow_cpp = read("core/src/pow.cpp")
val = read("core/src/validation.cpp")
lock = read("core/src/consensus/rigor_consensus_lock.h")
if "GetRigorPowHash" not in pow_cpp or "RGRPOW01" not in pow_digest:
    fail("CONNECT_BLOCK_TEST", "node PoW SSOT missing")
if "CheckProofOfWork" not in val or "high-hash" not in val:
    fail("INVALID_POW_REJECTION_TEST", "ConnectBlock high-hash reject missing")
if "ORU_GENESIS_BLOCK_SUBSIDY = 0" not in lock:
    fail("ORU_REWARD_TEST", "genesis subsidy lock missing")
if "ORU_INITIAL_BLOCK_SUBSIDY = 1000 * COIN" not in lock:
    fail("ORU_REWARD_TEST", "height>=1 ORU base missing")
if "COINBASE_MATURITY = 100" not in read("core/src/consensus/consensus.h"):
    fail("COINBASE_MATURITY_TEST", "COINBASE_MATURITY is not 100")
if "RIGOR_MARKER_RES_MINT" not in read("core/src/script/rigor_bytes.h"):
    fail("RES_POW_CREATION_TEST", "RES mint marker missing from classifier")

# --- Pool configuration ---
example = read("pool/config/example.toml")
pool_readme = read("pool/README.md")
gbt_rpc = read("pool/rigor-gbt/src/rpc.rs")
pilot_cfg = read("pool/pilot/src/config.rs")
if 'network = "mainnet"' not in example or "rpc_port = 8761" not in example:
    fail("POOL_CONFIGURATION_ALIGNMENT", "example.toml not mainnet/8761")
if re.search(r"rpc_port\s*=\s*8332", example):
    fail("POOL_CONFIGURATION_ALIGNMENT", "Bitcoin 8332 in production example")
if "8332" in example and "Never Bitcoin 8332" not in example:
    fail("POOL_CONFIGURATION_ALIGNMENT", "8332 without never-Bitcoin warning")
if "8761" not in pool_readme or "getblocktemplate" not in gbt_rpc or "submitblock" not in gbt_rpc:
    fail("POOL_CONFIGURATION_ALIGNMENT", "pool GBT/submitblock/8761 missing")
if "8761" not in pilot_cfg:
    fail("POOL_CONFIGURATION_ALIGNMENT", "pilot default RPC not 8761")
if "payout_script_hex" not in example or "worker_credentials" not in example:
    fail("POOL_CONFIGURATION_ALIGNMENT", "payout/worker identity missing from example")

# --- GBT / header alignment ---
gbt_c = read("miner/gbt_solo.c")
hdr_rs = read("pool/share-validator/src/header.rs")
sha2 = read("miner/sha2.c")
if "default_coinbase_tx" not in gbt_c or "header_words[19]" not in gbt_c:
    fail("GBT_ALIGNMENT", "miner GBT copy-bytes/header_words missing")
if "version.to_le_bytes()" not in hdr_rs or "nonce.to_le_bytes()" not in hdr_rs:
    fail("GBT_ALIGNMENT", "pool header endian missing")
if "rgrpow_hash80" not in sha2:
    fail("GBT_ALIGNMENT", "minerd scanhash not RGRPOW01")

# --- Stratum ---
orch = read("pool/stratum-server/src/orchestrator.rs")
agent_proto = read("pool/stratum-server/src/protocol.rs")
sv = read("pool/share-validator/src/validator.rs")
rgr = read("pool/share-validator/src/rgrpow.rs")
worker = read("pool/rigor-miner-agent/src/pow/worker.rs")
if "requires_rgrpow01_agent" not in agent_proto or "requires_rgrpow01_agent" not in orch:
    fail("STRATUM_ALIGNMENT", "Stratum rgrpow01 agent gate missing")
if "rgrpow_hash80" not in rgr or "verify_rgrpow_share" not in sv:
    fail("STRATUM_ALIGNMENT", "pool share-validator not on RGRPOW01")
if "rgrpow_hash80" not in worker:
    fail("STRATUM_ALIGNMENT", "miner-agent not hashing RGRPOW01")
if "OpuHash agents are rejected" not in orch:
    fail("OPUHASH_REJECTION_TEST", "Stratum still accepts OpuHash agents")

# --- Docs / deliverables ---
for rel in [
    "docs/miner/README.md",
    "docs/pool/PRODUCTION_MINING_FLOW.md",
    "docs/security/RIGOR_PRODUCTION_MINING_PATH_AUDIT_P0_099.md",
    "tools/run-production-mining-path-audit.sh",
    "tools/production_mining_path_dryrun.py",
]:
    if not exists(rel):
        fail("DELIVERABLE", f"missing {rel}")
flow = read("docs/pool/PRODUCTION_MINING_FLOW.md") + read("docs/miner/README.md")
for needle in ["SHA3-256(\"RGRPOW01\" || header_80)", "8761", "Miner", "Pool", "GBT", "RGRPOW01", "rigord"]:
    if needle not in flow:
        fail("DELIVERABLE", f"docs missing {needle!r}")

# --- Isolated dry-run ---
dry_rc, dry_out = run(["python3", str(root / "tools/production_mining_path_dryrun.py")])
if dry_rc != 0 or "DRYRUN_VERDICT=PASS" not in dry_out:
    fail("SOLO_MINING_TEST", "dry-run failed:\n" + dry_out[-1200:])

def dry_has(token: str) -> bool:
    return f"PASS {token}" in dry_out

# --- miner test_gbt_solo ---
solo_out = ""
if (root / "miner/Makefile").is_file():
    brc, bout = run(["make", "-C", str(root / "miner"), "test_gbt_solo"], timeout=120)
    if brc != 0:
        fail("SOLO_MINING_TEST", "make test_gbt_solo failed")
    else:
        trc, tout = run([str(root / "miner/test_gbt_solo")], cwd=root / "miner")
        solo_out = tout
        if trc != 0 or "ALL PASS" not in tout:
            fail("SOLO_MINING_TEST", "test_gbt_solo failed")
        if "SHA3_POW_VECTOR" not in tout:
            fail("SHA3_POW_VECTOR_TEST", "miner vectors missing")
else:
    info("SOLO_MINING_TEST", "miner/Makefile missing; relying on dry-run")

# --- pool cargo tests (tooling) ---
pool_ok = True
cargo = subprocess.run(["bash", "-lc", "command -v cargo"], capture_output=True, text=True)
if cargo.returncode != 0:
    fail("POOL_MINING_TEST", "cargo not installed")
    pool_ok = False
else:
    crates = [
        root / "pool/share-validator",
        root / "pool/stratum-server",
        root / "pool/rigor-miner-agent",
        root / "pool/pilot",
    ]
    for crate in crates:
        if not (crate / "Cargo.toml").is_file():
            fail("POOL_MINING_TEST", f"missing {crate}")
            pool_ok = False
            continue
        crc, cout = run(["cargo", "test", "--offline"], cwd=crate, timeout=240)
        if crc != 0:
            crc, cout = run(["cargo", "test"], cwd=crate, timeout=300)
        if crc != 0:
            fail("POOL_MINING_TEST", f"{crate.name} cargo test failed:\n" + cout[-1500:])
            pool_ok = False
        else:
            info("POOL_MINING_TEST", f"{crate.name} cargo test ok")

# --- Gate rollup ---
def gate_from(name: str, extra_ok: bool, dry_token: str | None = None) -> str:
    if any(f.startswith(name + ":") for f in fails):
        return "FAIL"
    if dry_token and not dry_has(dry_token):
        fail(name, f"dry-run missing PASS {dry_token}")
        return "FAIL"
    return "PASS" if extra_ok else "FAIL"

POOL_CONFIGURATION_ALIGNMENT = gate_from("POOL_CONFIGURATION_ALIGNMENT", True)
GBT_ALIGNMENT = gate_from("GBT_ALIGNMENT", True, "GBT_ALIGNMENT")
STRATUM_ALIGNMENT = gate_from("STRATUM_ALIGNMENT", True, "STRATUM_ALIGNMENT")
SOLO_MINING_TEST = "FAIL" if any(f.startswith("SOLO_MINING_TEST:") for f in fails) else (
    "PASS" if dry_has("SOLO_MINING_TEST") else "FAIL"
)
POOL_MINING_TEST = "FAIL" if any(f.startswith("POOL_MINING_TEST:") for f in fails) else (
    "PASS" if pool_ok else "FAIL"
)
BLOCK_SUBMISSION_TEST = gate_from("BLOCK_SUBMISSION_TEST", True, "BLOCK_SUBMISSION_TEST")
CONNECT_BLOCK_TEST = gate_from("CONNECT_BLOCK_TEST", True, "CONNECT_BLOCK_TEST")
COINBASE_MATURITY_TEST = gate_from("COINBASE_MATURITY_TEST", True, "COINBASE_MATURITY_TEST")
ORU_REWARD_TEST = gate_from("ORU_REWARD_TEST", True, "ORU_REWARD_TEST")
RES_POW_CREATION_TEST = gate_from("RES_POW_CREATION_TEST", True, "RES_POW_CREATION_TEST")
INVALID_POW_REJECTION_TEST = gate_from("INVALID_POW_REJECTION_TEST", True, "INVALID_POW_REJECTION_TEST")
OPUHASH_REJECTION_TEST = gate_from("OPUHASH_REJECTION_TEST", True, "OPUHASH_REJECTION_TEST")

# SHA3 vectors: dry-run + miner test if present
SHA3_ok = dry_has("SHA3_POW_VECTOR_TEST")
if solo_out and "SHA3_POW_VECTOR" not in solo_out:
    SHA3_ok = False
if any(f.startswith("SHA3_POW_VECTOR_TEST:") for f in fails):
    SHA3_ok = False

PRODUCTION_MINING_PATH_AUDIT = "PASS" if not fails else "FAIL"
FINAL_VERDICT = (
    "PASS"
    if (
        PRODUCTION_MINING_PATH_AUDIT == "PASS"
        and POOL_CONFIGURATION_ALIGNMENT == "PASS"
        and GBT_ALIGNMENT == "PASS"
        and STRATUM_ALIGNMENT == "PASS"
        and SOLO_MINING_TEST == "PASS"
        and POOL_MINING_TEST == "PASS"
        and BLOCK_SUBMISSION_TEST == "PASS"
        and CONNECT_BLOCK_TEST == "PASS"
        and COINBASE_MATURITY_TEST == "PASS"
        and ORU_REWARD_TEST == "PASS"
        and RES_POW_CREATION_TEST == "PASS"
        and INVALID_POW_REJECTION_TEST == "PASS"
        and OPUHASH_REJECTION_TEST == "PASS"
        and CONSENSUS_CODE_CHANGED == "FALSE"
        and DATABASE_FORMAT_CHANGED == "FALSE"
        and WALLET_FORMAT_CHANGED == "FALSE"
    )
    else "FAIL"
)

describe, head = git_describe()
manifest = {
 "task": "",
    "date": str(date.today()),
    "nature": "Audit-first production mining path. No consensus redesign.",
    "command": "bash tools/run-production-mining-path-audit.sh",
    "git_describe": describe,
    "git_head": head,
    "pow": "SHA3-256(RGRPOW01 || header_80)",
    "PRODUCTION_MINING_PATH_AUDIT": PRODUCTION_MINING_PATH_AUDIT,
    "POOL_CONFIGURATION_ALIGNMENT": POOL_CONFIGURATION_ALIGNMENT,
    "GBT_ALIGNMENT": GBT_ALIGNMENT,
    "STRATUM_ALIGNMENT": STRATUM_ALIGNMENT,
    "SOLO_MINING_TEST": SOLO_MINING_TEST,
    "POOL_MINING_TEST": POOL_MINING_TEST,
    "BLOCK_SUBMISSION_TEST": BLOCK_SUBMISSION_TEST,
    "CONNECT_BLOCK_TEST": CONNECT_BLOCK_TEST,
    "COINBASE_MATURITY_TEST": COINBASE_MATURITY_TEST,
    "ORU_REWARD_TEST": ORU_REWARD_TEST,
    "RES_POW_CREATION_TEST": RES_POW_CREATION_TEST,
    "INVALID_POW_REJECTION_TEST": INVALID_POW_REJECTION_TEST,
    "OPUHASH_REJECTION_TEST": OPUHASH_REJECTION_TEST,
    "CONSENSUS_CODE_CHANGED": CONSENSUS_CODE_CHANGED,
    "DATABASE_FORMAT_CHANGED": DATABASE_FORMAT_CHANGED,
    "WALLET_FORMAT_CHANGED": WALLET_FORMAT_CHANGED,
    "FINAL_VERDICT": FINAL_VERDICT,
    "fails": fails,
    "findings": findings,
}
man_path.parent.mkdir(parents=True, exist_ok=True)
man_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

flags = [
    "PRODUCTION_MINING_PATH_AUDIT",
    "POOL_CONFIGURATION_ALIGNMENT",
    "GBT_ALIGNMENT",
    "STRATUM_ALIGNMENT",
    "SOLO_MINING_TEST",
    "POOL_MINING_TEST",
    "BLOCK_SUBMISSION_TEST",
    "CONNECT_BLOCK_TEST",
    "COINBASE_MATURITY_TEST",
    "ORU_REWARD_TEST",
    "RES_POW_CREATION_TEST",
    "INVALID_POW_REJECTION_TEST",
    "OPUHASH_REJECTION_TEST",
    "CONSENSUS_CODE_CHANGED",
    "DATABASE_FORMAT_CHANGED",
    "WALLET_FORMAT_CHANGED",
    "FINAL_VERDICT",
]
loc = {**manifest}
for k in flags:
    print(f"{k}={loc[k]}")
if fails:
    print("-- FAIL CLOSED --")
    for f in fails:
        print("FAIL", f)
    sys.exit(1)
print("OK")
sys.exit(0)
PY
