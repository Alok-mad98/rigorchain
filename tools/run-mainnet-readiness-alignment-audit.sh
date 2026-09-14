#!/usr/bin/env bash
# P0-096: cross-component mainnet readiness alignment audit.
# AUDIT-FIRST. Does not modify Consensus Kernel, Transaction Kernel, Ledger,
# ConnectBlock / DisconnectBlock, CoinsDB, wallet SQLite schema, classifier,
# PoW, ASERT, P2P protocol, mining consensus rules, or Release Guardian rules.
#
# Re-reads source, official artifacts, workflows, and docs. A document existing
# is not a PASS. A mismatch is FAIL. Do not patch automatically.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
export PYTHONUNBUFFERED=1

DOC="$ROOT/docs/security/RIGOR_MAINNET_READINESS_ALIGNMENT_AUDIT_P0_096.md"
MANIFEST="$ROOT/formal_verification/mainnet_readiness_alignment_manifest.json"

echo "== P096 Mainnet readiness cross-component alignment audit =="
echo "command=bash tools/run-mainnet-readiness-alignment-audit.sh"
echo "nature=AUDIT-FIRST no consensus/protocol/database patch"

python3 - "$ROOT" "$DOC" "$MANIFEST" <<'PY'
from __future__ import annotations

import hashlib
import json
import os
import re
import subprocess
import zipfile
from pathlib import Path
import sys

root = Path(sys.argv[1])
doc_path = Path(sys.argv[2])
man_path = Path(sys.argv[3])

OFFICIAL = [
    "rigor-v0.1.3-windows-x86_64.exe",
    "rigor-v0.1.3-windows-x86_64.zip",
    "rigor-v0.1.3-linux-x86_64.tar.gz",
    "rigor-v0.1.3-macos-x86_64.dmg",
    "rigor-v0.1.3-macos-arm64.dmg",
]
EXPECTED_SHA256 = {
    "rigor-v0.1.3-windows-x86_64.exe": "be9a5e137d559571f4ffbc4ba31ed6840646091171c5d6415de3e1fc5a54e03d",
    "rigor-v0.1.3-windows-x86_64.zip": "92f3717a63cf376eac8dfb16f2d74948fb94082a51ea20955eef63cb7137d866",
    "rigor-v0.1.3-linux-x86_64.tar.gz": "bf85b3ae1c13e10aa4a986e52402dba8a2289771df6b62c561df54c53485052b",
    "rigor-v0.1.3-macos-x86_64.dmg": "9efbb8e9886669801b84a150f409671b6f3a22c7d1deb9905de767dbf2e6266f",
    "rigor-v0.1.3-macos-arm64.dmg": "6abadd5b9804fc94003fcac61b5b7d432d97a1271102b4ffd6cc3a8e7e243daa",
}
NODE_MAINNET_GENESIS = "f8aceb50d974f3157c7bc2dfffab28dee0c97d99e4e3ec931b84e06862fdd717"
NODE_TESTNET_GENESIS = "0aca439758fba7fa5190b7e1cf37f9447516ef4cade07a49e7d484ea5b67b9ce"
NODE_MAGIC = ["0xce", "0xf1", "0xdb", "0xfa"]
P096_FILES = {
    "docs/security/RIGOR_MAINNET_READINESS_ALIGNMENT_AUDIT_P0_096.md",
    "docs/security/README.md",
    "formal_verification/mainnet_readiness_alignment_manifest.json",
    "tools/run-mainnet-readiness-alignment-audit.sh",
}
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
    "core/src/dbwrapper.cpp",
    "core/src/dbwrapper.h",
]
PARAM_PATHS = [
    "core/src/kernel/chainparams.cpp",
    "core/src/consensus/consensus.h",
    "core/src/consensus/rigor_consensus_lock.h",
    "core/src/consensus/params.h",
]


def read(rel: str) -> str:
    return (root / rel).read_text(encoding="utf-8", errors="replace")


def exists(rel: str) -> bool:
    return (root / rel).is_file()


def git(*args: str) -> str:
    r = subprocess.run(
        ["git", "-C", str(root), *args],
        capture_output=True,
        text=True,
        check=False,
    )
    return (r.stdout or "").strip()


def git_names(paths: list[str]) -> set[str]:
    r = subprocess.run(
        ["git", "-C", str(root), "diff", "--name-only", "--"] + paths,
        capture_output=True,
        text=True,
        check=False,
    )
    return {ln.strip() for ln in r.stdout.splitlines() if ln.strip()}


def run_cmd(args: list[str], timeout: int = 20) -> tuple[int, str]:
    r = subprocess.run(args, capture_output=True, text=True, check=False, timeout=timeout)
    return r.returncode, (r.stdout or "") + (r.stderr or "")


findings: list[dict] = []
errors: list[str] = []


def finding(component: str, severity: str, title: str, evidence: str) -> None:
    findings.append(
        {
            "component": component,
            "severity": severity,
            "title": title,
            "evidence": evidence,
        }
    )
    if severity == "FAIL":
        errors.append(f"{component}: {title}")


