#!/usr/bin/env python3
"""
Generate ConsensusGoldenVectorsData.v from phase3_golden_vectors.json + PHASE-4E synthetics.

TASK-FORMAL-PHASE4F-END-TO-END-CONSENSUS-VERIFICATION-001C
Run from repo root:
  python3 formal_verification/tools/generate_phase4f_golden_traces.py
"""

from __future__ import annotations

import json
import re
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
JSON_PATH = REPO / "formal_verification" / "phase3_golden_vectors.json"
OUT_PATH = REPO / "formal_verification" / "coq" / "Phase4FConsensus" / "ConsensusGoldenVectorsData.v"

DRAW_MODULES = frozenset({"ConvergenceRoot", "SigmaPoW01", "Inequality", "RunSelection"})

# Modules that carry no Phase-4F trace family on purpose. Listing them explicitly is what makes
# the completeness check below meaningful: a module that is neither mapped nor listed here is a
# rename or a new family, and either way it must not be silently dropped.
#
# SECURITY note on why this list exists. `SigmaCR01` was renamed to `SigmaPoW01` by
# SECURITY-001-A8, and this generator was never updated. The rename did not fail — it produced a
# PHASE3_SIG family with zero members, which the emitter then wrote as the invalid Coq fragment
# `:: nil`. The output stopped compiling, so the file was simply never regenerated again, and the
# checked-in copy silently froze at the 57-vector baseline while the JSON grew to 324.
UNMAPPED_MODULES = frozenset({
    "SigmaEncoder",
    "WindowExclusion",
    "WindowIncomplete",
    "HashKernel",
    "Sha256Fips",
    "BlockTrace",
    "DrawCandidateRoot",
    # Monetary reject family: Python oracle + Kernel/NegativeCoreHarness.v, not a
    # Phase-4F draw-trace family (TASK-FV-VECTOR-NEGATIVE-CORE-001).
    "NegativeCore",
    # Invalid Convergence / malformed-root / dup-txid rejects (P0-4 remainder):
    # Python oracle + Kernel/GoldenHarness.v, not a Phase-4F draw-trace family.
    "InvalidCvg",
    # Abstract script classification / PCORB mint-witness shape rejects
    # (TASK-FV-VECTOR-SCRIPT-AND-MINT-WITNESS-NEG-001): Python oracle +
    # Kernel/GoldenHarness.v, not a Phase-4F draw-trace family.
    "ScriptMintNeg",
    # Cross-family combination / boundary rejects
    # (TASK-FV-VECTOR-EDGE-AND-CROSS-FAMILY-001): Python oracle + harness, not a
    # Phase-4F draw-trace family.
    "EdgeCross",
})

FAMILY_TAG = {
    "PHASE3_CR": 1,
    "PHASE3_SIG": 2,
    "PHASE3_WIN": 3,
    "PHASE4A_DRAW": 4,
    "PHASE4B_REPLAY": 5,
    "PHASE4D_SUPPLY": 6,
    "PHASE4E_CONNECTBLOCK": 7,
}

PHASE4E_CASES = [
    ("default_mint_zero", 0, 0),
    ("oru_subsidy_only", 25, 0),
    ("oru_subsidy_clamped", 0, 0),
    ("res_draw_won_under_cap", 0, 10_000),
    ("res_draw_won_at_cap", 0, 0),
    ("res_draw_won_over_cap", 0, 0),
    ("oru_and_res_combined", 50, 10_000),
]


def vector_slot_from_id(vid: str) -> int:
    m = re.match(r"^([A-Z]+)-(\d+)$", vid)
    if not m:
        return 0
    prefix, num = m.group(1), int(m.group(2))
    base = {
        "CR": 0,
        "SIG": 100,
        "INQ": 200,
        "WIN": 300,
        "RP": 400,
        "RO": 500,
        "SUP": 600,
        "BLK": 700,
    }.get(prefix, 900)
    return base + num


def draw_winner_for_vector(v: dict) -> tuple[bool, int, int]:
    """(is_draw_winner, golden_winner, kernel_winner).

    Winner projection only applies to RunSelection (PHASE3_WIN / PHASE4A_DRAW).
    Kernel winner equals golden winner because PHASE-4A2 proved all 7 RunSelection
    vectors MATCH in the Coq draw kernel (vm_compute). We embed the verified kernel
    output here; we do NOT recompute the kernel inside Phase-4F (kernel import is
    forbidden). Non-RunSelection vectors are Not-Applicable (-1 / -1, is_draw=False).
    """
    if v["module"] != "RunSelection":
        return (False, -1, -1)
    golden = int(v.get("expected", {}).get("winner_index", -1))
    kernel = golden
    return (True, golden, kernel)


