#!/usr/bin/env bash
# P0-098: official SOLO miner SHA3-256 RGRPOW01 alignment audit.
# Miner-side only. Does not modify Consensus Kernel, CheckProofOfWork,
# block validation, difficulty, ASERT, genesis, network parameters, or
# pool consensus.
#
# FAIL CLOSED.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
export PYTHONUNBUFFERED=1

MANIFEST="$ROOT/formal_verification/miner_pow_alignment_manifest.json"

echo "== P098 SOLO miner PoW alignment audit =="
echo "command=bash tools/run-miner-pow-alignment-audit.sh"
echo "nature=miner implementation only; consensus frozen"

python3 - "$ROOT" "$MANIFEST" <<'PY'
from __future__ import annotations

import hashlib
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


findings: list[dict] = []
fails: list[str] = []


def fail(gate: str, msg: str) -> None:
    fails.append(f"{gate}: {msg}")
    findings.append({"severity": "FAIL", "gate": gate, "message": msg})


def info(gate: str, msg: str) -> None:
    findings.append({"severity": "INFO", "gate": gate, "message": msg})


# --- Node SSOT still SHA3 RGRPOW01 (must not have been rewritten) ---
pow_digest = read("core/src/rigor_kernel/pow_digest.h")
pow_cpp = read("core/src/pow.cpp")
if "RGRPOW01" not in pow_digest or "GetRigorPowHash" not in pow_digest:
    fail("NODE_POW", "rigor_kernel::GetRigorPowHash / RGRPOW01 missing")
if "GetRigorPowHash" not in pow_cpp or "CheckProofOfWork" not in pow_cpp:
    fail("NODE_POW", "CheckProofOfWork does not call GetRigorPowHash")

