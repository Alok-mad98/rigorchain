#!/usr/bin/env bash
# P0-101: permanent GitHub production governance gate for RIGOR.
# Repository governance / CI security. Does NOT modify Consensus Kernel,
# Transaction Kernel, Ledger, ConnectBlock, CoinsDB, wallet schema, classifier,
# PoW, ASERT, genesis, or formal models.
#
# FAIL CLOSED. A document existing is not a PASS. An unauthorized protected
# change is FAIL. Bitcoin architecture regression is FAIL.
#
# Reproduce:
#   bash tools/run-production-governance-gate.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
export PYTHONUNBUFFERED=1

MANIFEST="$ROOT/formal_verification/github_governance_gate_manifest.json"
EXCEPTION="$ROOT/formal_verification/github_governance_exception.json"
ALIGN="$ROOT/tools/run-documentation-alignment-audit.sh"
ALIGN_LOG="$ROOT/formal_verification/github_governance_gate_alignment.log"

echo "== P101 RIGOR production governance gate =="
echo "command=bash tools/run-production-governance-gate.sh"
echo "nature=Repository governance. CI security gate. NOT a consensus modification task."
echo "principle=CODE > DOCUMENTATION > RELEASE"
echo "consensus_ssot=core/src/consensus/"

# --- 4. Documentation alignment (subprocess; fail-closed) -----------------
echo
echo "######## DOCUMENTATION ALIGNMENT ########"
echo "command=bash tools/run-documentation-alignment-audit.sh"
mkdir -p "$ROOT/formal_verification"
set +e
bash "$ALIGN" >"$ALIGN_LOG" 2>&1
ALIGN_RC=$?
set -e
tail -n 40 "$ALIGN_LOG" || true
echo "DOCUMENTATION_ALIGNMENT_EXIT=${ALIGN_RC}"

python3 - "$ROOT" "$MANIFEST" "$EXCEPTION" "$ALIGN_LOG" "$ALIGN_RC" <<'PY'
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
exception_path = Path(sys.argv[3])
align_log_path = Path(sys.argv[4])
align_rc = int(sys.argv[5])

fails: list[str] = []
findings: list[dict] = []


def fail(gate: str, msg: str) -> None:
    fails.append(f"{gate}: {msg}")
    findings.append({"severity": "FAIL", "gate": gate, "message": msg})


def info(gate: str, msg: str) -> None:
    findings.append({"severity": "INFO", "gate": gate, "message": msg})


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
    return (r.stdout or "") + (r.stderr or "")


def git_out(*args: str) -> str:
    r = subprocess.run(
        ["git", "-C", str(root), *args],
        capture_output=True,
        text=True,
        check=False,
    )
    return (r.stdout or "").strip()


def git_describe() -> tuple[str, str]:
    return git_out("describe", "--tags", "--always", "--dirty"), git_out("rev-parse", "HEAD")


def names_from(text: str) -> list[str]:
    return [ln.strip() for ln in text.splitlines() if ln.strip()]


# ---------------------------------------------------------------------------
# Protected path sets (absolute protection areas)
# ---------------------------------------------------------------------------
CONSENSUS_KERNEL_PATHS = [
    "core/src/consensus/",
    "core/src/pow.cpp",
    "core/src/pow.h",
    "core/src/kernel/chainparams.cpp",
    "core/src/kernel/chainparams.h",
    "core/src/chainparamsbase.cpp",
    "core/src/chainparamsbase.h",
    "core/src/validation.cpp",
    "core/src/validation.h",
    "core/src/primitives/",
    "core/src/rigor_kernel/pow_digest.cpp",
    "core/src/rigor_kernel/pow_digest.h",
    "core/src/connect_block_rigor_checks.cpp",
]

TX_KERNEL_PATHS = [
    "core/src/consensus/tx_verify.cpp",
    "core/src/consensus/tx_verify.h",
    "core/src/consensus/tx_check.cpp",
    "core/src/consensus/tx_check.h",
    "core/src/script/rigor_classify.cpp",
    "core/src/script/rigor_classify.h",
    "core/src/script/predicate_verify.cpp",
    "core/src/script/standard.cpp",
    "core/src/script/standard.h",
    "core/src/script/rigor_bytes.h",
]

ASSET_PATHS = [
    "core/src/consensus/rigor_res.cpp",
    "core/src/consensus/rigor_res.h",
    "core/src/consensus/rigor_issuance.cpp",
    "core/src/consensus/rigor_issuance.h",
    "core/src/consensus/rigor_consensus_lock.h",
    "core/src/consensus/convergenceres.cpp",
    "core/src/consensus/rigor_supply.cpp",
]

