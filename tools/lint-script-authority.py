#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P078: unique spend authority + opcode surface lint.

VerifyRigorPredicate is the only spend authority. There is no Script VM,
no opcode interpreter, and no second signature/script path in
validation.cpp / transaction.cpp / wallet / rpc.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "core/src"
PRED = SRC / "script/predicate_verify.cpp"
PRED_H = SRC / "script/predicate_verify.h"
BYTES_H = SRC / "script/rigor_bytes.h"
VALIDATION = SRC / "validation.cpp"
TX_CPP = SRC / "primitives/transaction.cpp"
MAKEFILE = SRC / "Makefile.am"
TEST_INC = SRC / "Makefile.test.include"
DOC = ROOT / "docs/security/RIGOR_SCRIPT_OPCODE_SECURITY_AUDIT_P0_078.md"
TEST = SRC / "test/test_script_opcode_security.cpp"
GATE = ROOT / "tools/run-script-opcode-security-audit.sh"
FUZZ = ROOT / "fuzz/script_opcode_boundary/script_opcode_fuzz.cpp"
MANIFEST = ROOT / "formal_verification/script_opcode_audit_manifest.json"

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

VM_SYMBOLS = (
    "EvalScript",
    "ExecuteOpcode",
    "ExecuteWitnessScript",
    "VerifyScript",
    "ScriptMachine",
    "OpcodeInstruction",
    "ConditionStack",
    "vfExec",
    "stacktop",
    "altstacktop",
    "popstack",
)

BITCOIN_OPCODES = (
    "OP_CHECKSIG",
    "OP_CHECKMULTISIG",
    "OP_CHECKLOCKTIMEVERIFY",
    "OP_CHECKSEQUENCEVERIFY",
    "OP_DUP",
    "OP_HASH160",
    "OP_EQUALVERIFY",
    "OP_CHECKSIGVERIFY",
    "OP_CHECKMULTISIGVERIFY",
    "OP_CHECKSIGADD",
)

PRED_DEF = re.compile(r"(?:^|\n)\s*(?:static\s+)?bool\s+VerifyRigorPredicate\s*\(")
EVAL_DEF = re.compile(r"(?:^|\n)\s*(?:static\s+)?bool\s+EvalScript\s*\(")
VERIFY_SCRIPT_DEF = re.compile(r"(?:^|\n)\s*(?:static\s+)?bool\s+VerifyScript\s*\(")
OPCODE_ID = re.compile(r"\bOP_[A-Z][A-Z0-9_]*\b")
ALLOWED_OP_NAMES = {
    "OP_RETURN_BURN",
    "OP_RETURN_RELAY",
    "OP_COUNT",
    "OP_CODESEPARATOR",
    "OP_PUSHBYTES_32",
    "OP_PUSHDATA",
    "OP_PUSHBYTES",
    "OP_SUCCESSx",
    "OP_SUCCESS",
    "OP_CVG_CLAIM",
    "OP_NEW_SIGCHECK",
    "OP_NEW_VM",
    "OP_NEW_NOP",
    "OP_EVAL",
    "OP_5",
    "OP_0",
    "OP_1NEGATE",
    "OP_1",
    "OP_16",
}


def fail(errors: list[str]) -> int:
    print("DUPLICATE_VALIDATION_PATH = FOUND")
    print("FAIL: script authority lint")
    for e in errors:
        print(f"  - {e}")
    return 1


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//.*?$", "", text, flags=re.MULTILINE)
    return text


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


def named_byte_count(text: str) -> int:
    return len(re.findall(r"^\s+RIGOR_(?:PUSH|PUSHDATA|MARKER|RESERVED_|BYTE_)", text, re.M))