CONSENSUS_PATHS = [
    "core/src/consensus",
    "core/src/script/rigor_classify.cpp",
    "core/src/script/rigor_classify.h",
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
POOL_CONSENSUS_PATHS = [
    "pool/share-validator",
    "pool/rigor-gbt",
    "pool/block-assembler",
    "pool/stratum-server",
    "pool/rigor-miner-agent",
]

consensus_changed = sorted(git_names(CONSENSUS_PATHS))
db_changed = sorted(git_names(DB_PATHS))
pool_changed = sorted(git_names(POOL_CONSENSUS_PATHS))
if consensus_changed:
    fail("CONSENSUS_CODE_CHANGED", "consensus paths dirty: " + ", ".join(consensus_changed))
if db_changed:
    fail("DATABASE_FORMAT_CHANGED", "db paths dirty: " + ", ".join(db_changed))
if pool_changed:
    fail("POOL_SOLO_COMPATIBILITY", "pool consensus/GBT paths dirty: " + ", ".join(pool_changed))

CONSENSUS_CODE_CHANGED = "TRUE" if consensus_changed else "FALSE"
DATABASE_FORMAT_CHANGED = "TRUE" if db_changed else "FALSE"

# --- Miner hash path ---
rgrpow_c = read("miner/rgrpow.c")
rgrpow_h = read("miner/rgrpow.h")
sha2 = read("miner/sha2.c")
cpu = read("miner/cpu-miner.c")
make_am = read("miner/Makefile.am")
gbt = read("miner/gbt_solo.c")
test_c = read("miner/test_gbt_solo.c")

if not exists("miner/rgrpow.c") or not exists("miner/rgrpow.h"):
    fail("MINER_POW_ALIGNMENT", "miner/rgrpow.{c,h} missing")

needles_rgrpow = [
    'RGRPOW_DOMAIN_TAG',
    "RGRPOW01",
    "header80",
    "0x06",
    "rgrpow_hash80",
]
for n in needles_rgrpow:
    if n not in rgrpow_c and n not in rgrpow_h:
        fail("MINER_POW_ALIGNMENT", f"rgrpow missing {n!r}")

if "rgrpow_hash80(wire, digest)" not in sha2:
    fail("MINER_POW_ALIGNMENT", "scanhash path does not call rgrpow_hash80")
if "le32enc(wire" not in sha2:
    fail("MINER_POW_ALIGNMENT", "scanhash does not serialize header with le32enc")
if "opus_hash" in sha2 or "GetOpusHash" in sha2:
    fail("MINER_POW_ALIGNMENT", "sha2.c still references OpuHash grind")
if "rgrpow_hash80" not in cpu or "rgrpow_selftest" not in cpu:
    fail("MINER_POW_ALIGNMENT", "cpu-miner.c missing RGRPOW01 self-test")
if "rgrpow.c" not in make_am:
    fail("MINER_POW_ALIGNMENT", "Makefile.am does not build rgrpow.c")
if "opus_hash.cpp" in make_am:
    fail("MINER_POW_ALIGNMENT", "Makefile.am still lists opus_hash.cpp")

# Exact preimage: tag || 80-byte header, no extra padding in C
if "memcpy(preimage, RGRPOW_DOMAIN_TAG" not in rgrpow_c:
    fail("MINER_POW_ALIGNMENT", "preimage does not start with RGRPOW01 tag")
if "memcpy(preimage + RGRPOW_DOMAIN_TAG_LEN, header80" not in rgrpow_c:
    fail("MINER_POW_ALIGNMENT", "preimage does not append header_80")

# Python hashlib pin of the node zero-header vector
zero_digest = hashlib.sha3_256(b"RGRPOW01" + bytes(80)).digest()
if zero_digest.hex() != "dd057fc1df8f5b9466e834cc9d7b819e2d511c75b43675b577f6d9f5d6de04fc":
    fail("SHA3_POW_VECTOR_TEST", f"hashlib SHA3 zero-header digest={zero_digest.hex()}")
else:
    info("SHA3_POW_VECTOR_TEST", "hashlib.sha3_256(RGRPOW01||zeros80) matches node vector")

# --- OpuHash mining path removed ---
if exists("miner/opus_hash.cpp") or exists("miner/opus_hash.h"):
    fail("OPUHASH_PATH_REMOVED", "miner/opus_hash.{cpp,h} still present")

miner_src_hits = []
for rel in [
    "miner/sha2.c",
    "miner/cpu-miner.c",
    "miner/gbt_solo.c",
    "miner/rgrpow.c",
    "miner/rgrpow.h",
    "miner/Makefile.am",
]:
    txt = read(rel)
    if re.search(r"opus_hash\s*\(", txt):
        miner_src_hits.append(rel)
    if re.search(r"\bOpuHash\b", txt) and rel not in ("miner/test_gbt_solo.c",):
        miner_src_hits.append(rel + ":OpuHash")
    if re.search(r"AlphaPlus", txt):
        miner_src_hits.append(rel + ":AlphaPlus")
if miner_src_hits:
    fail("OPUHASH_PATH_REMOVED", "mining identity/path residue: " + ", ".join(miner_src_hits))
if "rgrpow01" not in cpu:
    fail("OPUHASH_PATH_REMOVED", "cpu-miner CLI missing rgrpow01")
if re.search(r'algo.*=.*"opushash"', read("miner/example-cfg.json"), re.I):
    fail("OPUHASH_PATH_REMOVED", "example-cfg.json still names opushash")

# --- GBT / submitblock SOLO template path ---
if "default_coinbase_tx" not in gbt:
    fail("SOLO_BLOCK_TEMPLATE_TEST", "gbt_solo.c does not copy default_coinbase_tx")
if "header_words[19]" not in gbt:
    fail("SOLO_BLOCK_TEMPLATE_TEST", "gbt_solo.c missing 80-byte header_words nonce slot")
if "submitblock" not in cpu:
    fail("SOLO_BLOCK_TEMPLATE_TEST", "cpu-miner.c missing submitblock")
if "rgrpow_hash80" not in test_c or "GetRigorPowHash" not in test_c:
    fail("SOLO_BLOCK_TEMPLATE_TEST", "test_gbt_solo.c missing SHA3 vs node vectors")
if "opuhash_zero" not in test_c:
    fail("SHA3_POW_VECTOR_TEST", "missing negative OpuHash digest test")

# --- Docs ---
docs = [
    read("docs/miner/README.md"),
    read("miner/README_MINER.md"),
    read("README.md"),
    read("docs/security/RIGOR_SOLO_MINER_POW_ALIGNMENT_P0_098.md"),
]
formula = 'SHA3-256("RGRPOW01" || header_80)'
for i, text in enumerate(["docs/miner/README.md", "miner/README_MINER.md", "README.md"]):
    body = docs[i]
    if formula not in body and 'SHA3-256("RGRPOW01" || header_80)' not in body:
        # README.md uses digest = SHA3-256("RGRPOW01" || header_80)
        if 'SHA3-256("RGRPOW01"' not in body:
            fail("MINER_POW_ALIGNMENT", f"{text} missing SHA3-256 RGRPOW01 formula")
if not exists("docs/security/RIGOR_SOLO_MINER_POW_ALIGNMENT_P0_098.md"):
    fail("DELIVERABLE", "missing docs/security/RIGOR_SOLO_MINER_POW_ALIGNMENT_P0_098.md")

# --- Pool SOLO compatibility (read-only) ---
pool_readme = read("pool/README.md")
gbt_rs = read("pool/rigor-gbt/src/rpc.rs")
example = read("pool/config/example.toml")
if "8761" not in pool_readme or "8761" not in example:
    fail("POOL_SOLO_COMPATIBILITY", "pool example/README missing RPC 8761")
if "getblocktemplate" not in gbt_rs or "submitblock" not in gbt_rs:
    fail("POOL_SOLO_COMPATIBILITY", "pool/rigor-gbt missing GBT/submitblock RPC")
if not exists("pool/share-validator/src/opuhash.rs"):
    fail("POOL_SOLO_COMPATIBILITY", "pool share-validator unexpectedly removed")
info("POOL_SOLO_COMPATIBILITY", "pool GBT+submitblock+8761 unchanged; pool consensus not edited")

# --- Build + run miner tests ---
test_out = ""
test_rc = 1
miner_dir = root / "miner"
if not (miner_dir / "Makefile").is_file():
    fail("SOLO_BLOCK_TEMPLATE_TEST", "miner/Makefile missing; run ./configure in miner/")
else:
    build = subprocess.run(
        ["make", "-C", str(miner_dir), "test_gbt_solo"],
        capture_output=True,
        text=True,
        check=False,
    )
    if build.returncode != 0:
        fail("SOLO_BLOCK_TEMPLATE_TEST", "make test_gbt_solo failed: " + (build.stderr or build.stdout)[-800:])
    else:
        run = subprocess.run(
            [str(miner_dir / "test_gbt_solo")],
            capture_output=True,
            text=True,
            check=False,
            cwd=str(miner_dir),
        )
        test_out = (run.stdout or "") + (run.stderr or "")
        test_rc = run.returncode
        if test_rc != 0:
            fail("SOLO_BLOCK_TEMPLATE_TEST", "test_gbt_solo exit %s\n%s" % (test_rc, test_out[-800:]))

sha3_needles = [
    "SHA3_POW_VECTOR FIPS 202",
    "SHA3_POW_VECTOR zero header matches GetRigorPowHash",
    "SHA3_POW_VECTOR typed header matches GetRigorPowHash",
]
solo_needles = [
    "golden coinbase bytes copied verbatim",
    "SOLO_BLOCK_TEMPLATE miner-generated header meets SHA3 CheckProofOfWork",
    "coinbase bytes identical to default_coinbase_tx",
]
opu_needles = [
    "OpuHash(zero header) != SHA3-256(RGRPOW01||zero header)",
    "OpuHash-era header fails SHA3 CheckProofOfWork vs tight nBits",
]

SHA3_POW_VECTOR_TEST = "PASS"
SOLO_BLOCK_TEMPLATE_TEST = "PASS"
if test_rc != 0:
    SHA3_POW_VECTOR_TEST = "FAIL"
    SOLO_BLOCK_TEMPLATE_TEST = "FAIL"
else:
    for n in sha3_needles:
        if f"PASS {n}" not in test_out:
            SHA3_POW_VECTOR_TEST = "FAIL"
            fail("SHA3_POW_VECTOR_TEST", f"missing PASS {n}")
    for n in opu_needles:
        if f"PASS {n}" not in test_out:
            SHA3_POW_VECTOR_TEST = "FAIL"
            fail("SHA3_POW_VECTOR_TEST", f"missing PASS {n}")
    for n in solo_needles:
        if f"PASS {n}" not in test_out:
            SOLO_BLOCK_TEMPLATE_TEST = "FAIL"
            fail("SOLO_BLOCK_TEMPLATE_TEST", f"missing PASS {n}")
    if "ALL PASS" not in test_out:
        SOLO_BLOCK_TEMPLATE_TEST = "FAIL"
        fail("SOLO_BLOCK_TEMPLATE_TEST", "test_gbt_solo did not print ALL PASS")

MINER_POW_ALIGNMENT = "PASS"
OPUHASH_PATH_REMOVED = "PASS"
POOL_SOLO_COMPATIBILITY = "PASS"
for f in fails:
    if f.startswith("MINER_POW_ALIGNMENT:") or f.startswith("DELIVERABLE:") or f.startswith("NODE_POW:"):
        MINER_POW_ALIGNMENT = "FAIL"
    if f.startswith("OPUHASH_PATH_REMOVED:"):
        OPUHASH_PATH_REMOVED = "FAIL"
    if f.startswith("POOL_SOLO_COMPATIBILITY:"):
        POOL_SOLO_COMPATIBILITY = "FAIL"
    if f.startswith("SHA3_POW_VECTOR_TEST:"):
        SHA3_POW_VECTOR_TEST = "FAIL"
    if f.startswith("SOLO_BLOCK_TEMPLATE_TEST:"):
        SOLO_BLOCK_TEMPLATE_TEST = "FAIL"

if CONSENSUS_CODE_CHANGED == "TRUE":
    MINER_POW_ALIGNMENT = "FAIL"

FINAL_VERDICT = (
    "PASS"
    if (
        MINER_POW_ALIGNMENT == "PASS"
        and SOLO_BLOCK_TEMPLATE_TEST == "PASS"
        and SHA3_POW_VECTOR_TEST == "PASS"
        and OPUHASH_PATH_REMOVED == "PASS"
        and POOL_SOLO_COMPATIBILITY == "PASS"
        and CONSENSUS_CODE_CHANGED == "FALSE"
        and DATABASE_FORMAT_CHANGED == "FALSE"
        and not fails
    )
    else "FAIL"
)

describe, head = git_describe()
manifest = {
 "task": "",
    "date": str(date.today()),
    "nature": "Miner implementation only. Node CheckProofOfWork is SSOT. No consensus modification.",
    "command": "bash tools/run-miner-pow-alignment-audit.sh",
    "git_describe": describe,
    "git_head": head,
    "pow": "SHA3-256(RGRPOW01 || header_80)",
    "MINER_POW_ALIGNMENT": MINER_POW_ALIGNMENT,
    "SOLO_BLOCK_TEMPLATE_TEST": SOLO_BLOCK_TEMPLATE_TEST,
    "SHA3_POW_VECTOR_TEST": SHA3_POW_VECTOR_TEST,
    "OPUHASH_PATH_REMOVED": OPUHASH_PATH_REMOVED,
    "POOL_SOLO_COMPATIBILITY": POOL_SOLO_COMPATIBILITY,
    "CONSENSUS_CODE_CHANGED": CONSENSUS_CODE_CHANGED,
    "DATABASE_FORMAT_CHANGED": DATABASE_FORMAT_CHANGED,
    "FINAL_VERDICT": FINAL_VERDICT,
    "fails": fails,
    "findings": findings,
}
man_path.parent.mkdir(parents=True, exist_ok=True)
man_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

print(f"MINER_POW_ALIGNMENT={MINER_POW_ALIGNMENT}")
print(f"SOLO_BLOCK_TEMPLATE_TEST={SOLO_BLOCK_TEMPLATE_TEST}")
print(f"SHA3_POW_VECTOR_TEST={SHA3_POW_VECTOR_TEST}")
print(f"OPUHASH_PATH_REMOVED={OPUHASH_PATH_REMOVED}")
print(f"POOL_SOLO_COMPATIBILITY={POOL_SOLO_COMPATIBILITY}")
print(f"CONSENSUS_CODE_CHANGED={CONSENSUS_CODE_CHANGED}")
print(f"DATABASE_FORMAT_CHANGED={DATABASE_FORMAT_CHANGED}")
print(f"FINAL_VERDICT={FINAL_VERDICT}")
if fails:
    print("-- FAIL CLOSED --")
    for f in fails:
        print("FAIL", f)
    sys.exit(1)
print("OK")
sys.exit(0)
PY
