#!/usr/bin/env bash
# P0-097: documentation alignment audit against production C++ facts.
# AUDIT-FIRST. Does not modify Consensus Kernel, Transaction Kernel, Ledger,
# ConnectBlock / DisconnectBlock, CoinsDB, wallet schema, classifier, PoW,
# ASERT, genesis, or Release Guardian logic.
#
# FAIL CLOSED. A document existing is not a PASS. A mismatch is FAIL.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
export PYTHONUNBUFFERED=1

MANIFEST="$ROOT/formal_verification/documentation_alignment_manifest.json"

echo "== P097 Documentation alignment audit (protocol SSOT, C++ realization) =="
echo "command=bash tools/run-documentation-alignment-audit.sh"
echo "nature=AUDIT-FIRST documentation only; no consensus/protocol/database patch"

python3 - "$ROOT" "$MANIFEST" <<'PY'
from __future__ import annotations

import json
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


findings: list[dict] = []
fails: list[str] = []


def fail(gate: str, msg: str) -> None:
    fails.append(f"{gate}: {msg}")
    findings.append({"severity": "FAIL", "gate": gate, "message": msg})


def info(gate: str, msg: str) -> None:
    findings.append({"severity": "INFO", "gate": gate, "message": msg})


# --- Extract code facts ---
lock = read("core/src/consensus/rigor_consensus_lock.h")
params_h = read("core/src/consensus/params.h")
chainparams = read("core/src/kernel/chainparams.cpp")
chainparamsbase = read("core/src/chainparamsbase.cpp")
clientversion = read("core/src/clientversion.h")
pow_digest = read("core/src/rigor_kernel/pow_digest.h")
pow_cpp = read("core/src/pow.cpp")
bytes_h = read("core/src/script/rigor_bytes.h")
issuance = read("core/src/consensus/rigor_issuance.cpp")
sqlite = read("core/src/wallet/sqlite.cpp")

m = re.search(r'RIGOR_RELEASE_VERSION\s*=\s*"([^"]+)"', clientversion)
release_version = m.group(1) if m else ""
m = re.search(r'BECH32_HRP\s*=\s*"([^"]+)"', lock)
hrp = m.group(1) if m else ""
m = re.search(r"CONVERGENCE_DRAW_WINDOW\s*=\s*(\d+)", lock)
draw_w = int(m.group(1)) if m else -1
m = re.search(r"MAINNET_POW_ASERT_HALF_LIFE\s*=\s*(\d+)", lock)
asert_tau = int(m.group(1)) if m else -1
m = re.search(r"ORU_GENESIS_BLOCK_SUBSIDY\s*=\s*(\d+)", lock)
genesis_subsidy = int(m.group(1)) if m else -1

gm = re.search(
    r'uint256S\("0x(f8aceb50[0-9a-fA-F]+)"\)',
    chainparams,
)
genesis_main = gm.group(1).lower() if gm else ""

magic = re.search(
    r"pchMessageStart\[0\]\s*=\s*(0x[0-9a-fA-F]+);\s*\n\s*pchMessageStart\[1\]\s*=\s*(0x[0-9a-fA-F]+);\s*\n\s*pchMessageStart\[2\]\s*=\s*(0x[0-9a-fA-F]+);\s*\n\s*pchMessageStart\[3\]\s*=\s*(0x[0-9a-fA-F]+);",
    chainparams,
)
magic_bytes = [magic.group(i).lower() for i in range(1, 5)] if magic else []

rpc_main = re.search(r'CBaseChainParams::MAIN.*?\n.*?<CBaseChainParams>\("",\s*(\d+),', chainparamsbase)
if rpc_main is None:
    rpc_main = re.search(r'<CBaseChainParams>\("",\s*(\d+),', chainparamsbase)
p2p_main = re.search(r"nDefaultPort\s*=\s*(\d+);", chainparams)
rpc_port = int(rpc_main.group(1)) if rpc_main else -1
p2p_port = int(p2p_main.group(1)) if p2p_main else -1

