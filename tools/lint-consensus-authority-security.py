#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P063: adversarial authority-drift audit.

Unique production bodies only. A second implementation is FAIL.
Does not rewrite formulas. Does not add consensus authority.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "core/src"
VALIDATION = SRC / "validation.cpp"
CLASSIFY = SRC / "script/rigor_classify.cpp"
STANDARD = SRC / "script/standard.cpp"
ISSUANCE = SRC / "consensus/rigor_issuance.cpp"
SUPPLY = SRC / "consensus/rigor_supply.cpp"
DRAW = SRC / "consensus/convergence_logic.cpp"
PREDICATE = SRC / "script/predicate_verify.cpp"
LOCK = SRC / "consensus/rigor_consensus_lock.h"
MAKEFILE = SRC / "Makefile.am"

SKIP = {
    "test",
    "fuzz",
    "bench",
    "qt",
    "wallet",
    "rpc",
    "bridge",
    "shadow",
    "txkernel",
    "utxo",
    "kernel_migration",
    "kernel_canary",
    "ipc",
    "node",
}

AUTHORITY = {
    "GetBlockSubsidy": (
        re.compile(r"^CAmount GetBlockSubsidy\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        ISSUANCE,
        "issuance",
    ),
    "CheckResMintArithmetic": (
        re.compile(r"inline constexpr bool CheckResMintArithmetic\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        SRC / "consensus/res_issuance.h",
        "RES",
    ),
    "ClassifyRigorTxout": (
        re.compile(r"^TxoutType ClassifyRigorTxout\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        CLASSIFY,
        "classify",
    ),
    "RigorSupplyDelta": (
        re.compile(r"^bool RigorSupplyDelta\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        SUPPLY,
        "supply",
    ),
    "VerifyRigorPredicate": (
        re.compile(r"^bool VerifyRigorPredicate\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        PREDICATE,
        "spend",
    ),
}

FORMULA = re.compile(
    r"nHeight\s*/\s*nSubsidyHalvingInterval|"
    r"base\s*>>\s*halvings|"
    r"\(\s*(?:h|height|nHeight)\s*-\s*1\s*\)\s*/\s*210000"
)
CIRCULATING_ASSIGN = re.compile(r"oru_circulating_delta\s*=")
EVAL_DEF = re.compile(r"\bbool\s+EvalScript\s*\(")
VERIFY_DEF = re.compile(r"\bbool\s+VerifyScript\s*\(")
EVAL_CALL = re.compile(r"\bEvalScript\s*\(")
VERIFY_CALL = re.compile(r"\bVerifyScript\s*\(")
WINNER_LOOP = re.compile(r"Mul256x256Full")
CHAINSTATE = re.compile(r"\b(CCoinsViewCache|CBlockIndex|CDBWrapper|LevelDB|chainstate)\b")
KERNEL_PURE = {
    ISSUANCE.resolve(),
    SUPPLY.resolve(),
    CLASSIFY.resolve(),
    LOCK.resolve(),
}


def prod_files() -> list[Path]:
    out: list[Path] = []
    for p in SRC.rglob("*"):
        if not p.is_file() or p.suffix not in {".h", ".cpp"}:
            continue
        if any(s in p.parts for s in SKIP):
            continue
        out.append(p)
    return out


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//.*?$", "", text, flags=re.MULTILINE)
    return text


def extract_connectblock(text: str) -> str:
    start = text.find("bool Chainstate::ConnectBlock(")
    if start < 0:
        return ""
    end = text.find("\nbool Chainstate::FlushStateToDisk(", start)
    if end < 0:
        end = text.find("\nCoinsCacheSizeState Chainstate::GetCoinsCacheSizeState", start)
    return text[start:end]


def classify_parser_triple(text: str) -> bool:
    return (
        "IsWitnessProgram" in text
        and "ParseResMintScript" in text
        and "ParseConvergeScript" in text
    )


def main() -> int:
    errors: list[str] = []
    files = prod_files()

    print("Domain                Authority")
    for name, (_pat, _path, domain) in AUTHORITY.items():
        print(f"{domain:<20}  {name}")

    for name, (pat, expected, _domain) in AUTHORITY.items():
        hits = [f for f in files if pat.search(f.read_text(encoding="utf-8", errors="replace"))]
        if expected not in hits:
            errors.append(f"{name} body missing at {expected.relative_to(ROOT)}")
        extras = [f for f in hits if f != expected]
        if extras:
            print("DUPLICATE_CONSENSUS_AUTHORITY_FOUND")
            errors.append(
                f"{name} second body in "
                + ", ".join(str(x.relative_to(ROOT)) for x in extras)
            )

    for f in files:
        body = strip_comments(f.read_text(encoding="utf-8", errors="replace"))
        if f.resolve() != ISSUANCE.resolve():
            for m in FORMULA.finditer(body):
                errors.append(
                    f"issuance formula fragment {m.group(0)!r} in {f.relative_to(ROOT)}"
                )
        if f.resolve() != SUPPLY.resolve() and CIRCULATING_ASSIGN.search(body):
            errors.append(f"circulating delta assignment in {f.relative_to(ROOT)}")

    validation = VALIDATION.read_text(encoding="utf-8", errors="replace")
    cb = extract_connectblock(validation)
    if not cb:
        errors.append("ConnectBlock not found")
    else:
        cb_stripped = strip_comments(cb)
        print("CONNECTBLOCK_DRIFT_AUDIT")
        for banned, label in (
            (FORMULA, "subsidy calculation"),
            (re.compile(r"\bResRewardForEpoch\s*\("), "RES calculation"),
            (re.compile(r"\bGetBlockSubsidy\s*\("), "subsidy calculation"),
            (re.compile(r"\bCheckResMintArithmetic\s*\("), "RES calculation"),
            (re.compile(r"\bMul256x256Full\s*\("), "draw resolution"),
            (re.compile(r"\boru_circulating_delta\s*="), "supply calculation"),
            (re.compile(r"\bParseResMintScript\s*\("), "predicate parsing"),
            (re.compile(r"\bParseConvergeScript\s*\("), "predicate parsing"),
            (re.compile(r"\bIsWitnessProgram\s*\("), "predicate parsing"),
        ):
            if banned.search(cb_stripped):
                if label == "supply calculation" and "supply_delta.oru_circulating_delta" in cb_stripped:
                    if re.search(r"oru_circulating_delta\s*=(?!\s*supply_delta)", cb_stripped):
                        errors.append(f"ConnectBlock {label}")
                    continue
                errors.append(f"ConnectBlock {label}")
                print("CONNECTBLOCK_CONSENSUS_DUPLICATION_FOUND")
        if "RigorConsensusKernel::EvaluateConnect" not in cb:
            errors.append("ConnectBlock does not call RigorConsensusKernel::EvaluateConnect")
        if re.search(r"\bResolveConvergenceDraw\s*\(", cb):
            errors.append("ConnectBlock bypasses Kernel draw")
        if re.search(r"\bRigorSupplyDelta\s*\(", cb):
            errors.append("ConnectBlock bypasses Kernel supply")
        if "SelectWinner" in cb_stripped or "RunSelection" in cb_stripped:
            errors.append("ConnectBlock draw bypass")
            print("CONNECTBLOCK_CONSENSUS_DUPLICATION_FOUND")
        if EVAL_CALL.search(cb_stripped) or VERIFY_CALL.search(cb_stripped):
            errors.append("ConnectBlock calls EvalScript/VerifyScript")
            print("CONNECTBLOCK_CONSENSUS_DUPLICATION_FOUND")

    if "VerifyRigorPredicate" not in validation:
        errors.append("validation.cpp does not call VerifyRigorPredicate")

    classify = CLASSIFY.read_text(encoding="utf-8", errors="replace")
    if "return AssetTypeFromScriptPubKey(scriptPubKey)" not in classify:
        errors.append("ClassifyRigorAsset is not a wrapper of AssetTypeFromScriptPubKey")
    if "switch (ClassifyRigorTxout(scriptPubKey, sols))" not in classify:
        errors.append("AssetTypeFromScriptPubKey does not call ClassifyRigorTxout")
    solver = STANDARD.read_text(encoding="utf-8", errors="replace")
    if "return ClassifyRigorTxout(scriptPubKey, vSolutionsRet)" not in solver:
        errors.append("Solver is not a wrapper of ClassifyRigorTxout")

    parser_copies = []
    for f in files:
        if f.resolve() == CLASSIFY.resolve():
            continue
        text = f.read_text(encoding="utf-8", errors="replace")
        if classify_parser_triple(text):
            parser_copies.append(f)
    if parser_copies:
        print("DUPLICATE_CONSENSUS_AUTHORITY_FOUND")
        errors.append(
            "scriptPubKey/asset matcher copy in "
            + ", ".join(str(x.relative_to(ROOT)) for x in parser_copies)
        )

    for f in files:
        text = f.read_text(encoding="utf-8", errors="replace")
        stripped = strip_comments(text)
        if EVAL_DEF.search(stripped) or VERIFY_DEF.search(stripped):
            errors.append(f"EvalScript/VerifyScript definition in {f.relative_to(ROOT)}")
        if f.parts[-2:] in {
            ("script", "predicate_verify.cpp"),
            ("consensus", "convergence_logic.cpp"),
            ("consensus", "rigor_issuance.cpp"),
            ("consensus", "rigor_supply.cpp"),
            ("script", "rigor_classify.cpp"),
        } or f.name == "validation.cpp":
            if EVAL_CALL.search(stripped) or VERIFY_CALL.search(stripped):
                errors.append(f"EvalScript/VerifyScript call in {f.relative_to(ROOT)}")

    mul_hits = [
        f
        for f in files
        if WINNER_LOOP.search(f.read_text(encoding="utf-8", errors="replace"))
        and f.name not in {"rigor_math.h"}
        and f.resolve() != DRAW.resolve()
    ]
    if mul_hits:
        print("DUPLICATE_CONSENSUS_AUTHORITY_FOUND")
        errors.append(
            "Mul256x256Full winner-loop copy in "
            + ", ".join(str(x.relative_to(ROOT)) for x in mul_hits)
        )

    for f in files:
        if f.resolve() not in KERNEL_PURE:
            continue
        stripped = strip_comments(f.read_text(encoding="utf-8", errors="replace"))
        if CHAINSTATE.search(stripped):
            errors.append(f"kernel purity: chainstate/UTXO/DB in {f.relative_to(ROOT)}")

    makefile = MAKEFILE.read_text(encoding="utf-8", errors="replace")
    if "caml_" in makefile or "consensus_kernel.ml" in makefile:
        errors.append("Makefile.am links extracted OCaml")
    if "consensus/shadow" in makefile:
        errors.append("shadow compiled into production")
    if "consensus/txkernel" in makefile:
        errors.append("txkernel compiled into production")
    if "consensus/kernel_migration" in makefile:
        errors.append("kernel_migration compiled into production")
    if "consensus/kernel_canary" in makefile:
        errors.append("kernel_canary compiled into production")

    if errors:
        print("FAIL: consensus authority security lock")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("AUTHORITY_LOCK=PASS")
    print("CONNECTBLOCK_DRIFT_AUDIT=PASS")
    print("DUPLICATE_RULE_DETECTION=PASS")
    print("PRODUCTION_CONSENSUS_MODIFIED=NO")
    return 0


if __name__ == "__main__":
    sys.exit(main())