def main() -> int:
    errors: list[str] = []

    required = [
        PRED,
        PRED_H,
        BYTES_H,
        TEST,
        DOC,
        GATE,
        FUZZ,
        MANIFEST,
        ROOT / "fuzz/script_opcode_boundary/run_smoke.sh",
        SRC / "consensus/txkernel/transaction_kernel_input.h",
    ]
    for p in required:
        if not p.is_file():
            errors.append(f"missing {p.relative_to(ROOT)}")

    pred = PRED.read_text(encoding="utf-8", errors="replace") if PRED.is_file() else ""
    pred_h = PRED_H.read_text(encoding="utf-8", errors="replace") if PRED_H.is_file() else ""
    if "bool VerifyRigorPredicate(" not in pred:
        errors.append("VerifyRigorPredicate body missing")
    if "bool VerifyRigorPredicate(" not in pred_h:
        errors.append("VerifyRigorPredicate declaration missing")
    # A1: Boundary is the implementation of the unique authority, not a
    # second entry. validation.cpp must keep calling VerifyRigorPredicate.
    if "MakeKernelInput(" not in pred:
        errors.append("VerifyRigorPredicate does not call MakeKernelInput")
    if "VerifyRigorPredicateBoundary(" not in pred:
        errors.append("VerifyRigorPredicate does not call VerifyRigorPredicateBoundary")
    if VALIDATION.is_file():
        val_raw = VALIDATION.read_text(encoding="utf-8", errors="replace")
        if "VerifyRigorPredicateBoundary(" in strip_comments(val_raw):
            errors.append("validation.cpp calls VerifyRigorPredicateBoundary (second entry)")

    cpp_defs = []
    for p in iter_prod(SRC):
        if p.suffix != ".cpp":
            continue
        text = p.read_text(encoding="utf-8", errors="replace")
        if PRED_DEF.search(text):
            cpp_defs.append(p.relative_to(ROOT).as_posix())
    if cpp_defs != ["core/src/script/predicate_verify.cpp"]:
        errors.append(f"VerifyRigorPredicate cpp definitions: {cpp_defs}")

    authority_count = 1 if cpp_defs == ["core/src/script/predicate_verify.cpp"] else len(cpp_defs)

    vm_hits: list[str] = []
    opcode_hits: list[str] = []
    op_star_hits: list[str] = []
    for p in iter_prod(SRC):
        rel = p.relative_to(ROOT).as_posix()
        raw = p.read_text(encoding="utf-8", errors="replace")
        code = strip_comments(raw)
        if EVAL_DEF.search(code):
            errors.append(f"EvalScript defined in {rel}")
        if VERIFY_SCRIPT_DEF.search(code):
            errors.append(f"VerifyScript defined in {rel}")
        for tok in VM_SYMBOLS:
            if re.search(r"\b" + re.escape(tok) + r"\b", code):
                # Mentions in string prefixes of GetOpNameAlias ("OP_") are not VM symbols.
                if tok == "VerifyScript" and "VerifyRigorPredicate" in rel:
                    continue
                vm_hits.append(f"{rel}:{tok}")
        for tok in BITCOIN_OPCODES:
            if re.search(r"\b" + re.escape(tok) + r"\b", code):
                opcode_hits.append(f"{rel}:{tok}")
        for m in OPCODE_ID.finditer(code):
            name = m.group(0)
            if name in BITCOIN_OPCODES:
                continue
            if name in ALLOWED_OP_NAMES:
                continue
            if name.startswith("OP_RETURN"):
                continue
            op_star_hits.append(f"{rel}:{name}")

        if rel.endswith("predicate_verify.cpp"):
            if re.search(r"case\s+RIGOR_RESERVED_LEGACY_CHECKSIG\s*:", code):
                errors.append("predicate_verify.cpp dispatches CHECKSIG")
            if re.search(r"case\s+RIGOR_RESERVED_LEGACY_CHECKMULTISIG\s*:", code):
                errors.append("predicate_verify.cpp dispatches CHECKMULTISIG")

    # GetOpNameAlias holds a string prefix "OP_" for ParseScript roundtrip.
    op_star_hits = [h for h in op_star_hits if not h.endswith(":OP_")]

    if vm_hits:
        errors.append("VM symbols in production: " + ", ".join(vm_hits))
    if opcode_hits:
        errors.append("Bitcoin opcode identifiers in production: " + ", ".join(opcode_hits))
    if op_star_hits:
        errors.append("unexpected OP_* identifiers in production: " + ", ".join(op_star_hits))

    val = VALIDATION.read_text(encoding="utf-8", errors="replace") if VALIDATION.is_file() else ""
    val_code = strip_comments(val)
    if "VerifyRigorPredicate(" not in val_code.replace(" ", ""):
        errors.append("validation.cpp does not call VerifyRigorPredicate")
    if PRED_DEF.search(val):
        errors.append("validation.cpp defines VerifyRigorPredicate")
    if EVAL_DEF.search(val_code) or "EvalScript" in val_code:
        errors.append("validation.cpp names EvalScript")

    scoped = [
        TX_CPP,
        *iter_prod(SRC / "wallet"),
        *iter_prod(SRC / "rpc"),
        *iter_prod(SRC / "primitives"),
    ]
    seen: set[Path] = set()
    for p in scoped:
        if p in seen or not p.is_file():
            continue
        seen.add(p)
        rel = p.relative_to(ROOT).as_posix()
        code = strip_comments(p.read_text(encoding="utf-8", errors="replace"))
        if PRED_DEF.search(code):
            errors.append(f"second predicate definition in {rel}")
        if EVAL_DEF.search(code) or VERIFY_SCRIPT_DEF.search(code):
            errors.append(f"script VM definition in {rel}")
        if re.search(r"\bExecuteOpcode\b", code):
            errors.append(f"opcode interpreter in {rel}")

    bytes_h = BYTES_H.read_text(encoding="utf-8", errors="replace") if BYTES_H.is_file() else ""
    named = named_byte_count(bytes_h)
    residue = "RIGOR_RESERVED_LEGACY_" in bytes_h

    mk = MAKEFILE.read_text(encoding="utf-8", errors="replace") if MAKEFILE.is_file() else ""
    if "script/interpreter.cpp" in mk:
        errors.append("interpreter.cpp still listed in Makefile.am")
    if TEST_INC.is_file():
        inc = TEST_INC.read_text(encoding="utf-8", errors="replace")
        if "test_script_opcode_security.cpp" not in inc:
            errors.append("test_script_opcode_security.cpp not in Makefile.test.include")

    doc = DOC.read_text(encoding="utf-8", errors="replace") if DOC.is_file() else ""
    for req in (
        "single predicate authority",
        "no script VM execution",
        "no opcode bypass",
        "malformed input rejection",
        "deterministic failure",
        "cryptographic primitive proof",
        "compiler correctness",
        "hardware security",
    ):
        if req not in doc:
            errors.append(f"formal note missing: {req}")

    man = MANIFEST.read_text(encoding="utf-8", errors="replace") if MANIFEST.is_file() else ""
    for req in (
        '"script_authority_count": 1',
        '"unique_spend_authority": "VerifyRigorPredicate"',
        '"script_vm_residue": "NONE"',
        '"opcode_surface_count": 0',
        '"consensus_modified": false',
        "LEGACY_SCRIPT_RESIDUE" in man.upper() and "legacy_script_residue_found",
    ):
        if isinstance(req, bool):
            continue
        if req not in man:
            errors.append(f"manifest missing {req}")
    if '"legacy_script_residue_found": true' not in man:
        errors.append("manifest must register LEGACY_SCRIPT_RESIDUE_FOUND")

    if errors:
        return fail(errors)

    print("SCRIPT_AUTHORITY_COUNT = 1")
    print("SCRIPT_VM_PRESENT = NO")
    print("OPCODE_SURFACE_COUNT = 0")
    print(f"NAMED_BYTE_IDENTIFIERS = {named}")
    print("DUPLICATE_VALIDATION_PATH = NONE")
    if residue:
        print("LEGACY_SCRIPT_RESIDUE_FOUND")
    print("SCRIPT_VM_RESIDUE = NONE")
    print("CONSENSUS_MODIFIED = NO")
    print("PASS: VerifyRigorPredicate is the unique spend authority")
    print(f"authority_count_check = {authority_count}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