sha3_pow = "RGRPOW01" in pow_digest and "GetRigorPowHash" in pow_digest and "GetRigorPowHash" in pow_cpp
live_markers = all(x in bytes_h for x in ["RIGOR_MARKER_RETURN = 0x6a", "RIGOR_MARKER_RES_MINT = 0xbb", "RIGOR_MARKER_CONVERGE = 0xbd"])
no_vm = "There is no Script VM" in bytes_h or "not dispatched by a VM" in bytes_h
genesis_zero = "nHeight == 0" in issuance and genesis_subsidy == 0

if release_version != "v0.1.3":
    fail("CORE", f"RIGOR_RELEASE_VERSION={release_version!r}, expected v0.1.3")
if hrp != "rg":
    fail("CORE", f"BECH32_HRP={hrp!r}, expected rg")
if draw_w != 32:
    fail("CORE", f"CONVERGENCE_DRAW_WINDOW={draw_w}, expected 32")
if asert_tau != 43200:
    fail("CORE", f"ASERT tau={asert_tau}, expected 43200")
if genesis_main != "f8aceb50d974f3157c7bc2dfffab28dee0c97d99e4e3ec931b84e06862fdd717":
    fail("CORE", f"mainnet genesis={genesis_main}")
if magic_bytes != ["0xce", "0xf1", "0xdb", "0xfa"]:
    fail("CORE", f"magic={magic_bytes}")
if rpc_port != 8761 or p2p_port != 8760:
    fail("CORE", f"ports P2P={p2p_port} RPC={rpc_port}")
if not sha3_pow:
    fail("CORE", "CheckProofOfWork / GetRigorPowHash / RGRPOW01 missing")
if not live_markers:
    fail("CORE", "live script markers 0x6a/0xbb/0xbd missing")
if not genesis_zero:
    fail("CORE", "genesis subsidy is not locked to 0")

CONSENSUS_PATHS = [
    "core/src/consensus",
    "core/src/script/rigor_classify.cpp",
    "core/src/script/rigor_classify.h",
    "core/src/validation.cpp",
    "core/src/pow.cpp",
    "core/src/primitives",
]
DB_PATHS = [
    "core/src/coins.cpp",
    "core/src/coins.h",
    "core/src/txdb.cpp",
    "core/src/txdb.h",
    "core/src/undo.h",
    "core/src/compressor.h",
]
WALLET_FORMAT_PATHS = [
    "core/src/wallet/sqlite.cpp",
    "core/src/wallet/walletdb.cpp",
    "core/src/wallet/walletdb.h",
]

consensus_changed = sorted(git_names(CONSENSUS_PATHS))
db_changed = sorted(git_names(DB_PATHS))
wallet_changed = sorted(git_names(WALLET_FORMAT_PATHS))
if consensus_changed:
    fail("CONSENSUS_CODE_CHANGED", "consensus paths dirty: " + ", ".join(consensus_changed))
if db_changed:
    fail("DATABASE_FORMAT_CHANGED", "db paths dirty: " + ", ".join(db_changed))
if wallet_changed:
    fail("WALLET_FORMAT_CHANGED", "wallet format paths dirty: " + ", ".join(wallet_changed))

schema_m = re.search(r"WALLET_SCHEMA_VERSION\s*=\s*(\d+)", sqlite)
wallet_schema = int(schema_m.group(1)) if schema_m else None

# --- Deliverables ---
required = [
    "docs/audit/CODE_SSOT_PROTOCOL_FACTS_P0_097.md",
    "docs/specification/RIGOR_OPCODE_SPECIFICATION.md",
    "docs/release/RIGOR_GITHUB_PRESENTATION_FINAL_P0_097.md",
    "tools/run-documentation-alignment-audit.sh",
    "README.md",
    "docs/user/README.md",
    "docs/developer/README.md",
]
for rel in required:
    if not exists(rel):
        fail("DELIVERABLE", f"missing {rel}")