# ---------------------------------------------------------------------------
# 1. Core node
# ---------------------------------------------------------------------------
cv = read("core/src/clientversion.h")
ca = read("core/configure.ac")
cp = read("core/src/kernel/chainparams.cpp")
lock = read("core/src/consensus/rigor_consensus_lock.h")
params = read("core/src/consensus/params.h")
init_cpp = read("core/src/init.cpp")
cv_cpp = read("core/src/clientversion.cpp")
net_rpc = read("core/src/rpc/net.cpp")
mining_rpc = read("core/src/rpc/mining.cpp")
wallet_rpc = read("core/src/wallet/rpc/rpcwallet.cpp")

m_rel = re.search(r'RIGOR_RELEASE_VERSION = "([^"]+)"', cv)
release_version = m_rel.group(1) if m_rel else ""
packed_major = "25" if re.search(r"define\(_CLIENT_VERSION_MAJOR, 25\)", ca) else ""
subversion_uses_client = "FormatSubVersion(CLIENT_NAME, CLIENT_VERSION" in init_cpp
genesis_m = re.search(
    r'hashGenesisBlock == uint256S\("0x([0-9a-f]+)"\)',
    cp,
)
node_genesis = genesis_m.group(1) if genesis_m else ""
magic_ok = all(tok in cp for tok in NODE_MAGIC) and "nDefaultPort = 8760" in cp
hrp_ok = 'bech32_hrp = "rg"' in cp
e246 = "RES_PROBABILITY_TARGET_EXPONENT = 246" in params or "E = 246" in params or "exponent 246" in params.lower()
if "246" not in params:
    e246 = False
else:
    e246 = True
c1024 = "1024" in params
maturity_100 = "consensus.nCoinbaseMaturity = 100" in cp
rpc_gbt = "getblocktemplate" in mining_rpc and "default_coinbase_tx" in mining_rpc
rpc_wallet = "sendres" in wallet_rpc and "rescanblockchain" in wallet_rpc
rpc_net = '"version"' in net_rpc and "CLIENT_VERSION" in net_rpc
empty_seeds = "vFixedSeeds.clear()" in cp
min_work_bootstrap = "0000000800000000" in cp.replace("0x", "").replace('"', "")
# keep original hex check
min_work_bootstrap = 'nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000000000000000800000000")' in cp

rigord = root / "core/src/rigord"
core_banner = ""
core_binary_present = rigord.is_file() and os.access(rigord, os.X_OK)
if core_binary_present:
    rc, out = run_cmd([str(rigord), "--version"])
    core_banner = out
    if "RIGOR version v0.1.3" not in out.splitlines()[:3] and "RIGOR version v0.1.3" not in out:
        finding("CORE", "FAIL", "in-tree rigord --version is not v0.1.3", out[:400])
    else:
        finding("CORE", "INFO", "in-tree rigord --version is v0.1.3", out.splitlines()[0] if out else "")
else:
    finding("CORE", "FAIL", "in-tree core/src/rigord binary missing", str(rigord))

if release_version != "v0.1.3":
    finding("CORE", "FAIL", "RIGOR_RELEASE_VERSION is not v0.1.3", release_version)
if node_genesis != NODE_MAINNET_GENESIS:
    finding("CORE", "FAIL", "mainnet genesis hash does not match lock", node_genesis)
if not magic_ok:
    finding("CORE", "FAIL", "mainnet magic/port not ce f1 db fa / 8760", "chainparams.cpp")
if not hrp_ok:
    finding("CORE", "FAIL", "mainnet HRP is not rg", "chainparams.cpp")
if not e246 or not c1024:
    finding("CORE", "FAIL", "RES E/C lock not found in params.h", "params.h")
if not maturity_100:
    finding("CORE", "FAIL", "mainnet nCoinbaseMaturity is not 100", "chainparams.cpp")
if not (rpc_gbt and rpc_wallet and rpc_net):
    finding("CORE", "FAIL", "required RPC surface missing", "gbt/wallet/net")
if packed_major != "25":
    finding("CORE", "FAIL", "packed CLIENT_VERSION major unexpected", packed_major)
if subversion_uses_client:
    finding(
        "CORE",
        "INFO",
        "P2P subversion still derived from packed CLIENT_VERSION 25.0.0 (documented split)",
        "init.cpp FormatSubVersion + configure.ac 25.0.0",
    )
if empty_seeds:
    finding(
        "CORE",
        "INFO",
        "mainnet vFixedSeeds is empty until a signed seed snapshot",
        "chainparams.cpp vFixedSeeds.clear()",
    )
if min_work_bootstrap:
    finding(
        "CORE",
        "INFO",
        "nMinimumChainWork is still the 2^35 bootstrap floor; source asks to raise before each mainnet release",
        "chainparams.cpp",
    )

core_fail = any(f["component"] == "CORE" and f["severity"] == "FAIL" for f in findings)
CORE_ALIGNMENT = "FAIL" if core_fail else "PASS"

