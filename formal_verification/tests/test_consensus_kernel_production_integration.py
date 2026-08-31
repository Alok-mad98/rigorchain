#!/usr/bin/env python3
"""P051: production integration audit + extended OCaml ↔ C++ differential.

Option C: extracted kernel is an oracle, not the production execution source.
Not a refinement. Python is not consensus authority.
"""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
FV = REPO / "formal_verification"
OCAML = FV / "extracted" / "consensus_kernel_cli"
CPP = FV / "extracted" / "cpp_consensus_kernel_cli"
LINT = REPO / "tools" / "lint-consensus-kernel-production-source.py"

GRAPH = FV / "production_consensus_call_graph.json"
ABI = FV / "extracted_consensus_kernel_abi.json"
BUILD = FV / "production_consensus_kernel_build.json"

# P050 heights plus the P051 required extras.
HEIGHTS = [0, 1, 2, 99, 100, 101, 209999, 210000, 210001, 419999, 420000]


def run(cli: Path, *args: str) -> str:
    r = subprocess.run([str(cli), *args], check=True, capture_output=True, text=True)
    return r.stdout.strip()


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def main() -> int:
    errors: list[str] = []

    for path in (GRAPH, ABI, BUILD):
        if not path.is_file():
            print(f"FAIL: missing {path}", file=sys.stderr)
            return 1
        doc = load(path)
        for key in ("refinement_proved", "equivalence_proved"):
            if doc.get(key) is True:
                errors.append(f"{path.name}: {key} must stay false")

    graph = load(GRAPH)
    if graph.get("production_execution_source") != "handwritten_cpp":
        errors.append("call graph must name handwritten_cpp as production source")
    if graph.get("extracted_kernel", {}).get("production_path") != "INACTIVE":
        errors.append("extracted kernel must be INACTIVE on the production path")
    if graph.get("option") != "C":
        errors.append("integration option must be C until a safe switch exists")
    if graph.get("connect_block", {}).get("genesis", {}).get("actual_oru") != 0:
        errors.append("genesis actual ORU must stay 0")
    if graph.get("connect_block", {}).get("genesis", {}).get("actual_res") != 0:
        errors.append("genesis actual RES must stay 0")

    abi = load(ABI)
    if abi.get("extract_constant"):
        errors.append("Extract Constant must remain empty")
    if abi.get("ffi_requirements", {}).get("present_in_production_build") is not False:
        errors.append("ABI must record that FFI is not in the production build")

    build = load(BUILD)
    if build.get("extracted_kernel_linked_into_production") is not False:
        errors.append("build JSON must not claim production linkage")
    if build.get("production_kernel_switch") != "NOT_READY":
        errors.append("production_kernel_switch must be NOT_READY")

    lint = subprocess.run([sys.executable, str(LINT)], capture_output=True, text=True)
    if lint.returncode != 0:
        errors.append("production source lint failed:\n" + lint.stdout + lint.stderr)
    selft = subprocess.run(
        [sys.executable, str(LINT), "--self-test"], capture_output=True, text=True
    )
    if selft.returncode != 0:
        errors.append("lint self-test failed")

    cb = (REPO / "core/src/validation.cpp").read_text(encoding="utf-8")
    if "consensus_kernel.ml" in cb or "caml_startup" in cb:
        errors.append("validation.cpp gained extracted linkage")

    if not OCAML.is_file() or not CPP.is_file():
        errors.append("build extract_consensus_kernel.sh first")
        print("FAIL: consensus kernel production integration")
        for e in errors:
            print(f"  - {e}")
        return 1

    for h in HEIGHTS:
        for cmd in ("epoch", "oru", "res-schedule", "issuance"):
            o, c = run(OCAML, cmd, str(h)), run(CPP, cmd, str(h))
            if o != c:
                errors.append(f"{cmd}({h}) ocaml={o} cpp={c}")

    if run(OCAML, "epoch", "0") != "0":
        errors.append("epoch(0)")
    if run(OCAML, "epoch", "209999") != "0":
        errors.append("epoch(209999)")
    if run(OCAML, "epoch", "210000") != "1":
        errors.append("epoch(210000)")
    if run(OCAML, "oru", "0") != "0":
        errors.append("GetBlockSubsidy(0)")
    if run(OCAML, "res-issued", "0") != "0":
        errors.append("res_issued_at_height(0)")
    if run(OCAML, "res-schedule", "0") != "40000000":
        errors.append("GetResMintReward(0) schedule")
    if run(OCAML, "res-schedule", "0") == "0":
        errors.append("FORBIDDEN: schedule at 0 collapsed to actual 0")
    if run(OCAML, "draw-empty") != "NONE":
        errors.append("empty F must be NONE")
    if run(OCAML, "maturity-const") != "100":
        errors.append("ORU/RES maturity const")
    if run(CPP, "maturity-const") != "100":
        errors.append("C++ maturity const")
    if run(OCAML, "mature", "100", "1") != "false":
        errors.append("height 1 immature at 100")
    if run(OCAML, "mature", "101", "1") != "true":
        errors.append("height 1 mature at 101")

    p2wpkh = "0014" + ("11" * 20)
    res_v1 = "5120" + ("aa" * 32)
    if run(OCAML, "classify", "-") != "INVALID":
        errors.append("empty classify")
    if run(CPP, "classify", "-") != "INVALID":
        errors.append("cpp empty classify")
    if run(OCAML, "classify", p2wpkh) != run(CPP, "classify", p2wpkh):
        errors.append("classify ORU mismatch")
    if run(OCAML, "classify", res_v1) != run(CPP, "classify", res_v1):
        errors.append("classify RES mismatch")
    if run(OCAML, "classify", "76a914" + ("00" * 20) + "88ac") != "INVALID":
        errors.append("p2pkh must fail-closed")

    sub1 = run(OCAML, "oru", "1")
    if run(OCAML, "claimable", "100", "10", sub1) != str(100 - 10 + int(sub1)):
        errors.append("claimable")
    if run(OCAML, "ledger-delta", sub1, "2") != str(int(sub1) - 2):
        errors.append("ledger delta")
    if run(OCAML, "res-coinbase", "0", "0", "40000000") != "40000000":
        errors.append("res coinbase predicate")
    if run(OCAML, "apply-delta", "0", "0", "0", "0", "0", "1", "0", "1") != "REJECT":
        errors.append("invalid apply-delta must REJECT")
    if run(OCAML, "apply-delta", "0", "0", "0", "0", "0", "1", "0", "0") != "0 0 0 0":
        errors.append("genesis apply-delta")

    if errors:
        print("FAIL: consensus kernel production integration")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("PASS: Option C documented; OCaml oracle = C++ Kernel on extended heights")
    print("PRODUCTION_KERNEL_SWITCH=NOT_READY REFINEMENT_PROVED=false")
    return 0


if __name__ == "__main__":
    sys.exit(main())