facts = read("docs/audit/CODE_SSOT_PROTOCOL_FACTS_P0_097.md")
readme = read("README.md")
identity = read("docs/whitepaper/RIGOR_PROTOCOL_IDENTITY.md")
math = read("docs/whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md")
wp = read("docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md")
fv = read("docs/whitepaper/RIGOR_FORMAL_VERIFICATION_DESIGN.md")
opcode = read("docs/specification/RIGOR_OPCODE_SPECIFICATION.md")
expl = read("docs/explorer/README.md") + "\n" + read("docs/explorer/EXPLORER_ARCHITECTURE.md")
miner = read("miner/README_MINER.md")
pool = read("pool/README.md")
gh = read("docs/release/RIGOR_GITHUB_PRESENTATION_FINAL_P0_097.md")

# Facts file must pin code values
for needle, gate in [
    (genesis_main, "FACTS_GENESIS"),
    ("ce f1 db fa", "FACTS_MAGIC"),
    ("8761", "FACTS_RPC"),
    ("8760", "FACTS_P2P"),
    ('"rg"', "FACTS_HRP_LOCK") if False else ("`rg`", "FACTS_HRP"),
    ("v0.1.3", "FACTS_VERSION"),
    ("RGRPOW01", "FACTS_POW"),
    ("CONVERGENCE_DRAW_WINDOW", "FACTS_W"),
    ("PoW does NOT create RES", "FACTS_RES"),
]:
    if needle and needle not in facts:
        fail("FACTS", f"{gate}: {needle!r} not in CODE_SSOT facts")

# README alignment
readme_needles = [
    "v0.1.3",
    genesis_main,
    "8761",
    "8760",
    "`rg`",
    "ORU",
    "RES",
    "RGRPOW01",
    "getblocktemplate",
    "equivalence_claimed = false",
]
for n in readme_needles:
    if n not in readme:
        fail("README", f"missing {n!r}")

if re.search(r"Upcoming product", readme, re.I):
    fail("README", "contains Upcoming product wording")
if re.search(r"Bitcoin[- ]compatible protocol", readme, re.I):
    fail("README", "Bitcoin compatibility wording")
if "ALPHAPLUS_RELEASE=true" in readme:
    fail("README", "ALPHAPLUS_RELEASE=true")
# Current product must not be named AlphaPlus
if re.search(r"product identity.*AlphaPlus", readme, re.I):
    fail("README", "AlphaPlus presented as current product identity")
if "CheckProofOfWork" in readme and "OpuHash" in readme and "RGRPOW01" not in readme:
    fail("README", "OpuHash presented as node PoW without RGRPOW01")

# Whitepapers
for rel, text in [
    ("docs/whitepaper/RIGOR_PROTOCOL_IDENTITY.md", identity),
    ("docs/whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md", math),
    ("docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md", wp),
    ("docs/whitepaper/RIGOR_FORMAL_VERIFICATION_DESIGN.md", fv),
]:
    if not text:
        fail("WHITEPAPER", f"missing {rel}")
    if "v0.1.3" not in text and rel.endswith("RIGOR_FORMAL_VERIFICATION_DESIGN.md"):
        fail("WHITEPAPER", f"{rel} missing v0.1.3")
    if rel.endswith("RIGOR_PROTOCOL_IDENTITY.md") and "v0.1.3" not in text:
        fail("WHITEPAPER", "identity paper missing v0.1.3")
    if rel.endswith("RIGOR_PROTOCOL_WHITEPAPER.md") and "AlphaPlus" in text and "Historical" not in text:
        fail("WHITEPAPER", "whitepaper names AlphaPlus without Historical label")

if "| Proof-of-Work | OpuHash with ASERT difficulty |" in identity:
    fail("WHITEPAPER", "identity still lists OpuHash as current PoW")
if "Half-life | 172800" in math or "172800 seconds" in math:
    fail("WHITEPAPER", "math paper still lists ASERT 172800")
if "R(0) = 0" not in math:
    fail("WHITEPAPER", "math paper missing R(0) = 0")
if "interpreter.cpp" in math and "There is no `interpreter.cpp`" not in math:
    fail("WHITEPAPER", "math paper still cites interpreter.cpp as live")
if "RGRPOW01" not in math or "RGRPOW01" not in identity:
    fail("WHITEPAPER", "SHA3 RGRPOW01 missing from identity/math papers")