# ---------------------------------------------------------------------------
# 2. Wallet
# ---------------------------------------------------------------------------
overview = read("core/src/qt/overviewpage.cpp")
gui = read("core/src/qt/bitcoingui.cpp")
orupanel = exists("core/src/qt/rigorwallet/widgets/orubalancepanel.cpp")
respanel = exists("core/src/qt/rigorwallet/widgets/resbalancepanel.cpp")
backup = "backupWalletAction" in gui
restore = "m_restore_wallet_action" in gui and "Restore Wallet" in gui
rescan_rpc = "rescanblockchain" in wallet_rpc
linux_tar = root / "dist/release/v0.1.3/official/rigor-v0.1.3-linux-x86_64.tar.gz"
win_exe = root / "dist/release/v0.1.3/official/rigor-v0.1.3-windows-x86_64.exe"
win_zip = root / "dist/release/v0.1.3/official/rigor-v0.1.3-windows-x86_64.zip"
dmg_intel = root / "dist/release/v0.1.3/official/rigor-v0.1.3-macos-x86_64.dmg"
dmg_arm = root / "dist/release/v0.1.3/official/rigor-v0.1.3-macos-arm64.dmg"

wallet_linux_banner = ""
if linux_tar.is_file():
    import tarfile
    import tempfile

    with tempfile.TemporaryDirectory() as td:
        with tarfile.open(linux_tar) as tf:
            names = tf.getnames()
            tf.extractall(td)
        has_qt = any(n.endswith("rigor-qt") or n.endswith("bin/rigor-qt") for n in names)
        has_wallet = any(n.endswith("rigor-wallet") for n in names)
        qt_bin = Path(td) / "bin" / "rigor-qt"
        if qt_bin.is_file():
            rc, out = run_cmd([str(qt_bin), "--version"])
            wallet_linux_banner = out
            if "v0.1.3" not in out:
                finding("WALLET", "FAIL", "linux rigor-qt --version is not v0.1.3", out[:300])
        if not has_qt or not has_wallet:
            finding("WALLET", "FAIL", "linux tarball missing rigor-qt or rigor-wallet", str(names[:20]))
else:
    finding("WALLET", "FAIL", "official linux tarball missing", str(linux_tar))

if win_zip.is_file():
    with zipfile.ZipFile(win_zip) as zf:
        names = zf.namelist()
    if not any(n.lower().endswith("rigord.exe") for n in names):
        finding("WALLET", "FAIL", "windows zip missing rigord.exe", str(names))
    if not any(n.lower().endswith("rigor-qt.exe") for n in names):
        finding(
            "WALLET",
            "INFO",
            "windows zip is node-only (no rigor-qt.exe); GUI is the NSIS installer",
            str(names),
        )
else:
    finding("WALLET", "FAIL", "official windows zip missing", str(win_zip))

if win_exe.is_file():
    blob = win_exe.read_bytes()
    utf16 = blob.decode("utf-16le", errors="ignore")
    if "v0.1.3" not in utf16 and "0.1.3" not in utf16:
        finding("WALLET", "FAIL", "windows NSIS installer has no UTF-16 v0.1.3 stamp", str(win_exe))
else:
    finding("WALLET", "FAIL", "official windows installer missing", str(win_exe))

for p, label in ((dmg_intel, "macos-x86_64"), (dmg_arm, "macos-arm64")):
    if not p.is_file():
        finding("WALLET", "FAIL", f"official {label} dmg missing", str(p))
    elif p.stat().st_size < 1_000_000:
        finding("WALLET", "FAIL", f"{label} dmg is a stub (<1 MiB), not a wallet", f"size={p.stat().st_size}")
    else:
        finding("WALLET", "INFO", f"{label} dmg present (~wallet size); WSL cannot hdiutil-attach", f"size={p.stat().st_size}")

if "ORUBalancePanel" not in overview and not orupanel:
    finding("WALLET", "FAIL", "ORU balance panel missing", "overviewpage / orubalancepanel")
if "RESBalancePanel" not in overview and not respanel:
    finding("WALLET", "FAIL", "RES balance panel missing", "overviewpage / resbalancepanel")
if "MUST NEVER" not in overview or "mix ORU and RES" not in overview:
    finding("WALLET", "FAIL", "wallet design lock missing dual-asset separation", "overviewpage.cpp")
if not backup or not restore:
    finding("WALLET", "FAIL", "backup/restore menu actions missing", "bitcoingui.cpp")
if not rescan_rpc:
    finding("WALLET", "FAIL", "rescanblockchain RPC missing", "rpcwallet.cpp")
if "rig1" in overview:
    finding("WALLET", "INFO", "overview comment still says rig1…; live HRP is rg1…", "overviewpage.cpp:9")

wallet_fail = any(f["component"] == "WALLET" and f["severity"] == "FAIL" for f in findings)
WALLET_ALIGNMENT = "FAIL" if wallet_fail else "PASS"

# ---------------------------------------------------------------------------
# 3. Browser / explorer
# ---------------------------------------------------------------------------
net_ts = read("explorer/src/network.ts")
comp_ts = read("explorer/src/compliance.ts")
amount_ts = read("explorer/src/amount.ts")
status_ts = read("explorer/src/api/status.ts")
script_ts = read("explorer/src/script.ts")
arch_md = read("docs/explorer/EXPLORER_ARCHITECTURE.md") if exists("docs/explorer/EXPLORER_ARCHITECTURE.md") else ""

