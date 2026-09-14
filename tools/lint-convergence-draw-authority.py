#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P079: unique Convergence Draw authority lint.

ResolveConvergenceDraw is the only production winner selector.
GetConvergenceDrawSetForBlock assembles F; it does not pick a winner.
SelectWinner in selection.cpp is historical and must not be called from
validation / miner / wallet / rpc.

Does not rewrite the draw formula.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "core/src"
DRAW = SRC / "consensus/convergence_logic.cpp"
DRAW_H = SRC / "consensus/convergence_logic.h"
VALIDATION = SRC / "validation.cpp"
KERNEL = SRC / "consensus/kernel/genesis_authority.cpp"
SELECTION = SRC / "consensus/selection.cpp"
MINER = SRC / "node/miner.cpp"
WALLET = SRC / "wallet"
RPC = SRC / "rpc"
DOC = ROOT / "docs/security/RIGOR_CONVERGENCE_DRAW_SECURITY_AUDIT_P0_079.md"
TEST = SRC / "test/test_convergence_draw_security.cpp"
GATE = ROOT / "tools/run-convergence-draw-security-audit.sh"
FUZZ = ROOT / "fuzz/convergence_draw/convergence_draw_fuzz.cpp"
MANIFEST = ROOT / "formal_verification/convergence_draw_audit_manifest.json"
MAKEFILE_TEST = SRC / "Makefile.test.include"
REFINEMENT = ROOT / "formal_verification/coq/ConsensusKernelRefinement.v"
BRIDGE = ROOT / "formal_verification/coq/DrawResolverBridge.v"

SKIP_DIR_PARTS = {
    "test",
    "bench",
    "fuzz",
    "qt",
    "ipc",
    "univalue",
    "leveldb",
    "secp256k1",
    "minisketch",
    "crc32c",
}

RESOLVE_DEF = re.compile(
    r"^ConvergenceDrawOutcome ResolveConvergenceDraw\s*\([^;{]*\{",
    re.MULTILINE | re.DOTALL,
)
SELECT_DEF = re.compile(r"^int SelectWinner\s*\([^;{]*\{", re.MULTILINE | re.DOTALL)
ASSUME_RE = re.compile(r"^\s*(Admitted|Axiom|Parameter)\s")


def fail(errors: list[str]) -> int:
    print("DRAW_AUTHORITY_COUNT = FAIL")
    print("FAIL: convergence draw authority lint")
    for e in errors:
        print(f"  - {e}")
    return 1


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//.*?$", "", text, flags=re.MULTILINE)
    return text