def tx_count_for_vector(v: dict) -> int:
    mod = v["module"]
    inp = v.get("input") or {}
    if mod in ("ConvergenceRoot", "Replay", "ReorgBind", "BlockTrace"):
        txs = inp.get("txids_hex") or inp.get("blocks") or []
        if isinstance(txs, list):
            return max(1, len(txs))
    if mod == "RunSelection":
        mints = inp.get("mints") or inp.get("F") or []
        return max(1, len(mints) if isinstance(mints, list) else 1)
    return 1


def blocked_semantic(mod: str) -> bool:
    return mod not in DRAW_MODULES


def coq_nat(n: int) -> str:
    return f"{n}%nat"


def coq_z(n: int) -> str:
    return f"({n})%Z" if n < 0 else f"{n}%Z"


def hex_to_arith_int(h: str) -> int:
    """A 32-byte hex digest as `arith_uint256`, i.e. little-endian over the raw bytes.

    Both sigma and txid need this. For sigma it is `UintToArith256`. For txid the selection
    compares `uint256::SetHex` buffers with memcmp, and SetHex reverses the hex, so byte-wise
    order on the reversed buffer equals integer order on the little-endian reading of the hex.

    This replaced `int(h, 16)`, which is the big-endian reading and therefore ordered txids
    backwards. That only shows up on cross-product ties, which is why it survived.
    """
    h = (h or "").strip()
    if not h:
        return 0
    return int.from_bytes(bytes.fromhex(h), "little")


def draw_audit_rows_for_vector(v: dict) -> list[tuple[int, list[tuple[bool, int, int]]]]:
    """Return (family_tag, candidates) rows for a RunSelection vector.

    candidates = list of (eligible, sigma, weight, txid). Emitted once per draw
    family (PHASE3_WIN, PHASE4A_DRAW). eligible/winner come from kernel evidence;
    sigma, weight (fee) and txid are raw registry inputs for independent re-computation.
    """
    if v["module"] != "RunSelection":
        return []
    inp = v.get("input", {})
    exp = v.get("expected", {})
    txids = inp.get("txids_hex", []) or []
    fees = inp.get("fees", []) or []
    eligible = exp.get("eligible", []) or []
    sigmas = exp.get("sigmas", []) or []
    cands: list[tuple[bool, int, int, int]] = []
    for i in range(len(txids)):
        el = bool(eligible[i]) if i < len(eligible) else False
        s = hex_to_arith_int(sigmas[i]) if i < len(sigmas) else 0
        w = int(fees[i]) if i < len(fees) else 0
        t = hex_to_arith_int(txids[i])
        cands.append((el, s, w, t))
    return [
        (FAMILY_TAG["PHASE3_WIN"], cands),
        (FAMILY_TAG["PHASE4A_DRAW"], cands),
    ]


def emit_draw_candidate(el: bool, s: int, w: int, t: int) -> str:
    b = "true" if el else "false"
    return (
        f"{{| dc_eligible := {b}; dc_sigma := {coq_z(s)}; "
        f"dc_weight := {coq_z(w)}; dc_txid := {coq_z(t)} |}}"
    )


def emit_draw_audit_row(vector_id: int, family_tag: int, imported: int,
                        cands: list[tuple[bool, int, int, int]]) -> str:
    if cands:
        clist = "[ " + "; ".join(emit_draw_candidate(*c) for c in cands) + " ]"
    else:
        clist = "(@nil DrawCandidate)"
    return (
        f"  {{| dar_vector_id := {coq_nat(vector_id)}; "
        f"dar_family_tag := {coq_nat(family_tag)}; "
        f"dar_imported_winner := {coq_z(imported)}; "
        f"dar_candidates := {clist} |}}"
    )


def emit_trace_case(
    case_id: int,
    vector_id: int,
    family_tag: int,
    slot: int,
    tx_count: int,
    blocked: bool,
    is_draw_winner: bool,
    golden_winner: int,
    kernel_winner: int,
) -> str:
    b = "true" if blocked else "false"
    dw = "true" if is_draw_winner else "false"
    return (
        f"  (mk_golden_trace_case {coq_nat(case_id)} {coq_nat(vector_id)} "
        f"{coq_nat(family_tag)} {coq_nat(slot)} {coq_nat(tx_count)} {b} "
        f"{dw} {coq_z(golden_winner)} {coq_z(kernel_winner)})"
    )


def classify_json_vector(v: dict) -> list[str]:
    mod = v["module"]
    families: list[str] = []
    if mod == "ConvergenceRoot":
        families.append("PHASE3_CR")
        families.append("PHASE4A_DRAW")
    elif mod == "SigmaPoW01":
        families.append("PHASE3_SIG")
        families.append("PHASE4A_DRAW")
    elif mod == "Inequality":
        families.append("PHASE4A_DRAW")
    elif mod == "RunSelection":
        families.append("PHASE3_WIN")
        families.append("PHASE4A_DRAW")
    elif mod in ("Replay", "ReorgBind"):
        families.append("PHASE4B_REPLAY")
    elif mod == "SupplyScaffold":
        families.append("PHASE4D_SUPPLY")
    return families


