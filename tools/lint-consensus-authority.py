#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P060: unique production Kernel authorities.

Does not rewrite formulas. STOP on a second production body or a
ConnectBlock bypass. Historical SelectWinner in selection.cpp is not
ConnectBlock authority and must not be called from validation.cpp.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from refinement_status import print_status  # noqa: E402
SRC = ROOT / "core/src"
MAKEFILE = SRC / "Makefile.am"
VALIDATION = SRC / "validation.cpp"
CLASSIFY = SRC / "script/rigor_classify.cpp"
STANDARD = SRC / "script/standard.cpp"
ISSUANCE = SRC / "consensus/rigor_issuance.cpp"
SUPPLY = SRC / "consensus/rigor_supply.cpp"
DRAW = SRC / "consensus/convergence_logic.cpp"
SELECTION = SRC / "consensus/selection.cpp"

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

BODY = {
    "GetBlockSubsidy": (
        re.compile(r"^CAmount GetBlockSubsidy\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        ISSUANCE,
    ),
    "GetResMintReward": (
        re.compile(r"inline constexpr CAmount GetResMintReward\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        SRC / "consensus/rigor_consensus_lock.h",
    ),
    "ComputeRigorIssuance": (
        re.compile(r"^RigorIssuance ComputeRigorIssuance\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        ISSUANCE,
    ),
    "RigorSupplyDelta": (
        re.compile(r"^bool RigorSupplyDelta\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        SUPPLY,
    ),
    "ResolveConvergenceDraw": (
        re.compile(r"^ConvergenceDrawOutcome ResolveConvergenceDraw\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        DRAW,
    ),
    "ClassifyRigorTxout": (
        re.compile(r"^TxoutType ClassifyRigorTxout\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        CLASSIFY,
    ),
    "ClassifyRigorAsset": (
        re.compile(r"^AssetType ClassifyRigorAsset\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        CLASSIFY,
    ),
    "VerifyRigorPredicate": (
        re.compile(r"^bool VerifyRigorPredicate\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        SRC / "script/predicate_verify.cpp",
    ),
}

FORMULA = re.compile(
    r"nHeight\s*/\s*nSubsidyHalvingInterval|"
    r"base\s*>>\s*halvings"
)
CIRCULATING_ASSIGN = re.compile(r"oru_circulating_delta\s*=")
EVAL_DEF = re.compile(r"\bbool\s+EvalScript\s*\(")
VERIFY_DEF = re.compile(r"\bbool\s+VerifyScript\s*\(")
EVAL_CALL = re.compile(r"\bEvalScript\s*\(")
VERIFY_CALL = re.compile(r"\bVerifyScript\s*\(")
WINNER_LOOP = re.compile(r"Mul256x256Full")
SELECT_WINNER_BODY = re.compile(r"^int SelectWinner\s*\([^;{]*\{", re.MULTILINE | re.DOTALL)


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
    table = (
        ("ORU issuance", "GetBlockSubsidy"),
        ("RES schedule", "GetResMintReward"),
        ("issuance pack", "ComputeRigorIssuance"),
        ("supply", "RigorSupplyDelta"),
        ("draw", "ResolveConvergenceDraw"),
        ("classify txout", "ClassifyRigorTxout"),
        ("classify asset", "ClassifyRigorAsset"),
        ("spend", "VerifyRigorPredicate"),
    )
    for domain, auth in table:
        print(f"{domain:<20}  {auth}")

    for name, (pat, expected) in BODY.items():
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
        for m in FORMULA.finditer(cb_stripped):
            errors.append(f"ConnectBlock formula fragment: {m.group(0)}")
        if "RigorConsensusKernel::ResolveDraw" not in cb:
            errors.append("ConnectBlock does not call RigorConsensusKernel::ResolveDraw")
        if "RigorConsensusKernel::EvaluateConnect" not in cb:
            errors.append("ConnectBlock does not call RigorConsensusKernel::EvaluateConnect")
        if re.search(r"\bResolveConvergenceDraw\s*\(", cb):
            errors.append("ConnectBlock bypasses Kernel draw")
        if re.search(r"\bRigorSupplyDelta\s*\(", cb):
            errors.append("ConnectBlock bypasses Kernel supply")
        if re.search(r"\bConnectEvaluateCoinbaseResMint\s*\(", cb):
            errors.append("ConnectBlock bypasses Kernel mint")
        if "SelectWinner" in cb_stripped:
            errors.append("ConnectBlock calls SelectWinner (draw bypass)")
        if "RunSelection" in cb_stripped:
            errors.append("ConnectBlock calls RunSelection (draw bypass)")
        if EVAL_CALL.search(cb_stripped) or VERIFY_CALL.search(cb_stripped):
            errors.append("ConnectBlock calls EvalScript/VerifyScript")
        if re.search(r"\boru_circulating_delta\s*=", cb_stripped) and "supply_delta.oru_circulating_delta" not in cb_stripped:
            errors.append("ConnectBlock recomposes circulating delta")

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

    sel_text = SELECTION.read_text(encoding="utf-8", errors="replace")
    if not SELECT_WINNER_BODY.search(sel_text):
        errors.append("historical SelectWinner missing from selection.cpp (unexpected)")
    if "SelectWinner" in validation:
        errors.append("validation.cpp references SelectWinner")

    makefile = MAKEFILE.read_text(encoding="utf-8", errors="replace")
    if "caml_" in makefile or "consensus_kernel.ml" in makefile:
        errors.append("Makefile.am links extracted OCaml")
    if "consensus/shadow" in makefile:
        errors.append("shadow compiled into production")
    if "consensus/txkernel" in makefile:
        errors.append("txkernel compiled into production")
    if "consensus/utxo" in makefile:
        errors.append("utxo compiled into production")
    if "consensus/ledger" in makefile:
        errors.append("ledger compiled into production")
    if "consensus/kernel_migration" in makefile:
        errors.append("kernel_migration compiled into production")
    if "consensus/kernel_canary" in makefile:
        errors.append("kernel_canary compiled into production")

    if errors:
        print("FAIL: consensus authority lock")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("AUTHORITY_LOCK=PASS")
    print("DUPLICATE_RULE_DETECTION=PASS")
    print("PRODUCTION_CONSENSUS_MODIFIED=NO")
    print_status()
    return 0


if __name__ == "__main__":
    sys.exit(main())