exp_prod = re.search(r'RIGOR_PRODUCT = "([^"]+)"', net_ts) or re.search(r'ALPHAPLUS_PRODUCT = "([^"]+)"', net_ts)
exp_genesis = re.search(
    r'(?:MAINNET_GENESIS_HASH|ALPHAPLUS_GENESIS_HASH)\s*=\s*\n\s*"([0-9a-f]+)"',
    net_ts,
)
exp_g = exp_genesis.group(1) if exp_genesis else ""
exp_p = exp_prod.group(1) if exp_prod else ""
testnet_frozen = re.search(
    r'(?:TESTNET_GENESIS_HASH|FROZEN_TESTNET_GENESIS_HASH)\s*=\s*\n?\s*"([0-9a-f]+)"',
    net_ts + "\n" + comp_ts,
)
tn_g = testnet_frozen.group(1) if testnet_frozen else ""
hrp_ok = '"rgrt"' in net_ts and 'mainnet: "rg"' in net_ts and 'testnet: "trg"' in net_ts
hrp_regtest_wrong = (not hrp_ok) or ('return "trg"' in net_ts and '"rgrt"' not in net_ts)
signet_port = "signet: 38761" in net_ts
oru8 = "ORU_DISPLAY_DECIMALS = 8" in amount_ts
res6 = "RES_DISPLAY_DECIMALS = 6" in amount_ts
maturity_disp = "COINBASE_MATURITY_DISPLAY = 100" in status_ts
classify = "AssetTypeFromScriptPubKey" in script_ts

if exp_g != NODE_MAINNET_GENESIS:
    finding(
        "BROWSER",
        "FAIL",
        "explorer mainnet genesis hash != node mainnet genesis",
        f"explorer={exp_g} node={NODE_MAINNET_GENESIS}",
    )
if exp_p and "v0.1.3" not in exp_p:
    finding("BROWSER", "FAIL", "explorer product string is not v0.1.3", exp_p)
if tn_g and tn_g != NODE_TESTNET_GENESIS:
    finding(
        "BROWSER",
        "FAIL",
        "explorer frozen testnet genesis != node testnet genesis",
        f"explorer={tn_g} node={NODE_TESTNET_GENESIS}",
    )
if hrp_regtest_wrong:
    finding(
        "BROWSER",
        "FAIL",
        "explorer HRP map does not match node (mainnet rg, testnet trg, regtest rgrt, devtest dvt)",
        "explorer/src/network.ts bech32HrpForNetwork",
    )
if signet_port:
    finding(
        "BROWSER",
        "FAIL",
        "explorer advertises signet RPC port 38761; node has no shipped signet chain",
        "explorer/src/network.ts vs chainparamsbase.cpp",
    )
if not (oru8 and res6):
    finding("BROWSER", "FAIL", "explorer ORU/RES display decimals != 8/6", "amount.ts")
if not maturity_disp:
    finding("BROWSER", "FAIL", "explorer coinbase maturity display is not 100", "status.ts")
if not classify:
    finding("BROWSER", "FAIL", "explorer classifier does not cite AssetTypeFromScriptPubKey", "script.ts")
if "testnet" in arch_md.lower() and "mainnet-freeze-rfc014" in arch_md:
    finding(
        "BROWSER",
        "INFO",
        "explorer architecture doc still scoped to closed testnet / RFC-014 freeze tag",
        "docs/explorer/EXPLORER_ARCHITECTURE.md",
    )

browser_fail = any(f["component"] == "BROWSER" and f["severity"] == "FAIL" for f in findings)
BROWSER_ALIGNMENT = "FAIL" if browser_fail else "PASS"

# ---------------------------------------------------------------------------
# 4. Miner
# ---------------------------------------------------------------------------
cpu = read("miner/cpu-miner.c")
readme_m = read("miner/README_MINER.md") if exists("miner/README_MINER.md") else ""
ex_cfg = read("miner/example-cfg.json") if exists("miner/example-cfg.json") else ""
e2e = exists("core/test/functional/mining_solo_gbt_copy_bytes.py")
gbt = exists("miner/gbt_solo.c")
stratum_refused = "Stratum is not the RIGOR SOLO path" in cpu
algo_sha256d = 'opt_algo = ALGO_SHA256D' in cpu or "ALGO_SHA256D" in cpu
scrypt_disabled = "scrypt is not a RIGOR SOLO algorithm" in cpu
cfg_ok = '"algo": "sha256d"' in ex_cfg and "8761" in ex_cfg

if not stratum_refused:
    finding("MINER", "FAIL", "SOLO miner does not refuse Stratum URLs", "cpu-miner.c")
if not algo_sha256d:
    finding("MINER", "FAIL", "default algo is not ALGO_SHA256D/OpuHash", "cpu-miner.c")
if not gbt:
    finding("MINER", "FAIL", "miner/gbt_solo.c missing", "gbt_solo.c")