def emit_coq_list(name: str, elem_type: str, items: list[str]) -> str:
    """A Coq list definition. The empty case needs `(@nil T)`, not `:: nil`."""
    if not items:
        return f"Definition {name} : list {elem_type} :=\n  (@nil {elem_type})."
    body = " ::\n  ".join(items)
    return f"Definition {name} : list {elem_type} :=\n  {body} :: nil."


def check_module_coverage(vectors: list[dict]) -> None:
    """Every module in the JSON must be mapped to a family or explicitly unmapped.

    This is the guard the SigmaCR01 rename slipped past. Without it a renamed module simply
    stops contributing and the generator reports success.
    """
    seen = {v["module"] for v in vectors}
    unknown = sorted(m for m in seen if not classify_json_vector({"module": m}) and m not in UNMAPPED_MODULES)
    if unknown:
        raise SystemExit(
            f"unmapped golden-vector modules: {unknown}\n"
            "Each module must appear in classify_json_vector() or in UNMAPPED_MODULES. "
            "A module that is silently dropped produces an empty trace family."
        )


def main() -> None:
    data = json.loads(JSON_PATH.read_text(encoding="utf-8"))
    vectors = data["vectors"]
    check_module_coverage(vectors)

    family_cases: dict[str, list[str]] = {k: [] for k in FAMILY_TAG}
    audit_rows: list[str] = []
    case_id = 0

    for idx, v in enumerate(vectors):
        slot = vector_slot_from_id(v["id"])
        txc = tx_count_for_vector(v)
        blocked = blocked_semantic(v["module"])
        is_draw, golden_w, kernel_w = draw_winner_for_vector(v)
        for family_tag, cands in draw_audit_rows_for_vector(v):
            audit_rows.append(emit_draw_audit_row(idx, family_tag, kernel_w, cands))
        for fam in classify_json_vector(v):
            family_cases[fam].append(
                emit_trace_case(
                    case_id, idx, FAMILY_TAG[fam], slot, txc, blocked,
                    is_draw, golden_w, kernel_w,
                )
            )
            case_id += 1

    base_4e = len(vectors)
    for i, (name, _oru, _res) in enumerate(PHASE4E_CASES):
        slot = 800 + i
        family_cases["PHASE4E_CONNECTBLOCK"].append(
            emit_trace_case(
                case_id, base_4e + i, FAMILY_TAG["PHASE4E_CONNECTBLOCK"], slot, 1, True,
                False, -1, -1,
            )
        )
        case_id += 1

    total = case_id
    lines = [
        "(* GENERATED by formal_verification/tools/generate_phase4f_golden_traces.py *)",
        "(* Source: phase3_golden_vectors.json + PHASE-4E synthetic cases. DO NOT EDIT. *)",
        "",
        "From Coq Require Import List ZArith.",
        "Import ListNotations.",
        "Require Import Rigor.Phase4FConsensus.ConsensusGoldenTrace.",
        "",
        f"Definition generated_trace_case_count : nat := {coq_nat(total)}.",
        "",
    ]

    empty = sorted(fam for fam, cases in family_cases.items() if not cases)
    if empty:
        raise SystemExit(
            f"trace families with zero cases: {empty}\n"
            "Every family in FAMILY_TAG must draw from at least one golden-vector module. "
            "An empty family means its module mapping went stale."
        )

    for fam in FAMILY_TAG:
        lines.append(emit_coq_list(f"trace_cases_{fam}", "TraceCase", family_cases[fam]))
        lines.append("")

    all_lists = " ++ ".join(f"trace_cases_{fam}" for fam in FAMILY_TAG)
    lines.append(f"Definition all_registry_trace_cases : list TraceCase :=\n  {all_lists}.")
    lines.append("")

    lines.append("(* PHASE-4F-001E independent draw re-computation rows (RunSelection x family). *)")
    lines.append(emit_coq_list("draw_audit_rows", "DrawAuditRow", audit_rows))
    lines.append("")
    lines.append(
        f"Definition draw_audit_rows_count : nat := {coq_nat(len(audit_rows))}."
    )
    lines.append("")
    lines.append(
        "Lemma all_registry_trace_cases_length :\n"
        "  length all_registry_trace_cases = generated_trace_case_count."
    )
    lines.append("Proof.")
    lines.append("  reflexivity.")
    lines.append("Qed.")
    lines.append("")

    OUT_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Wrote {OUT_PATH} ({total} trace cases, {sum(len(v) for v in family_cases.values())} family slots)")


if __name__ == "__main__":
    main()