DATABASE_PATHS = [
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

WALLET_BOUNDARY_PATHS = [
    "core/src/wallet/",
]

MINING_PATHS = [
    "miner/",
    "pool/",
    "core/src/node/miner.cpp",
    "core/src/rpc/mining.cpp",
]

EXPLORER_PATHS = [
    "explorer/",
]

FORMAL_PATHS = [
    "formal_verification/coq/",
]

GENESIS_PATHS = [
    "core/src/kernel/chainparams.cpp",
    "core/src/consensus/kernel/genesis_authority.h",
    "core/src/consensus/kernel/genesis_authority.cpp",
    "core/src/consensus/rigor_consensus_lock.h",
]

POW_REVIEW_PATHS = [
    "core/src/pow.cpp",
    "core/src/pow.h",
    "core/src/rigor_kernel/pow_digest.cpp",
    "core/src/rigor_kernel/pow_digest.h",
]

SKIP_DIFF_PARTS = {
    ".deps",
    "__pycache__",
    "node_modules",
}


def is_skipped(rel: str) -> bool:
    parts = Path(rel).parts
    return any(p in SKIP_DIFF_PARTS for p in parts) or rel.endswith(".Po") or rel.endswith(".Plo")


def path_matches(rel: str, prefixes: list[str]) -> bool:
    if is_skipped(rel):
        return False
    for pref in prefixes:
        if pref.endswith("/"):
            if rel == pref[:-1] or rel.startswith(pref):
                return True
        elif rel == pref:
            return True
    return False


# ---------------------------------------------------------------------------
# Changed-file scan (PR base / push before / local HEAD)
# ---------------------------------------------------------------------------
ZERO = "0" * 40
base = (os.environ.get("RIGOR_GOVERNANCE_BASE") or "").strip()
if base.lower().startswith("0" * 8) or base == ZERO:
    base = ""
ref_name = (os.environ.get("RIGOR_GOVERNANCE_REF") or os.environ.get("GITHUB_REF") or "").strip()
event_name = (os.environ.get("RIGOR_GOVERNANCE_EVENT") or os.environ.get("GITHUB_EVENT_NAME") or "").strip()
is_tag = ref_name.startswith("refs/tags/v") or bool(re.match(r"^v\d", ref_name))

changed: set[str] = set()
if base:
    # Confirm the base object exists; otherwise fall back to working-tree scan.
    probe = subprocess.run(
        ["git", "-C", str(root), "cat-file", "-e", f"{base}^{{commit}}"],
        capture_output=True,
        text=True,
        check=False,
    )
    if probe.returncode != 0:
        info("DIFF", f"base {base} not in this clone; using working-tree scan")
        base = ""

if base:
    changed.update(names_from(git_out("diff", "--name-only", f"{base}...HEAD")))
    changed.update(names_from(git_out("diff", "--name-only", base, "HEAD")))

# Local / CI working tree (staged + unstaged vs HEAD) and untracked.
changed.update(names_from(git_out("diff", "--name-only", "HEAD")))
untracked = names_from(git_out("ls-files", "--others", "--exclude-standard"))
changed.update(untracked)
changed = {p for p in changed if p and not is_skipped(p)}


def changed_in(prefixes: list[str]) -> list[str]:
    return sorted(p for p in changed if path_matches(p, prefixes))


consensus_changed = changed_in(CONSENSUS_KERNEL_PATHS)
tx_changed = changed_in(TX_KERNEL_PATHS)
asset_changed = changed_in(ASSET_PATHS)
db_changed = changed_in(DATABASE_PATHS)
wallet_fmt_changed = changed_in(WALLET_FORMAT_PATHS)
wallet_changed = changed_in(WALLET_BOUNDARY_PATHS)
mining_changed = changed_in(MINING_PATHS)
explorer_changed = changed_in(EXPLORER_PATHS)
formal_changed = changed_in(FORMAL_PATHS)
genesis_changed = changed_in(GENESIS_PATHS)
pow_changed = changed_in(POW_REVIEW_PATHS)

CONSENSUS_CODE_CHANGED = "TRUE" if consensus_changed else "FALSE"

# ---------------------------------------------------------------------------
# Exception file (authorized protected-path change only)
# ---------------------------------------------------------------------------
exception: dict = {}
if exception_path.is_file():
    try:
        exception = json.loads(exception_path.read_text(encoding="utf-8"))
        if not isinstance(exception, dict):
            exception = {}
            fail("EXCEPTION", "github_governance_exception.json must be an object")
    except json.JSONDecodeError as e:
        fail("EXCEPTION", f"invalid JSON: {e}")

review_ok = bool(exception.get("security_review_approved") is True)
genesis_exc = bool(exception.get("genesis_release_exception") is True)
justification = str(exception.get("justification") or "").strip()
reviewed_paths = exception.get("reviewed_paths") or []
if not isinstance(reviewed_paths, list):
    reviewed_paths = []
    fail("EXCEPTION", "reviewed_paths must be a list")


def covered_by_review(rel: str) -> bool:
    for pref in reviewed_paths:
        if not isinstance(pref, str) or not pref:
            continue
        if pref.endswith("/"):
            if rel == pref[:-1] or rel.startswith(pref):
                return True
        elif rel == pref or rel.startswith(pref.rstrip("/") + "/"):
            return True
    return False


def require_review(paths: list[str], gate: str, extra_genesis: bool = False) -> None:
    if not paths:
        return
    if not review_ok:
        fail(
            gate,
            "unauthorized protected-path change without security_review_approved=true: "
            + ", ".join(paths),
        )
        return
    if not justification or len(justification) < 16:
        fail(gate, "security review exception requires a non-empty justification")
    missing = [p for p in paths if not covered_by_review(p)]
    if missing:
        fail(gate, "reviewed_paths does not cover: " + ", ".join(missing))
    if extra_genesis and not genesis_exc:
        fail(
            gate,
            "Genesis / network-identity change requires genesis_release_exception=true: "
            + ", ".join(paths),
        )


require_review(consensus_changed, "CONSENSUS_KERNEL", extra_genesis=bool(genesis_changed))
require_review(tx_changed, "TRANSACTION_KERNEL")
require_review(asset_changed, "ASSET_MODEL")
require_review(db_changed, "DATABASE")
require_review(wallet_fmt_changed, "WALLET_FORMAT")
require_review(formal_changed, "FORMAL_VERIFICATION")
if genesis_changed:
    require_review(genesis_changed, "GENESIS", extra_genesis=True)
if pow_changed:
    require_review(pow_changed, "POW_REVIEW")

# ---------------------------------------------------------------------------
# 1. Production governance scan — live code invariants
# ---------------------------------------------------------------------------
pow_cpp = read("core/src/pow.cpp")
pow_h = read("core/src/pow.h")
pow_digest = read("core/src/rigor_kernel/pow_digest.h")
chainparams = read("core/src/kernel/chainparams.cpp")
chainparamsbase = read("core/src/chainparamsbase.cpp")
lock = read("core/src/consensus/rigor_consensus_lock.h")
issuance = read("core/src/consensus/rigor_issuance.cpp")
classify = read("core/src/script/rigor_classify.cpp")
bytes_h = read("core/src/script/rigor_bytes.h")
validation = read("core/src/validation.cpp")
genesis_auth = read("core/src/consensus/kernel/genesis_authority.h")
eq_fw = read("formal_verification/coq/Phase5Equivalence/EquivalenceFramework.v")
readme = read("README.md")
sqlite = read("core/src/wallet/sqlite.cpp")

# CheckProofOfWork / ASERT / SHA3
cpow = re.search(
    r"bool CheckProofOfWork\s*\(.*?^\}\s*$",
    pow_cpp,
    re.MULTILINE | re.DOTALL,
)
cpow_body = cpow.group(0) if cpow else ""
if "GetRigorPowHash" not in cpow_body:
    fail("CONSENSUS_KERNEL", "CheckProofOfWork must call GetRigorPowHash")
if re.search(r"\bGetOpusHash\s*\(", cpow_body) or re.search(r"\bOpuHash\s*\(", cpow_body):
    fail("CONSENSUS_KERNEL", "CheckProofOfWork must not call OpuHash / GetOpusHash")
if "RGRPOW01" not in pow_digest or "GetRigorPowHash" not in pow_digest:
    fail("CONSENSUS_KERNEL", "pow_digest.h missing SHA3-256 RGRPOW01 / GetRigorPowHash")
if "GetNextWorkASERT" not in pow_cpp and "CalculateASERT" not in pow_cpp:
    fail("CONSENSUS_KERNEL", "ASERT difficulty path missing from pow.cpp")

# Genesis / magic / ports
if 'uint256S("0xf8aceb50d974f3157c7bc2dfffab28dee0c97d99e4e3ec931b84e06862fdd717")' not in chainparams:
    fail("CONSENSUS_KERNEL", "mainnet genesis pin missing from chainparams.cpp")
if not re.search(
    r"pchMessageStart\[0\]\s*=\s*0xce;\s*\n\s*pchMessageStart\[1\]\s*=\s*0xf1;\s*\n\s*pchMessageStart\[2\]\s*=\s*0xdb;\s*\n\s*pchMessageStart\[3\]\s*=\s*0xfa;",
    chainparams,
):
    fail("CONSENSUS_KERNEL", "mainnet magic must remain ce f1 db fa")
if not re.search(r"nDefaultPort\s*=\s*8760;", chainparams):
    fail("CONSENSUS_KERNEL", "mainnet P2P port must remain 8760")
if re.search(r"nDefaultPort\s*=\s*8332\s*;", chainparams) or re.search(
    r"nDefaultPort\s*=\s*8333\s*;", chainparams
):
    fail("BITCOIN_LEGACY", "Bitcoin P2P ports 8332/8333 reintroduced as nDefaultPort")
if 'CBaseChainParams>("", 8761,' not in chainparamsbase.replace(" ", ""):
    # tolerate whitespace variants
    if not re.search(r'CBaseChainParams>\("",\s*8761\s*,', chainparamsbase):
        fail("CONSENSUS_KERNEL", "mainnet RPC port must remain 8761")
if re.search(r'CBaseChainParams>\("",\s*8332\s*,', chainparamsbase):
    fail("BITCOIN_LEGACY", "Bitcoin RPC port 8332 reintroduced as mainnet default")
if re.search(r'CBaseChainParams>\("",\s*8333\s*,', chainparamsbase):
    fail("BITCOIN_LEGACY", "Bitcoin RPC port 8333 reintroduced as mainnet default")

# Production help / examples must advertise RIGOR ports, never Bitcoin 8332/8333.
rpc_util = read("core/src/rpc/util.cpp")
if "http://127.0.0.1:8761/" not in rpc_util:
    fail("BITCOIN_LEGACY", "RPC help examples must use RIGOR RPC 8761")
if "http://127.0.0.1:8332/" in rpc_util or "http://127.0.0.1:8333/" in rpc_util:
    fail("BITCOIN_LEGACY", "RPC help examples must not use Bitcoin ports 8332/8333")

BITCOIN_LIVE_PORT = re.compile(r"(?<![0-9])(8332|8333)(?![0-9])")
PORT_REJECT_LINE = re.compile(
    r"(?i)never bitcoin|not bitcoin|must not|do not use|forbidden|not used"
)
PORT_SCAN_SKIP = {
    "test",
    "fuzz",
    "bench",
    "minisketch",
    "leveldb",
    "secp256k1",
    "crc32c",
    "univalue",
    "qt",  # GUI has no port identity; skip vendor-adjacent qt tests via parts
}
src_root = root / "core/src"
if src_root.is_dir():
    for f in list(src_root.rglob("*.cpp")) + list(src_root.rglob("*.h")) + list(src_root.rglob("*.c")):
        rel = f.relative_to(root).as_posix()
        if any(p in PORT_SCAN_SKIP for p in f.parts):
            continue
        text = f.read_text(encoding="utf-8", errors="replace")
        for i, line in enumerate(text.splitlines(), 1):
            if not BITCOIN_LIVE_PORT.search(line):
                continue
            if PORT_REJECT_LINE.search(line):
                continue
            fail(
                "BITCOIN_LEGACY",
                f"{rel}:{i} advertises Bitcoin port 8332/8333; RIGOR live defaults are P2P 8760 / RPC 8761",
            )

if "ORU_GENESIS_BLOCK_SUBSIDY" not in lock:
    fail("CONSENSUS_KERNEL", "genesis subsidy lock missing")
if "nHeight == 0" not in issuance:
    fail("CONSENSUS_KERNEL", "issuance genesis height-0 lock missing")

if "PRODUCTION_CONSENSUS_AUTHORITY[] = \"RigorConsensusKernel(C++)\"" not in genesis_auth:
    fail("CONSENSUS_KERNEL", "PRODUCTION_CONSENSUS_AUTHORITY is not RigorConsensusKernel(C++)")

# Transaction kernel
if "AssetTypeFromScriptPubKey" not in classify:
    fail("TRANSACTION_KERNEL", "canonical AssetTypeFromScriptPubKey missing")
if "ClassifyRigorTxout" not in classify:
    fail("TRANSACTION_KERNEL", "canonical ClassifyRigorTxout missing")
if "RIGOR_MARKER_RES_MINT = 0xbb" not in bytes_h or "RIGOR_MARKER_CONVERGE = 0xbd" not in bytes_h:
    fail("TRANSACTION_KERNEL", "live script markers 0xbb/0xbd missing")
if "There is no Script VM" not in bytes_h and "not dispatched by a VM" not in bytes_h:
    fail("TRANSACTION_KERNEL", "script bytes header must state there is no Script VM")

# Dual-asset: RES is not a PoW reward
if re.search(r"GetResMintReward\s*\(", cpow_body):
    fail("ASSET_MODEL", "CheckProofOfWork must not mint or schedule RES")
if re.search(r"GetResMintReward\s*\(", pow_cpp) and "GetResMintReward" in cpow_body:
    fail("ASSET_MODEL", "pow.cpp CheckProofOfWork must not call GetResMintReward")
# GetBlockSubsidy is ORU-only
if "GetResMintReward" in issuance and "out.res_reward" in issuance:
    info("ASSET_MODEL", "issuance pack still separates ORU subsidy and RES schedule")
else:
    if "GetBlockSubsidy" not in issuance:
        fail("ASSET_MODEL", "GetBlockSubsidy missing from issuance")

# ---------------------------------------------------------------------------
# 2 + 3. Bitcoin legacy scan (live production, not historical comments)
# ---------------------------------------------------------------------------
if (root / "core/src/script/interpreter.cpp").is_file() or (
    root / "core/src/interpreter.cpp"
).is_file():
    fail("BITCOIN_LEGACY", "Script VM interpreter.cpp reintroduced")

eval_def = re.compile(r"^\s*bool\s+EvalScript\s*\(", re.MULTILINE)
verify_def = re.compile(r"^\s*bool\s+VerifyScript\s*\(", re.MULTILINE)
prod_script_roots = [
    root / "core/src/script",
    root / "core/src/consensus",
    root / "core/src/validation.cpp",
]
for p in prod_script_roots:
    files = [p] if p.is_file() else list(p.rglob("*.h")) + list(p.rglob("*.cpp"))
    for f in files:
        if not f.is_file():
            continue
        rel = f.relative_to(root).as_posix()
        if any(part in rel for part in ("/test/", "test/", "fuzz/", "bench/")):
            continue
        text = f.read_text(encoding="utf-8", errors="replace")
        if eval_def.search(text) or verify_def.search(text):
            fail("BITCOIN_LEGACY", f"EvalScript/VerifyScript definition in {rel}")

# Account-model / administrative consensus keys (production consensus only)
account_re = re.compile(
    r"\b(AccountLedger|account_balance_index|AdministrativeMintKey|admin_consensus_key)\b"
)
admin_re = re.compile(
    r"\b(privileged_mint_key|consensus_admin_key|master_mint_privkey)\b",
    re.I,
)
consensus_dir = root / "core/src/consensus"
if consensus_dir.is_dir():
    for f in list(consensus_dir.rglob("*.h")) + list(consensus_dir.rglob("*.cpp")):
        rel = f.relative_to(root).as_posix()
        if "/test/" in rel or ".deps" in rel:
            continue
        text = f.read_text(encoding="utf-8", errors="replace")
        if account_re.search(text):
            fail("BITCOIN_LEGACY", f"account-model identifier in {rel}")
        if admin_re.search(text):
            fail("BITCOIN_LEGACY", f"administrative consensus key in {rel}")

# OP_CHECK* live dispatch without specification: fail if a VM opcode table is added
opcode_spec = read("docs/specification/RIGOR_OPCODE_SPECIFICATION.md")
if "OP_CHECKSIG" in opcode_spec and "never executed" not in opcode_spec and "Reserved-inert" not in opcode_spec:
    fail("BITCOIN_LEGACY", "OP_CHECKSIG documented as live without reserved-inert specification")

# ---------------------------------------------------------------------------
# Wallet / CoinsDB boundary
# ---------------------------------------------------------------------------
wallet_root = root / "core/src/wallet"
forbidden_wallet_defs = re.compile(
    r"^\s*(bool|void)\s+(Chainstate::)?(ConnectBlock|DisconnectBlock)\s*\(",
    re.MULTILINE,
)
forbidden_wallet_calls = (
    "RigorConsensusKernel",
    "ConsensusKernelInput",
)
classifier_def = re.compile(
    r"^\s*(TxoutType|AssetType)\s+(ClassifyRigorTxout|ClassifyRigorAsset|AssetTypeFromScriptPubKey)\s*\(",
    re.MULTILINE,
)
if wallet_root.is_dir():
    for f in list(wallet_root.rglob("*.h")) + list(wallet_root.rglob("*.cpp")):
        rel = f.relative_to(root).as_posix()
        if "/test/" in rel or rel.endswith(".Po"):
            continue
        text = f.read_text(encoding="utf-8", errors="replace")
        if forbidden_wallet_defs.search(text):
            fail("WALLET_BOUNDARY", f"{rel} defines ConnectBlock/DisconnectBlock")
        if classifier_def.search(text):
            fail("WALLET_BOUNDARY", f"{rel} redefines the canonical classifier")
        stripped = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
        stripped = re.sub(r"//.*?$", "", stripped, flags=re.MULTILINE)
        for tok in ("ConnectBlock(", "DisconnectBlock("):
            # Comments already stripped; help text should not call these.
            if tok in stripped and "ConnectBlock" in tok:
                # wallet must not invoke ConnectBlock
                if re.search(r"\bConnectBlock\s*\(", stripped):
                    fail("WALLET_BOUNDARY", f"{rel} calls ConnectBlock")
                    break
        for tok in forbidden_wallet_calls:
            if re.search(r"\b" + re.escape(tok) + r"\b", stripped):
                fail("WALLET_BOUNDARY", f"{rel} references {tok} (wallet consensus authority)")

schema_m = re.search(r"WALLET_SCHEMA_VERSION\s*=\s*(\d+)", sqlite)
if schema_m is None:
    fail("DATABASE", "WALLET_SCHEMA_VERSION missing from sqlite.cpp")

coins_h = read("core/src/coins.h")
if re.search(r"\bAssetType\b", coins_h):
    fail("DATABASE", "Coin must not carry an AssetType field")
txdb = read("core/src/txdb.cpp")
if re.search(r"DB_ORU|DB_RES|DB_ASSET", txdb):
    fail("DATABASE", "CoinsDB must not grow an asset-specific type byte")

# ---------------------------------------------------------------------------
# Mining boundary — miner/pool cannot redefine consensus or mint RES
# ---------------------------------------------------------------------------
def scan_tree(prefix: str, suffixes: tuple[str, ...]) -> list[Path]:
    basep = root / prefix
    if not basep.exists():
        return []
    out: list[Path] = []
    if basep.is_file():
        return [basep]
    for f in basep.rglob("*"):
        if f.is_file() and f.suffix in suffixes:
            rel = f.relative_to(root).as_posix()
            if "node_modules" in rel or "/test/" in rel and prefix == "explorer":
                # still scan explorer tests for authority claims; skip node_modules only
                if "node_modules" in rel:
                    continue
            if "node_modules" in rel:
                continue
            out.append(f)
    return out


mining_forbidden = re.compile(
    r"\b(CheckProofOfWork|ConnectBlock|GetResMintReward|GetBlockSubsidy|RigorConsensusKernel)\s*\("
)
for f in scan_tree("miner", (".c", ".h", ".cpp", ".py", ".rs", ".go")) + scan_tree(
    "pool", (".c", ".h", ".cpp", ".py", ".rs", ".go", ".ts", ".js")
):
    rel = f.relative_to(root).as_posix()
    if rel.startswith("pool/rigor-miner-agent/target/") or "/.deps/" in rel:
        continue
    # Historical OpuHash notes and docs are not production consensus.
    if rel.endswith(".md"):
        continue
    text = f.read_text(encoding="utf-8", errors="replace")
    # Pool/miner may *call* RPC; they must not define consensus functions.
    if re.search(
        r"^\s*(bool|CAmount|uint256)\s+(CheckProofOfWork|GetResMintReward|GetBlockSubsidy|ConnectBlock)\s*\(",
        text,
        re.MULTILINE,
    ):
        fail("MINING_BOUNDARY", f"{rel} defines a consensus function")
    if re.search(r"nDefaultPort\s*=\s*8333", text) or re.search(r"rpc.?port\s*=\s*8332", text, re.I):
        fail("BITCOIN_LEGACY", f"{rel} sets Bitcoin ports 8332/8333 as live defaults")

# node miner.cpp may call CheckProofOfWork when assembling candidates; that is
# candidate creation, not replacing validation. Fail only if it *defines* them.
miner_cpp = read("core/src/node/miner.cpp")
if re.search(
    r"^\s*(bool|CAmount)\s+(CheckProofOfWork|GetResMintReward|ConnectBlock)\s*\(",
    miner_cpp,
    re.MULTILINE,
):
    fail("MINING_BOUNDARY", "node/miner.cpp must not define consensus functions")

# ---------------------------------------------------------------------------
# Explorer boundary — observation only
# ---------------------------------------------------------------------------
explorer_forbidden_auth = re.compile(
    r"\b(overrideGenesis|setGenesisHash|mintRES|mintRes|createAsset|assetLedgerAuthority|authorizeAssetCreation)\b"
)
for f in scan_tree("explorer/src", (".ts", ".js")):
    rel = f.relative_to(root).as_posix()
    text = f.read_text(encoding="utf-8", errors="replace")
    if explorer_forbidden_auth.search(text):
        fail("EXPLORER_BOUNDARY", f"{rel} claims asset/genesis authority")
    if re.search(r"hashGenesisBlock\s*=", text) and "expected" not in text.lower():
        fail("EXPLORER_BOUNDARY", f"{rel} assigns hashGenesisBlock")

expl_arch = read("docs/explorer/EXPLORER_ARCHITECTURE.md") + "\n" + read("docs/explorer/README.md")
if "observation" not in expl_arch.lower() and "observational" not in expl_arch.lower() and "does not" not in expl_arch.lower():
    # Architecture doc should state explorer is not consensus. Soft check via SSOT classifier mention.
    if "AssetTypeFromScriptPubKey" not in expl_arch:
        fail("EXPLORER_BOUNDARY", "explorer docs missing classifier SSOT")

# ---------------------------------------------------------------------------
# 10. Formal verification honesty
# ---------------------------------------------------------------------------
eq_defs = []
coq_root = root / "formal_verification/coq"
if coq_root.is_dir():
    for f in coq_root.rglob("*.v"):
        text = f.read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(
            r"Definition\s+equivalence_claimed\s*:\s*bool\s*:=\s*(true|false)\s*\.",
            text,
        ):
            eq_defs.append((f.relative_to(root).as_posix(), m.group(1)))
if not eq_defs:
    fail("FORMAL_VERIFICATION", "no Definition equivalence_claimed found")
for rel, val in eq_defs:
    if val == "true":
        fail("FORMAL_VERIFICATION", f"{rel} sets equivalence_claimed=true without proven C++ equivalence")

forbidden_claim = re.compile(
    r"fully\s+proven\s+(?:C\+\+\s+)?equivalence|complete\s+C\+\+\s+equivalence|Coq\s*=\s*C\+\+\s+equivalence",
    re.I,
)
claim_allow = re.compile(
    r"\b(not|never|false|forbidden|rejected|do not|must not|cannot|unproven|not claimed|no Coq)\b",
    re.I,
)
claim_surfaces = [
    "README.md",
    "docs/whitepaper/RIGOR_FORMAL_VERIFICATION_DESIGN.md",
    "docs/verification/FORMAL-VERIFICATION-STATUS.md",
    "formal_verification/README.md",
    "docs/security/FORMAL_VERIFICATION_ASSUMPTION_BOUNDARY.md",
]
for rel in claim_surfaces:
    text = read(rel)
    if not text:
        continue
    for i, line in enumerate(text.splitlines(), 1):
        if forbidden_claim.search(line) and not claim_allow.search(line):
            fail("FORMAL_VERIFICATION", f"{rel}:{i} claims proven C++ equivalence")

if "Definition equivalence_claimed : bool := false." not in eq_fw.replace("  ", " "):
    if "Definition equivalence_claimed : bool := false." not in eq_fw:
        fail("FORMAL_VERIFICATION", "EquivalenceFramework.v must keep equivalence_claimed = false")

if "equivalence_claimed = false" not in readme and "equivalence_claimed = false" not in readme.replace(" ", ""):
    if "equivalence_claimed = false" not in readme:
        fail("FORMAL_VERIFICATION", "README.md must keep equivalence_claimed = false")

# ---------------------------------------------------------------------------
# 5. Release artifact validation (wiring; full matrix stays on tag workflows)
# ---------------------------------------------------------------------------
rb = read(".github/workflows/release-build.yml")
rw = read(".github/workflows/release-wallet-macos.yml")
rg = read(".github/workflows/genesis-release-guardian.yml")
if not rb:
    fail("RELEASE_GUARDIAN", "missing .github/workflows/release-build.yml")
if not rw:
    fail("RELEASE_GUARDIAN", "missing .github/workflows/release-wallet-macos.yml")
if not rg:
    fail("RELEASE_GUARDIAN", "missing .github/workflows/genesis-release-guardian.yml")
if "tags:" not in rb or '"v*"' not in rb:
    fail("RELEASE_GUARDIAN", "release-build.yml must trigger on tags v*")
if "workflow_call:" not in rb:
    fail("RELEASE_GUARDIAN", "release-build.yml must expose workflow_call for Guardian")
if "release-wallet-macos" not in rw and "macos-wallet" not in rw:
    fail("RELEASE_GUARDIAN", "release-wallet-macos.yml missing macos wallet jobs")
if "tags:" not in rw or '"v*"' not in rw:
    fail("RELEASE_GUARDIAN", "release-wallet-macos.yml must trigger on tags v*")
if "workflow_call:" not in rw:
    fail("RELEASE_GUARDIAN", "release-wallet-macos.yml must expose workflow_call")
if '"v*"' not in rg and "- \"v*\"" not in rg:
    fail("RELEASE_GUARDIAN", "genesis-release-guardian.yml must trigger on tags v*")
if "uses: ./.github/workflows/release-build.yml" not in rg:
    fail("RELEASE_GUARDIAN", "Guardian must require release-build.yml")
if not exists("tools/run-genesis-release-guardian.sh"):
    fail("RELEASE_GUARDIAN", "missing tools/run-genesis-release-guardian.sh")
if not exists("tools/run-release-guardian-full-pipeline-audit.sh"):
    fail("RELEASE_GUARDIAN", "missing tools/run-release-guardian-full-pipeline-audit.sh")

# Fail closed: a new GitHub Release publisher that bypasses Guardian.
for wf in (root / ".github/workflows").glob("*.yml"):
    rel = wf.relative_to(root).as_posix()
    text = wf.read_text(encoding="utf-8", errors="replace")
    publishes = any(
        tok in text
        for tok in (
            "softprops/action-gh-release",
            "ncipollo/release-action",
            "actions/create-release",
            "gh release create",
        )
    )
    if publishes and "run-genesis-release-guardian.sh" not in text and "genesis-release-guardian" not in text:
        fail("RELEASE_GUARDIAN", f"{rel} publishes a GitHub Release without Guardian")

if is_tag:
    info("RELEASE_GUARDIAN", f"tag event {ref_name}: wiring required; Guardian workflow must PASS independently")
    # Tag events still fail closed if Guardian orchestrator is missing (already checked).

# ---------------------------------------------------------------------------
# 6. Security audit — existing fail-closed lints (no Coq rebuild)
# ---------------------------------------------------------------------------
LINTS = [
    ("tools/lint-consensus-authority.py", "AUTHORITY_LOCK"),
    ("tools/lint-wallet-security.py", "WALLET_SECURITY"),
    ("tools/lint-pow-boundary-authority.py", "POW_BOUNDARY"),
    ("tools/lint-dual-asset-database-boundary.py", "DUAL_ASSET"),
    ("tools/lint-coq-claim-strength.py", "COQ_CLAIM_STRENGTH"),
    ("tools/lint-connectblock-kernel-boundary.py", "CONNECTBLOCK_BOUNDARY"),
]
lint_results: dict[str, str] = {}
for rel, name in LINTS:
    p = root / rel
    if not p.is_file():
        fail("SECURITY_AUDIT", f"missing {rel}")
        lint_results[name] = "FAIL"
        continue
    r = subprocess.run(
        [sys.executable, str(p)],
        cwd=str(root),
        capture_output=True,
        text=True,
        check=False,
    )
    if r.returncode != 0:
        fail("SECURITY_AUDIT", f"{rel} exit {r.returncode}")
        tail = (r.stdout or "") + (r.stderr or "")
        for ln in tail.splitlines()[-8:]:
            info("SECURITY_AUDIT", ln)
        lint_results[name] = "FAIL"
    else:
        lint_results[name] = "PASS"
        info("SECURITY_AUDIT", f"{rel}=PASS")

# ---------------------------------------------------------------------------
# Documentation alignment result
# ---------------------------------------------------------------------------
align_log = align_log_path.read_text(encoding="utf-8", errors="replace") if align_log_path.is_file() else ""
align_status = "FAIL"
for ln in align_log.splitlines():
    if ln.startswith("DOCUMENT_ALIGNMENT_STATUS="):
        align_status = ln.split("=", 1)[1].strip()
if align_rc != 0:
    align_status = "FAIL"
    fail("DOCUMENTATION_ALIGNMENT", f"run-documentation-alignment-audit.sh exit {align_rc}")
elif align_status != "PASS":
    fail("DOCUMENTATION_ALIGNMENT", f"DOCUMENT_ALIGNMENT_STATUS={align_status}")

# Required deliverables of this gate.
# The GitHub workflow was dropped after RFC-019: merge CI is
# consensus-kernel-integrity only. This script stays for local runs.
for rel in (
    "tools/run-production-governance-gate.sh",
    "docs/security/RIGOR_GITHUB_PRODUCTION_GOVERNANCE_GATE_P0_101.md",
    "formal_verification/github_governance_gate_manifest.json",
):
    if rel == "formal_verification/github_governance_gate_manifest.json":
        continue  # written at end of this process
    if not exists(rel):
        fail("DELIVERABLE", f"missing {rel}")

# ---------------------------------------------------------------------------
# Gate aggregation
# ---------------------------------------------------------------------------
def gate_status(prefixes: tuple[str, ...]) -> str:
    return "FAIL" if any(f.startswith(prefixes) for f in fails) else "PASS"


CONSENSUS_PROTECTION = gate_status(("CONSENSUS_KERNEL:", "POW_REVIEW:", "GENESIS:", "TRANSACTION_KERNEL:"))
DATABASE_PROTECTION = gate_status(("DATABASE:",))
WALLET_BOUNDARY_PROTECTION = gate_status(("WALLET_BOUNDARY:", "WALLET_FORMAT:", "WALLET_SECURITY:"))
ASSET_MODEL_PROTECTION = gate_status(("ASSET_MODEL:", "DUAL_ASSET:"))
MINING_BOUNDARY_PROTECTION = gate_status(("MINING_BOUNDARY:", "EXPLORER_BOUNDARY:"))
# Mining and explorer are separate expected tokens; keep explorer in mining aggregate
# only for FINAL_VERDICT. Explicit explorer token:
EXPLORER_BOUNDARY_PROTECTION = gate_status(("EXPLORER_BOUNDARY:",))
BITCOIN_LEGACY_PROTECTION = gate_status(("BITCOIN_LEGACY:",))
DOCUMENTATION_ALIGNMENT_GATE = "PASS" if align_status == "PASS" and not any(
    f.startswith("DOCUMENTATION_ALIGNMENT:") for f in fails
) else "FAIL"
RELEASE_GUARDIAN_GATE = gate_status(("RELEASE_GUARDIAN:",))
FORMAL_VERIFICATION_PROTECTION = gate_status(("FORMAL_VERIFICATION:", "COQ_CLAIM_STRENGTH:"))
SECURITY_AUDIT_GATE = gate_status(("SECURITY_AUDIT:", "AUTHORITY_LOCK:", "CONNECTBLOCK_BOUNDARY:"))
if any(v != "PASS" for v in lint_results.values()):
    SECURITY_AUDIT_GATE = "FAIL"
    CONSENSUS_PROTECTION = "FAIL" if lint_results.get("AUTHORITY_LOCK") == "FAIL" else CONSENSUS_PROTECTION

# Detector self-check: the documented fail examples must remain detectable.
_self = []
if not re.search(r"nDefaultPort\s*=\s*8333\s*;", "        nDefaultPort = 8333;\n"):
    _self.append("Bitcoin P2P 8333")
if not re.search(r'CBaseChainParams>\("",\s*8332\s*,', 'CBaseChainParams>("", 8332, 8334);'):
    _self.append("Bitcoin RPC 8332")
if "http://127.0.0.1:8332/" not in "http://127.0.0.1:8332/\n":
    _self.append("RPC help 8332")
if not re.search(r"\bGetOpusHash\s*\(", "    uint256 h = GetOpusHash(block);\n"):
    _self.append("OpuHash in CheckProofOfWork")
if not re.search(r"^\s*bool\s+EvalScript\s*\(", "bool EvalScript(std::vector<val>&)\n", re.MULTILINE):
    _self.append("EvalScript reintroduction")
if not re.search(
    r"^\s*(bool|void)\s+(Chainstate::)?(ConnectBlock|DisconnectBlock)\s*\(",
    "bool ConnectBlock(const CBlock& block)\n",
    re.MULTILINE,
):
    _self.append("wallet ConnectBlock")
if not re.search(
    r"^\s*(TxoutType|AssetType)\s+(ClassifyRigorTxout|ClassifyRigorAsset|AssetTypeFromScriptPubKey)\s*\(",
    "AssetType AssetTypeFromScriptPubKey(const CScript& s)\n",
    re.MULTILINE,
):
    _self.append("wallet classifier redefinition")
if not re.search(r"\b(overrideGenesis|mintRES|createAsset)\b", "function overrideGenesis() {}"):
    _self.append("explorer genesis override")
if not re.search(
    r"Definition\s+equivalence_claimed\s*:\s*bool\s*:=\s*true\s*\.",
    "Definition equivalence_claimed : bool := true.",
):
    _self.append("equivalence_claimed=true")
if _self:
    fail("SELF_CHECK", "fail-example detector broken: " + ", ".join(_self))

# Mining token is miner/pool only; explorer has its own. Recompute mining without explorer.
MINING_BOUNDARY_PROTECTION = "FAIL" if any(f.startswith("MINING_BOUNDARY:") for f in fails) else "PASS"

GITHUB_PRODUCTION_GOVERNANCE_GATE = (
    "PASS"
    if not fails
    else "FAIL"
)
FINAL_VERDICT = GITHUB_PRODUCTION_GOVERNANCE_GATE

describe, head = git_describe()
manifest = {
 "task": "",
    "date": str(date.today()),
    "nature": "Repository governance. CI security gate. Production integrity protection. NOT a consensus modification task.",
    "principle": "CODE > DOCUMENTATION > RELEASE",
    "consensus_ssot": "core/src/consensus/",
    "command": "bash tools/run-production-governance-gate.sh",
    "git_describe": describe,
    "git_head": head,
    "event": event_name,
    "ref": ref_name,
    "base": base or None,
    "is_tag": is_tag,
    "equivalence_claimed": False,
    "GITHUB_PRODUCTION_GOVERNANCE_GATE": GITHUB_PRODUCTION_GOVERNANCE_GATE,
    "CONSENSUS_PROTECTION": CONSENSUS_PROTECTION,
    "DATABASE_PROTECTION": DATABASE_PROTECTION,
    "WALLET_BOUNDARY_PROTECTION": WALLET_BOUNDARY_PROTECTION,
    "ASSET_MODEL_PROTECTION": ASSET_MODEL_PROTECTION,
    "MINING_BOUNDARY_PROTECTION": MINING_BOUNDARY_PROTECTION,
    "EXPLORER_BOUNDARY_PROTECTION": EXPLORER_BOUNDARY_PROTECTION,
    "BITCOIN_LEGACY_PROTECTION": BITCOIN_LEGACY_PROTECTION,
    "DOCUMENTATION_ALIGNMENT_GATE": DOCUMENTATION_ALIGNMENT_GATE,
    "DOCUMENT_ALIGNMENT_STATUS": align_status,
    "RELEASE_GUARDIAN_GATE": RELEASE_GUARDIAN_GATE,
    "FORMAL_VERIFICATION_PROTECTION": FORMAL_VERIFICATION_PROTECTION,
    "SECURITY_AUDIT_GATE": SECURITY_AUDIT_GATE,
    "CONSENSUS_CODE_CHANGED": CONSENSUS_CODE_CHANGED,
    "FINAL_VERDICT": FINAL_VERDICT,
    "changed": {
        "consensus_kernel": consensus_changed,
        "transaction_kernel": tx_changed,
        "asset_model": asset_changed,
        "database": db_changed,
        "wallet_format": wallet_fmt_changed,
        "wallet": wallet_changed,
        "mining": mining_changed,
        "explorer": explorer_changed,
        "formal": formal_changed,
        "genesis": genesis_changed,
        "pow": pow_changed,
    },
    "exception_honored": bool(review_ok and justification),
    "lint_results": lint_results,
    "fails": fails,
    "findings": findings,
}

man_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

print()
print("GITHUB_PRODUCTION_GOVERNANCE_GATE=" + GITHUB_PRODUCTION_GOVERNANCE_GATE)
print("CONSENSUS_PROTECTION=" + CONSENSUS_PROTECTION)
print("DATABASE_PROTECTION=" + DATABASE_PROTECTION)
print("WALLET_BOUNDARY_PROTECTION=" + WALLET_BOUNDARY_PROTECTION)
print("ASSET_MODEL_PROTECTION=" + ASSET_MODEL_PROTECTION)
print("MINING_BOUNDARY_PROTECTION=" + MINING_BOUNDARY_PROTECTION)
print("EXPLORER_BOUNDARY_PROTECTION=" + EXPLORER_BOUNDARY_PROTECTION)
print("BITCOIN_LEGACY_PROTECTION=" + BITCOIN_LEGACY_PROTECTION)
print("DOCUMENTATION_ALIGNMENT_GATE=" + DOCUMENTATION_ALIGNMENT_GATE)
print("RELEASE_GUARDIAN_GATE=" + RELEASE_GUARDIAN_GATE)
print("FORMAL_VERIFICATION_PROTECTION=" + FORMAL_VERIFICATION_PROTECTION)
print("SECURITY_AUDIT_GATE=" + SECURITY_AUDIT_GATE)
print("CONSENSUS_CODE_CHANGED=" + CONSENSUS_CODE_CHANGED)
print("FINAL_VERDICT=" + FINAL_VERDICT)
if fails:
    print("-- FAIL CLOSED --")
    for f in fails:
        print("FAIL", f)
    sys.exit(1)
print("OK")
sys.exit(0)
PY