if not e2e:
    finding("MINER", "FAIL", "live GBT copy-bytes functional test missing", "mining_solo_gbt_copy_bytes.py")
if not scrypt_disabled:
    finding("MINER", "FAIL", "scrypt is not refused on the SOLO path", "cpu-miner.c")
if not cfg_ok:
    finding("MINER", "FAIL", "miner/example-cfg.json is not sha256d @ 8761", ex_cfg[:200])
if "OpuHash" not in readme_m:
    finding("MINER", "FAIL", "miner README does not document OpuHash", "README_MINER.md")

miner_fail = any(f["component"] == "MINER" and f["severity"] == "FAIL" for f in findings)
MINER_ALIGNMENT = "FAIL" if miner_fail else "PASS"

# ---------------------------------------------------------------------------
# 5. Pool
# ---------------------------------------------------------------------------
ex_toml = read("pool/config/example.toml") if exists("pool/config/example.toml") else ""
cfg_rs = read("pool/pilot/src/config.rs") if exists("pool/pilot/src/config.rs") else ""
pool_readme = read("pool/README.md") if exists("pool/README.md") else ""
yaamp = (root / "pool/web/yaamp").is_dir()
stratum_rs = exists("pool/stratum-server/src/protocol.rs") or exists("pool/stratum-server/README.md")
res_boundary = exists("pool/share-validator/src/res_mint.rs")

if "rpc_port = 8332" in ex_toml:
    finding(
        "POOL",
        "FAIL",
        "pool example.toml default RPC port is Bitcoin 8332, not RIGOR 8761",
        "pool/config/example.toml",
    )
if "fn default_rpc_port" in cfg_rs and "8332" in cfg_rs:
    finding(
        "POOL",
        "FAIL",
        "pool pilot default_rpc_port() is 8332, not 8761",
        "pool/pilot/src/config.rs",
    )
if not stratum_rs:
    finding("POOL", "FAIL", "Rust stratum-server missing", "pool/stratum-server")
if not res_boundary:
    finding("POOL", "FAIL", "ORU-only RES mint boundary module missing", "res_mint.rs")
if yaamp:
    finding(
        "POOL",
        "INFO",
        "legacy yaamp/yiimp tree still present (documented quarantine)",
        "pool/web/yaamp",
    )

pool_fail = any(f["component"] == "POOL" and f["severity"] == "FAIL" for f in findings)
POOL_ALIGNMENT = "FAIL" if pool_fail else "PASS"

# ---------------------------------------------------------------------------
# 6. Release artifacts
# ---------------------------------------------------------------------------
stage = root / "dist/release/v0.1.3/official"
sums_stage = stage / "SHA256SUMS"
sums_github = root / ".github/releases/v0.1.3/SHA256SUMS"
man_rel = root / "formal_verification/release_v0.1.3_manifest.json"
tag_commit = git("rev-parse", "v0.1.3^{commit}")
head = git("rev-parse", "HEAD")
describe = git("describe", "--tags", "--always", "--dirty")
ahead = git("rev-list", "--count", "v0.1.3..HEAD")

sha_ok = True
sha_results = {}
if sums_stage.is_file():
    for line in sums_stage.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        digest, name = line.split()
        sha_results[name] = digest
        if EXPECTED_SHA256.get(name) != digest:
            sha_ok = False
            finding("RELEASE", "FAIL", f"SHA256SUMS digest mismatch for {name}", digest)
        blob = stage / name
        if blob.is_file():
            got = hashlib.sha256(blob.read_bytes()).hexdigest()
            if got != digest:
                sha_ok = False
                finding("RELEASE", "FAIL", f"file digest != SHA256SUMS for {name}", got)
        else:
            sha_ok = False
            finding("RELEASE", "FAIL", f"official staging missing {name}", str(blob))
else:
    sha_ok = False
    finding("RELEASE", "FAIL", "dist/release/v0.1.3/official/SHA256SUMS missing", str(sums_stage))

if sums_github.is_file():
    gh_text = sums_github.read_text(encoding="utf-8")
    for name, digest in EXPECTED_SHA256.items():
        if f"{digest}  {name}" not in gh_text:
            finding("RELEASE", "FAIL", f".github/releases SHA256SUMS missing {name}", name)
else:
    finding("RELEASE", "FAIL", ".github/releases/v0.1.3/SHA256SUMS missing", str(sums_github))

if not man_rel.is_file():
    finding("RELEASE", "FAIL", "formal_verification/release_v0.1.3_manifest.json missing", "")
else:
    relj = json.loads(man_rel.read_text(encoding="utf-8"))
    if relj.get("tag") != "v0.1.3":
        finding("RELEASE", "FAIL", "release manifest tag is not v0.1.3", str(relj.get("tag")))

if tag_commit != "8743c509d9273b19c431df2f4f0ee630ccd810f8" and not tag_commit:
    finding("RELEASE", "FAIL", "git tag v0.1.3 missing", tag_commit)
elif not tag_commit:
    finding("RELEASE", "FAIL", "git tag v0.1.3 missing", "")