def extract_function(text: str, signature: str) -> str | None:
    idx = text.find(signature)
    if idx < 0:
        return None
    brace = text.find("{", idx)
    if brace < 0:
        return None
    depth = 0
    for i in range(brace, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[idx : i + 1]
    return None


def iter_prod(root: Path) -> list[Path]:
    out: list[Path] = []
    if not root.is_dir():
        return out
    for p in root.rglob("*"):
        if not p.is_file() or p.suffix not in {".h", ".cpp"}:
            continue
        rel = p.relative_to(SRC).as_posix()
        parts = set(rel.split("/"))
        if parts & SKIP_DIR_PARTS:
            continue
        if "/test/" in f"/{rel}/":
            continue
        out.append(p)
    return out


def coq_assumption_counts(path: Path) -> tuple[int, int, int]:
    admitted = axioms = parameters = 0
    if not path.is_file():
        return (0, 0, 0)
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        m = ASSUME_RE.match(line)
        if not m:
            continue
        kind = m.group(1)
        if kind == "Admitted":
            admitted += 1
        elif kind == "Axiom":
            axioms += 1
        else:
            parameters += 1
    return admitted, axioms, parameters


def main() -> int:
    errors: list[str] = []

    # RFC-019 retired the lottery. Absence of the draw TU is the live state.
    if not DRAW.is_file() and not DRAW_H.is_file():
        val = VALIDATION.read_text(encoding="utf-8", errors="replace") if VALIDATION.is_file() else ""
        if re.search(r"\bResolveConvergenceDraw\s*\(", val) or re.search(
            r"\bRigorConsensusKernel::ResolveDraw\s*\(", val
        ):
            return fail(["ConnectBlock still calls retired draw"])
        print("DRAW_AUTHORITY_COUNT = 0")
        print("DRAW_RETIRED=PASS")
        return 0

    required = [
        DRAW,
        DRAW_H,
        VALIDATION,
        KERNEL,
        SELECTION,
        MINER,
        TEST,
        DOC,
        GATE,
        FUZZ,
        MANIFEST,
        ROOT / "fuzz/convergence_draw/run_smoke.sh",
        REFINEMENT,
        BRIDGE,
    ]
    for p in required:
        if not p.is_file():
            errors.append(f"missing {p.relative_to(ROOT)}")

    draw = DRAW.read_text(encoding="utf-8", errors="replace") if DRAW.is_file() else ""
    draw_h = DRAW_H.read_text(encoding="utf-8", errors="replace") if DRAW_H.is_file() else ""
    if "ConvergenceDrawOutcome ResolveConvergenceDraw(" not in draw:
        errors.append("ResolveConvergenceDraw body missing")
    if "ConvergenceDrawOutcome ResolveConvergenceDraw(" not in draw_h:
        errors.append("ResolveConvergenceDraw declaration missing")

    cpp_defs: list[str] = []
    for p in iter_prod(SRC):
        if p.suffix != ".cpp":
            continue
        text = p.read_text(encoding="utf-8", errors="replace")
        if RESOLVE_DEF.search(text):
            cpp_defs.append(p.relative_to(ROOT).as_posix())
    if cpp_defs != ["core/src/consensus/convergence_logic.cpp"]:
        errors.append(f"ResolveConvergenceDraw cpp definitions: {cpp_defs}")

    authority_count = 1 if cpp_defs == ["core/src/consensus/convergence_logic.cpp"] else len(cpp_defs)

    kernel = KERNEL.read_text(encoding="utf-8", errors="replace") if KERNEL.is_file() else ""
    kernel_fn = extract_function(kernel, "RigorConsensusKernel::ResolveDraw")
    if kernel_fn:
        compact = re.sub(r"\s+", "", kernel_fn)
        if "returnResolveConvergenceDraw(F,T);" not in compact:
            errors.append("ResolveDraw is not a forward to ResolveConvergenceDraw")
        if "SelectWinner" in kernel_fn:
            errors.append("ResolveDraw calls SelectWinner")

    val = VALIDATION.read_text(encoding="utf-8", errors="replace") if VALIDATION.is_file() else ""
    assembly = extract_function(val, "bool GetConvergenceDrawSetForBlock")
    if assembly:
        asm_code = strip_comments(assembly)
        if re.search(r"\bResolveConvergenceDraw\s*\(", asm_code):
            errors.append("GetConvergenceDrawSetForBlock selects a winner (ResolveConvergenceDraw)")
        if re.search(r"\bRigorConsensusKernel::ResolveDraw\s*\(", asm_code):
            errors.append("GetConvergenceDrawSetForBlock selects a winner (ResolveDraw)")
        if "SelectWinner" in asm_code or "RunSelection" in asm_code:
            errors.append("GetConvergenceDrawSetForBlock calls SelectWinner")
        if "winner_index" in asm_code:
            errors.append("GetConvergenceDrawSetForBlock writes winner_index")

    val_code = strip_comments(val)
    if "SelectWinner" in val_code or "RunSelection" in val_code:
        errors.append("validation.cpp names SelectWinner")
    if re.search(r"\bRigorConsensusKernel::ResolveDraw\s*\(", val_code):
        errors.append("ConnectBlock still calls retired ResolveDraw")

    if SELECTION.is_file():
        sel = SELECTION.read_text(encoding="utf-8", errors="replace")
        if not SELECT_DEF.search(sel):
            errors.append("historical SelectWinner missing from selection.cpp (unexpected)")

    def forbid_select(path: Path, label: str) -> None:
        if not path.is_file():
            errors.append(f"missing {label}")
            return
        code = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
        if re.search(r"\bSelectWinner\s*\(", code) or re.search(r"\bRunSelection\s*\(", code):
            errors.append(f"{label} calls SelectWinner legacy path")

    forbid_select(MINER, "node/miner.cpp")
    miner = MINER.read_text(encoding="utf-8", errors="replace") if MINER.is_file() else ""
    miner_code = strip_comments(miner)
    if not re.search(r"\bResolveConvergenceDraw\s*\(", miner_code):
        errors.append("miner.cpp does not call ResolveConvergenceDraw (local selection risk)")

    for p in iter_prod(WALLET):
        code = strip_comments(p.read_text(encoding="utf-8", errors="replace"))
        rel = p.relative_to(ROOT).as_posix()
        if re.search(r"\bResolveConvergenceDraw\s*\(", code):
            errors.append(f"wallet selects a winner in {rel}")
        if re.search(r"\bSelectWinner\s*\(", code) or re.search(r"\bRunSelection\s*\(", code):
            errors.append(f"wallet SelectWinner in {rel}")

    for p in iter_prod(RPC):
        code = strip_comments(p.read_text(encoding="utf-8", errors="replace"))
        rel = p.relative_to(ROOT).as_posix()
        if re.search(r"\bSelectWinner\s*\(", code) or re.search(r"\bRunSelection\s*\(", code):
            errors.append(f"RPC SelectWinner in {rel}")
        # RPC may *call* ResolveConvergenceDraw (introspection / GBT). It must not define it.
        if RESOLVE_DEF.search(p.read_text(encoding="utf-8", errors="replace")):
            errors.append(f"RPC defines ResolveConvergenceDraw in {rel}")

    if MAKEFILE_TEST.is_file():
        inc = MAKEFILE_TEST.read_text(encoding="utf-8", errors="replace")
        if "test_convergence_draw_security.cpp" not in inc:
            errors.append("test_convergence_draw_security.cpp not in Makefile.test.include")

    coq_admitted = coq_axioms = coq_parameters = 0
    for path in (REFINEMENT, BRIDGE):
        a, x, p = coq_assumption_counts(path)
        coq_admitted += a
        coq_axioms += x
        coq_parameters += p
    if coq_admitted or coq_axioms or coq_parameters:
        errors.append(
            f"Coq assumptions drifted: ADMITTED={coq_admitted} AXIOMS={coq_axioms} PARAMETERS={coq_parameters}"
        )
    ref = REFINEMENT.read_text(encoding="utf-8", errors="replace") if REFINEMENT.is_file() else ""
    if "Theorem draw_refinement" not in ref:
        errors.append("existing Draw refinement theorem missing")

    doc = DOC.read_text(encoding="utf-8", errors="replace") if DOC.is_file() else ""
    for req in (
        "single draw authority",
        "deterministic winner",
        "overflow safety",
        "tie-break safety",
        "candidate integrity",
        "adversarial resistance",
        "true randomness proof",
        "economic game theory equilibrium",
        "miner behavior prediction",
        "DRAW_AUTHORITY_COUNT = 1",
        "RIGOR_CONVERGENCE_DRAW_SECURITY_AUDIT_CLOSED",
    ):
        if req not in doc:
            errors.append(f"formal note missing: {req}")

    man = MANIFEST.read_text(encoding="utf-8", errors="replace") if MANIFEST.is_file() else ""
    for req in (
        '"draw_authority_count": 1',
        '"unique_draw_authority": "ResolveConvergenceDraw"',
        '"consensus_modified": false',
        '"new_theorem_added": false',
        '"coq_admitted": 0',
        '"coq_axioms": 0',
        '"coq_parameters": 0',
        '"draw_determinism": "PASS"',
        '"draw_tiebreak": "PASS"',
        '"draw_weight_security": "PASS"',
        "RIGOR_CONVERGENCE_DRAW_SECURITY_AUDIT_CLOSED",
    ):
        if req not in man:
            errors.append(f"manifest missing {req}")

    if errors:
        return fail(errors)

    print("DRAW_AUTHORITY_COUNT = 1")
    print("UNIQUE_DRAW_AUTHORITY = ResolveConvergenceDraw")
    print("ASSEMBLY = GetConvergenceDrawSetForBlock (candidates only)")
    print("SELECT_WINNER_LEGACY_PATH = NOT_CALLED")
    print("MINER_LOCAL_SELECTION = NONE")
    print("WALLET_SELECTION = NONE")
    print("RPC_SELECTION = NONE")
    print("COQ_ADMITTED = 0")
    print("COQ_AXIOMS = 0")
    print("COQ_PARAMETERS = 0")
    print("CONSENSUS_MODIFIED = NO")
    print("PASS: ResolveConvergenceDraw is the unique draw authority")
    print(f"authority_count_check = {authority_count}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