if "Product | RIGOR / AlphaPlus" in wp:
    fail("WHITEPAPER", "narrative whitepaper product still AlphaPlus")

# Opcode spec
if "0xbd" not in opcode or "0xbb" not in opcode or "0x6a" not in opcode:
    fail("OPCODE", "live markers missing from opcode spec")
if "no Script VM" not in opcode and "no Bitcoin Script VM" not in opcode:
    fail("OPCODE", "opcode spec does not state there is no Script VM")
if "OP_CHECKSIG" in opcode and "never executed" not in opcode and "Reserved-inert" not in opcode:
    fail("OPCODE", "Bitcoin OP_CHECKSIG documented as live")
if "EvalScript" in opcode and "Removed" not in opcode:
    fail("OPCODE", "EvalScript not marked removed")

# Explorer
if genesis_main not in expl:
    fail("EXPLORER", "explorer docs missing live genesis")
if "v0.1.3" not in expl:
    fail("EXPLORER", "explorer docs missing v0.1.3")
if "`rg`" not in expl and "HRP:** `rg`" not in expl and "HRP: `rg`" not in expl:
    if "rg" not in expl:
        fail("EXPLORER", "explorer docs missing HRP rg")
if "AssetTypeFromScriptPubKey" not in expl:
    fail("EXPLORER", "explorer docs missing classifier SSOT")

# Miner
if "getblocktemplate" not in miner:
    fail("MINER", "miner README missing GBT")
if "8761" not in miner:
    fail("MINER", "miner README missing RPC 8761")
if "RGRPOW01" not in miner and "SHA3-256" not in miner:
    fail("MINER", "miner README does not state node SHA3 PoW")
if "default_coinbase_tx" not in miner:
    fail("MINER", "miner README missing canonical GBT copy-bytes")

# Pool
if "8761" not in pool:
    fail("POOL", "pool README missing RPC 8761")
if "getblocktemplate" not in pool and "GBT" not in pool:
    fail("POOL", "pool README missing GBT")
if "does **not** mint RES" not in pool and "Does **not** create" not in pool:
    fail("POOL", "pool README missing RES boundary")
if "v0.1.3" not in pool:
    fail("POOL", "pool README missing v0.1.3")

# GitHub presentation
if "No repository split" not in gh and "no repository split" not in gh.lower():
    fail("GITHUB", "presentation doc missing no-split rule")
if "README" not in gh or "Whitepapers" not in gh:
    fail("GITHUB", "presentation tree incomplete")

# Classification dirs
for rel in ["docs/user/README.md", "docs/developer/README.md", "docs/security/README.md", "docs/formal/README.md", "docs/specification/SPEC-INDEX.md", "archive/README.md"]:
    if not exists(rel):
        fail("CLASSIFICATION", f"missing {rel}")

# Archived AlphaPlus pack should not remain as live release identity
if exists("docs/release/ALPHAPLUS_RC0_GITHUB_PUSH_READINESS_REPORT.md"):
    fail("ARCHIVE", "AlphaPlus RC0 report still under docs/release/")
if not exists("archive/docs/alphaplus/release/RIGOR_ALPHAPLUS_RELEASE_NOTES.md"):
    fail("ARCHIVE", "AlphaPlus release notes were not moved to archive/")

README_ALIGNMENT = "PASS" if not any(f.startswith("README:") for f in fails) else "FAIL"
WHITEPAPER_ALIGNMENT = "PASS" if not any(f.startswith("WHITEPAPER:") for f in fails) else "FAIL"
OPCODE_DOCUMENTATION_ALIGNMENT = "PASS" if not any(f.startswith("OPCODE:") for f in fails) else "FAIL"
EXPLORER_DOCUMENTATION_ALIGNMENT = "PASS" if not any(f.startswith("EXPLORER:") for f in fails) else "FAIL"
MINER_DOCUMENTATION_ALIGNMENT = "PASS" if not any(f.startswith("MINER:") for f in fails) else "FAIL"
POOL_DOCUMENTATION_ALIGNMENT = "PASS" if not any(f.startswith("POOL:") for f in fails) else "FAIL"
DOCUMENT_ALIGNMENT_STATUS = "PASS" if not fails else "FAIL"
CONSENSUS_CODE_CHANGED = "TRUE" if consensus_changed else "FALSE"
DATABASE_FORMAT_CHANGED = "TRUE" if db_changed else "FALSE"
WALLET_FORMAT_CHANGED = "TRUE" if wallet_changed else "FALSE"
FINAL_VERDICT = "PASS" if DOCUMENT_ALIGNMENT_STATUS == "PASS" and CONSENSUS_CODE_CHANGED == "FALSE" and DATABASE_FORMAT_CHANGED == "FALSE" and WALLET_FORMAT_CHANGED == "FALSE" else "FAIL"