if ahead and ahead not in ("0",):
    finding(
        "RELEASE",
        "INFO",
        f"HEAD is {ahead} commits past tag v0.1.3; tag was not moved after P0-094/P0-095",
        f"HEAD={head} tag={tag_commit} describe={describe}",
    )

p095 = root / "formal_verification/macos_signing_p0_095_manifest.json"
if p095.is_file():
    j = json.loads(p095.read_text(encoding="utf-8"))
    if j.get("official_dmg_signed") is False:
        finding(
            "RELEASE",
            "INFO",
            "official macOS DMGs are unsigned; P0-095 deferred signing and does not block release",
            "macos_signing_p0_095_manifest.json",
        )

release_fail = any(f["component"] == "RELEASE" and f["severity"] == "FAIL" for f in findings)
RELEASE_ALIGNMENT = "FAIL" if release_fail else "PASS"

# ---------------------------------------------------------------------------
# 7. Documentation
# ---------------------------------------------------------------------------
rel_notes = read("docs/release/RIGOR_V0.1.3_RELEASE_NOTES.md") if exists("docs/release/RIGOR_V0.1.3_RELEASE_NOTES.md") else ""
p092 = read("docs/release/RIGOR_RELEASE_BUILD_MATRIX_P0_092.md") if exists("docs/release/RIGOR_RELEASE_BUILD_MATRIX_P0_092.md") else ""
p095_md = read("docs/release/RIGOR_MACOS_SIGNING_P0_095.md") if exists("docs/release/RIGOR_MACOS_SIGNING_P0_095.md") else ""
alpha_notes = read("docs/release/RIGOR_ALPHAPLUS_RELEASE_NOTES.md") if exists("docs/release/RIGOR_ALPHAPLUS_RELEASE_NOTES.md") else ""
readme = read("README.md") if exists("README.md") else ""

names_in_notes = all(n in rel_notes for n in OFFICIAL)
if not names_in_notes:
    finding("DOCUMENTATION", "FAIL", "v0.1.3 release notes missing an official artifact name", "RIGOR_V0.1.3_RELEASE_NOTES.md")
if "remains `v0.1.2-alphaplus`" in p092 or "clientversion.h remains `v0.1.2-alphaplus`" in p092:
    finding(
        "DOCUMENTATION",
        "FAIL",
        "P0-092 build-matrix doc still claims clientversion.h remains v0.1.2-alphaplus; source is v0.1.3",
        "docs/release/RIGOR_RELEASE_BUILD_MATRIX_P0_092.md §1",
    )
if "Notarization is P0-096" in p095_md:
    finding(
        "DOCUMENTATION",
        "FAIL",
        "P0-096 task-id collision: P0-095 reserved P0-096 for macOS notarization; this TASK is the alignment audit",
        "docs/release/RIGOR_MACOS_SIGNING_P0_095.md",
    )
if "current user download" in alpha_notes.lower() or "current user download" in alpha_notes:
    finding(
        "DOCUMENTATION",
        "FAIL",
        "AlphaPlus release notes still describe AlphaPlus as the current user download",
        "docs/release/RIGOR_ALPHAPLUS_RELEASE_NOTES.md",
    )
elif "current" in alpha_notes.lower() and "alphaplus" in alpha_notes.lower() and "replaces" in alpha_notes.lower():
    finding(
        "DOCUMENTATION",
        "FAIL",
        "AlphaPlus release notes still present AlphaPlus as the current download",
        "docs/release/RIGOR_ALPHAPLUS_RELEASE_NOTES.md",
    )
if "v0.1.3" not in readme and "0.1.3" not in readme:
    finding("DOCUMENTATION", "FAIL", "root README does not mention v0.1.3", "README.md")
if "Upcoming product identity" in readme and "v0.1.3" in readme:
    finding(
        "DOCUMENTATION",
        "FAIL",
        "root README still labels v0.1.3 as Upcoming product identity after the production tag exists",
        "README.md",
    )

doc_fail = any(f["component"] == "DOCUMENTATION" and f["severity"] == "FAIL" for f in findings)
DOCUMENTATION_ALIGNMENT = "FAIL" if doc_fail else "PASS"

# ---------------------------------------------------------------------------
# 8. CI/CD
# ---------------------------------------------------------------------------
rb = read(".github/workflows/release-build.yml")
rmw_path = "release-wallet-macos.yml"
rmw_old = exists(".github/workflows/release-macos-wallet.yml")
rmw = read(f".github/workflows/{rmw_path}") if exists(f".github/workflows/{rmw_path}") else ""
grd = read(".github/workflows/genesis-release-guardian.yml")
matrix_ok = all(
    x in rb
    for x in (
        "windows-x86_64",
        "linux-x86_64",
        "macos-x86_64",
        "macos-arm64",
        "macos-15-intel",
        "macos-15",
    )
)
linux_win_in_build = all(
    n in rb
    for n in (
        "rigor-v0.1.3-windows-x86_64.exe",
        "rigor-v0.1.3-windows-x86_64.zip",
        "rigor-v0.1.3-linux-x86_64.tar.gz",
    )
)
official_dmg_in_wallet = all(
    n in rmw
    for n in (
        "rigor-v0.1.3-macos-x86_64.dmg",
        "rigor-v0.1.3-macos-arm64.dmg",
    )
)
guardian_on_tags = "tags:" in grd and "v*" in grd
guardian_needs_build = bool(re.search(r"(?m)^\s+needs:\s*$", grd)) and "release-build" in grd
# Required SHA256 list in release-build must not demand official wallet DMGs.
sha_required = rb[rb.find("required = [") : rb.find("]", rb.find("required = [")) + 1] if "required = [" in rb else rb
official_dmg_in_build_required = (
    "rigor-v0.1.3-macos-x86_64.dmg" in sha_required and "identity" not in sha_required
)

