#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""F-4/F-5: extracted MakeKernelInput is test-only; CompCert/VST deferred
and scoped to MakeKernelInput only."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "core/src/consensus/kernel/genesis_authority.h"
COQ = ROOT / "formal_verification/coq/MakeKernelInput.v"
EXTRACT = ROOT / "formal_verification/coq/ExtractMakeKernelInput.v"
PROJECT = ROOT / "formal_verification/coq/_CoqProject"
MAKEFILE = ROOT / "core/src/Makefile.am"
GATE = ROOT / "formal_verification/run_transaction_kernel_refinement.sh"
EXTRACT_GITIGNORE = ROOT / "formal_verification/extracted/.gitignore"


def main() -> int:
    errors: list[str] = []
    header = HEADER.read_text(encoding="utf-8", errors="replace")
    if "inline constexpr bool EXTRACTED_KERNEL_PRODUCTION = false" not in header:
        errors.append("EXTRACTED_KERNEL_PRODUCTION must stay false")
    if "inline constexpr bool COMPCERT_BINARY_VERIFIED_KERNEL = false" not in header:
        errors.append("COMPCERT_BINARY_VERIFIED_KERNEL must stay false")
    if "inline constexpr bool COMPCERT_VST_DEFERRED = true" not in header:
        errors.append("COMPCERT_VST_DEFERRED must be true")
    if 'COMPCERT_VST_CANDIDATE[] = "MakeKernelInput"' not in header:
        errors.append('COMPCERT_VST_CANDIDATE must be "MakeKernelInput"')

    coq = COQ.read_text(encoding="utf-8", errors="replace")
    for needle in (
        "Definition compcert_vst_deferred : bool := true",
        "Definition vst_scope_makekernelinput : bool := true",
        "Definition vst_scope_full_kernel : bool := false",
        "Definition extracted_kernel_production : bool := false",
    ):
        if needle not in coq:
            errors.append(f"MakeKernelInput.v missing {needle}")

    ext = EXTRACT.read_text(encoding="utf-8", errors="replace")
    if re.search(r"^\s*Extract Constant", ext, re.M):
        errors.append("ExtractMakeKernelInput.v uses Extract Constant")
    if "make_crypto_accept" not in ext or "raw_from_domain" not in ext:
        errors.append("extraction must name make_crypto_accept and raw_from_domain")
    if "all_raw" not in ext or "domain_crypto_accept" not in ext:
        errors.append("extraction must name all_raw and domain_crypto_accept")
    if "NOT listed in _CoqProject" not in ext:
        errors.append("extraction file must state it is not in _CoqProject")

    project = PROJECT.read_text(encoding="utf-8", errors="replace")
    if "ExtractMakeKernelInput.v" in project:
        errors.append("ExtractMakeKernelInput.v must not be in _CoqProject")

    makefile = MAKEFILE.read_text(encoding="utf-8", errors="replace")
    for banned in (
        "caml_",
        "consensus_kernel.ml",
        "makekernelinput_oracle.ml",
        "libasmrun",
        "ExtractMakeKernelInput",
        "ocamlopt",
    ):
        if banned in makefile:
            errors.append(f"production Makefile.am mentions {banned}")

    ignore = EXTRACT_GITIGNORE.read_text(encoding="utf-8", errors="replace")
    if "makekernelinput_oracle.ml" not in ignore:
        errors.append("extracted/.gitignore must ignore generated MakeKernelInput oracle")

    gate = GATE.read_text(encoding="utf-8", errors="replace")
    if "extract_makekernelinput_oracle.sh" not in gate:
        errors.append("transaction kernel gate does not run the extracted test oracle")
    if "EXTRACTED_KERNEL_TEST_ORACLE=PASS" not in gate:
        errors.append("gate does not print EXTRACTED_KERNEL_TEST_ORACLE=PASS")
    if "COMPCERT_VST_DEFERRED=TRUE" not in gate:
        errors.append("gate does not print COMPCERT_VST_DEFERRED=TRUE")
    if "COMPCERT_VST_CANDIDATE=MakeKernelInput" not in gate:
        errors.append("gate does not print COMPCERT_VST_CANDIDATE=MakeKernelInput")

    cli = ROOT / "formal_verification/tools/makekernelinput_oracle_cli.ml"
    cli_text = cli.read_text(encoding="utf-8", errors="replace")
    if "K.all_raw" not in cli_text or "K.domain_crypto_accept" not in cli_text:
        errors.append("oracle CLI does not walk extracted all_raw")
    if "K.tag_mismatch_pins" not in cli_text:
        errors.append("oracle CLI does not walk tag_mismatch_pins")
    if "tag_mismatch_pins" not in ext:
        errors.append("extraction must name tag_mismatch_pins")

    workflow = ROOT / ".github/workflows/consensus-kernel-integrity.yml"
    wf = workflow.read_text(encoding="utf-8", errors="replace")
    if "run_transaction_kernel_refinement.sh" not in wf:
        errors.append("CI does not run the P058 transaction kernel gate")

    if errors:
        print("FAIL: extracted-oracle / CompCert-VST scope")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("EXTRACTED_KERNEL_PRODUCTION=FALSE")
    print("EXTRACTED_KERNEL_TEST_ORACLE=REQUIRED")
    print("COMPCERT_VST_DEFERRED=TRUE")
    print("COMPCERT_VST_CANDIDATE=MakeKernelInput")
    print("COMPCERT_BINARY_VERIFIED_KERNEL=FALSE")
    return 0


if __name__ == "__main__":
    sys.exit(main())