describe, head = git_describe()
manifest = {
    "task": "",
    "date": str(date.today()),
    "nature": "Documentation alignment only. Protocol is SSOT. No consensus modification.",
    "command": "bash tools/run-documentation-alignment-audit.sh",
    "git_describe": describe,
    "git_head": head,
    "code_facts": {
        "release_version": release_version,
        "mainnet_genesis": genesis_main,
        "mainnet_magic": " ".join(x.replace("0x", "") for x in magic_bytes),
        "p2p_port": p2p_port,
        "rpc_port": rpc_port,
        "hrp": hrp,
        "draw_window": draw_w,
        "asert_half_life": asert_tau,
        "pow": "SHA3-256 RGRPOW01",
        "genesis_oru_subsidy": genesis_subsidy,
        "wallet_schema_version": wallet_schema,
    },
    "DOCUMENT_ALIGNMENT_STATUS": DOCUMENT_ALIGNMENT_STATUS,
    "README_ALIGNMENT": README_ALIGNMENT,
    "WHITEPAPER_ALIGNMENT": WHITEPAPER_ALIGNMENT,
    "OPCODE_DOCUMENTATION_ALIGNMENT": OPCODE_DOCUMENTATION_ALIGNMENT,
    "EXPLORER_DOCUMENTATION_ALIGNMENT": EXPLORER_DOCUMENTATION_ALIGNMENT,
    "MINER_DOCUMENTATION_ALIGNMENT": MINER_DOCUMENTATION_ALIGNMENT,
    "POOL_DOCUMENTATION_ALIGNMENT": POOL_DOCUMENTATION_ALIGNMENT,
    "CONSENSUS_CODE_CHANGED": CONSENSUS_CODE_CHANGED,
    "DATABASE_FORMAT_CHANGED": DATABASE_FORMAT_CHANGED,
    "WALLET_FORMAT_CHANGED": WALLET_FORMAT_CHANGED,
    "FINAL_VERDICT": FINAL_VERDICT,
    "fails": fails,
    "findings": findings,
}

man_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

print(f"DOCUMENT_ALIGNMENT_STATUS={DOCUMENT_ALIGNMENT_STATUS}")
print(f"README_ALIGNMENT={README_ALIGNMENT}")
print(f"WHITEPAPER_ALIGNMENT={WHITEPAPER_ALIGNMENT}")
print(f"OPCODE_DOCUMENTATION_ALIGNMENT={OPCODE_DOCUMENTATION_ALIGNMENT}")
print(f"EXPLORER_DOCUMENTATION_ALIGNMENT={EXPLORER_DOCUMENTATION_ALIGNMENT}")
print(f"MINER_DOCUMENTATION_ALIGNMENT={MINER_DOCUMENTATION_ALIGNMENT}")
print(f"POOL_DOCUMENTATION_ALIGNMENT={POOL_DOCUMENTATION_ALIGNMENT}")
print(f"CONSENSUS_CODE_CHANGED={CONSENSUS_CODE_CHANGED}")
print(f"DATABASE_FORMAT_CHANGED={DATABASE_FORMAT_CHANGED}")
print(f"WALLET_FORMAT_CHANGED={WALLET_FORMAT_CHANGED}")
print(f"FINAL_VERDICT={FINAL_VERDICT}")
if fails:
    print("-- FAIL CLOSED --")
    for f in fails:
        print("FAIL", f)
    sys.exit(1)
print("OK")
sys.exit(0)
PY