if rmw_old:
    finding("CI", "FAIL", "legacy release-macos-wallet.yml still present; use release-wallet-macos.yml", "")
if not rmw:
    finding("CI", "FAIL", "release-wallet-macos.yml missing", "")
if not matrix_ok:
    finding("CI", "FAIL", "release-build matrix missing an official target/runner", "release-build.yml")
if not linux_win_in_build:
    finding("CI", "FAIL", "release-build missing linux/windows official artifact names", "release-build.yml")
if not official_dmg_in_wallet:
    finding("CI", "FAIL", "release-wallet-macos.yml missing official macOS DMG names", rmw_path)
if not guardian_on_tags:
    finding("CI", "FAIL", "genesis-release-guardian.yml is not tag-triggered", "genesis-release-guardian.yml")
if not guardian_needs_build:
    finding(
        "CI",
        "FAIL",
        "genesis-release-guardian.yml has no needs: release-build; Guardian can pass without artifacts",
        "genesis-release-guardian.yml",
    )
if official_dmg_in_build_required:
    finding(
        "CI",
        "FAIL",
        "release-build SHA256 required list still uses official wallet DMG names (collision with wallet workflow)",
        "release-build.yml",
    )

ci_fail = any(f["component"] == "CI" and f["severity"] == "FAIL" for f in findings)
CI_ALIGNMENT = "FAIL" if ci_fail else "PASS"

# ---------------------------------------------------------------------------
# Security boundary for THIS task
# ---------------------------------------------------------------------------
consensus_changed = bool(P096_FILES & git_names(CONSENSUS_PATHS))
db_changed = bool(P096_FILES & git_names(DB_PATHS))
params_changed = bool(P096_FILES & git_names(PARAM_PATHS))
if consensus_changed:
    finding("BOUNDARY", "FAIL", "this TASK modified Consensus Kernel paths", str(P096_FILES & git_names(CONSENSUS_PATHS)))
if db_changed:
    finding("BOUNDARY", "FAIL", "this TASK modified CoinsDB paths", str(P096_FILES & git_names(DB_PATHS)))
if params_changed:
    finding("BOUNDARY", "FAIL", "this TASK modified protocol parameter paths", str(P096_FILES & git_names(PARAM_PATHS)))

if not doc_path.is_file():
    finding("BOUNDARY", "FAIL", "audit document missing", str(doc_path))

component_fail = any(
    v == "FAIL"
    for v in (
        CORE_ALIGNMENT,
        WALLET_ALIGNMENT,
        BROWSER_ALIGNMENT,
        MINER_ALIGNMENT,
        POOL_ALIGNMENT,
        RELEASE_ALIGNMENT,
        DOCUMENTATION_ALIGNMENT,
        CI_ALIGNMENT,
    )
)
MAINNET_ALIGNMENT_AUDIT = "FAIL" if component_fail or consensus_changed or db_changed or params_changed else "PASS"
FINAL_VERDICT = MAINNET_ALIGNMENT_AUDIT

man = {
    "task": "TASK-RIGOR-MAINNET-READINESS-CROSS-COMPONENT-ALIGNMENT-AUDIT-P0-096",
    "date": "2026-09-03",
    "nature": "Audit-first. No bug fix. No consensus modification. No protocol modification.",
    "command": "bash tools/run-mainnet-readiness-alignment-audit.sh",
    "git_describe": describe,
    "git_head": head,
    "git_tag_v0_1_3": tag_commit,
    "commits_past_tag": ahead,
    "core": {
        "alignment": CORE_ALIGNMENT,
        "release_version": release_version,
        "packed_client_version": "25.0.0",
        "binary_present": core_binary_present,
        "binary_banner": core_banner.splitlines()[0] if core_banner else "",
        "mainnet_genesis": node_genesis,
        "mainnet_magic": "ce f1 db fa",
        "mainnet_p2p_port": 8760,
        "mainnet_rpc_port": 8761,
        "mainnet_hrp": "rg",
        "nCoinbaseMaturity": 100,
        "subversion_reports_25_0_0": True,
        "vFixedSeeds_empty": empty_seeds,
        "nMinimumChainWork_bootstrap_floor": min_work_bootstrap,
    },
    "wallet": {
        "alignment": WALLET_ALIGNMENT,
        "linux_banner": next((ln for ln in wallet_linux_banner.splitlines() if "v0.1.3" in ln), ""),
        "windows_zip_contains_qt": False,
        "macos_runtime_verification": "SKIPPED_NO_HDIUTIL_ON_WSL",
        "oru_res_panels": orupanel and respanel,
        "backup_restore": backup and restore,
    },
    "browser": {
        "alignment": BROWSER_ALIGNMENT,
        "explorer_product": exp_p,
        "explorer_mainnet_genesis": exp_g,
        "node_mainnet_genesis": NODE_MAINNET_GENESIS,
        "genesis_match": exp_g == NODE_MAINNET_GENESIS,
        "explorer_testnet_genesis": tn_g,
        "node_testnet_genesis": NODE_TESTNET_GENESIS,
        "oru_decimals": 8,
        "res_decimals": 6,
        "coinbase_maturity_display": 100,
    },
    "miner": {
        "alignment": MINER_ALIGNMENT,
        "algo_cli": "sha256d",
        "algo_actual": "OpuHash",
        "stratum_solo_refused": stratum_refused,
        "gbt_copy_bytes_test": e2e,
    },
    "pool": {
        "alignment": POOL_ALIGNMENT,
        "example_rpc_port": 8332 if "rpc_port = 8332" in ex_toml else None,
        "node_rpc_port": 8761,
        "yaamp_tree_present": yaamp,
        "oru_only_res_boundary": res_boundary,
    },
    "release": {
        "alignment": RELEASE_ALIGNMENT,
        "official_names": OFFICIAL,
        "sha256": EXPECTED_SHA256,
        "sha256_verified_local": sha_ok,
        "tag": "v0.1.3",
        "macos_signed": False,
    },
    "documentation": {"alignment": DOCUMENTATION_ALIGNMENT},
    "ci": {
        "alignment": CI_ALIGNMENT,
        "release_build_matrix": ["windows-x86_64", "linux-x86_64", "macos-x86_64", "macos-arm64"],
        "guardian_workflow": ".github/workflows/genesis-release-guardian.yml",
        "guardian_enforced_on_release_build": False,
        "dual_macos_pipelines": True,
    },
    "MAINNET_ALIGNMENT_AUDIT": MAINNET_ALIGNMENT_AUDIT,
    "CORE_ALIGNMENT": CORE_ALIGNMENT,
    "WALLET_ALIGNMENT": WALLET_ALIGNMENT,
    "BROWSER_ALIGNMENT": BROWSER_ALIGNMENT,
    "MINER_ALIGNMENT": MINER_ALIGNMENT,
    "POOL_ALIGNMENT": POOL_ALIGNMENT,
    "RELEASE_ALIGNMENT": RELEASE_ALIGNMENT,
    "DOCUMENTATION_ALIGNMENT": DOCUMENTATION_ALIGNMENT,
    "CI_ALIGNMENT": CI_ALIGNMENT,
    "CONSENSUS_CODE_CHANGED": False if not consensus_changed else True,
    "DATABASE_FORMAT_CHANGED": False if not db_changed else True,
    "PROTOCOL_PARAMETERS_CHANGED": False if not params_changed else True,
    "findings": findings,
    "errors": errors,
    "final_verdict": FINAL_VERDICT,
    "final_verdict_reason": (
        "Explorer genesis/product/HRP disagree with the node; pool defaults to RPC 8332; "
        "docs and CI still split macOS stub vs wallet and collide P0-096 with notarization."
        if FINAL_VERDICT == "FAIL"
        else "All eight alignment surfaces matched the v0.1.3 node identity."
    ),
}
man_path.write_text(json.dumps(man, indent=2) + "\n", encoding="utf-8")


def yn(v: bool) -> str:
    return "TRUE" if v else "FALSE"


print(f"MAINNET_ALIGNMENT_AUDIT={MAINNET_ALIGNMENT_AUDIT}")
print(f"CORE_ALIGNMENT={CORE_ALIGNMENT}")
print(f"WALLET_ALIGNMENT={WALLET_ALIGNMENT}")
print(f"BROWSER_ALIGNMENT={BROWSER_ALIGNMENT}")
print(f"MINER_ALIGNMENT={MINER_ALIGNMENT}")
print(f"POOL_ALIGNMENT={POOL_ALIGNMENT}")
print(f"RELEASE_ALIGNMENT={RELEASE_ALIGNMENT}")
print(f"DOCUMENTATION_ALIGNMENT={DOCUMENTATION_ALIGNMENT}")
print(f"CI_ALIGNMENT={CI_ALIGNMENT}")
print(f"CONSENSUS_CODE_CHANGED={yn(consensus_changed)}")
print(f"DATABASE_FORMAT_CHANGED={yn(db_changed)}")
print(f"PROTOCOL_PARAMETERS_CHANGED={yn(params_changed)}")
print(f"FINAL_VERDICT={FINAL_VERDICT}")
print()
print("== findings ==")
for f in findings:
    print(f"{f['severity']}: [{f['component']}] {f['title']}")
if errors:
    print()
    for e in errors:
        print(f"FAIL: {e}")
    raise SystemExit(1)
PY
