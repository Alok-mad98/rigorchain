#!/usr/bin/env python3
"""Trace → KernelInputCandidate. Deterministic. Not a Coq KernelInput.

Does not change consensus. Does not edit KernelInput.v.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any, Mapping, Optional

KI_FIELDS = (
    "ki_utxo_ok",
    "ki_incoming",
    "ki_outgoing",
    "ki_fees",
    "ki_burn",
    "ki_subsidy",
    "ki_res_mint",
    "ki_script_ok",
    "ki_pow_ok",
    "ki_asert_ok",
)

FIELD_CLASS = {
    "ki_utxo_ok": "DERIVED",
    "ki_script_ok": "DERIVED",
    "ki_pow_ok": "DIRECT",
    "ki_asert_ok": "DIRECT",
    "ki_fees": "DERIVED",
    "ki_subsidy": "MISSING",
    "ki_res_mint": "MISSING",
    "ki_incoming": "MISSING",
    "ki_outgoing": "MISSING",
    "ki_burn": "MISSING",
}

SCRIPT_FNS = {"CheckTransaction", "VerifyRigorPredicate", "VerifyWitnessProgram"}
ORACLE_NOTE = (
    "Generated from consensus_trace.json. Not MakeKernelInput. Not Coq KernelInput."
)


def canonical_payload(fields: Mapping[str, Any]) -> dict:
    return {k: fields[k] for k in sorted(KI_FIELDS)}


def canonical_json(fields: Mapping[str, Any]) -> str:
    return json.dumps(canonical_payload(fields), sort_keys=True, separators=(",", ":"), ensure_ascii=True)


def candidate_hash(fields: Mapping[str, Any]) -> str:
    return hashlib.sha256(canonical_json(fields).encode("utf-8")).hexdigest()


def _and_or_true(values: list[bool]) -> bool:
    acc = True
    for v in values:
        acc = acc and bool(v)
    return acc


def generate_kernel_candidate(trace: Mapping[str, Any]) -> dict:
    txs = list(trace.get("tx_results") or [])
    utxo_vals = [
        bool(row.get("utxo_result"))
        for row in txs
        if row.get("function") == "CheckTxInputs" and row.get("inputs_checked")
    ]
    script_vals = []
    for row in txs:
        fn = row.get("function")
        if fn in SCRIPT_FNS:
            script_vals.append(bool(row.get("script_result")))
        if fn == "CheckTransaction":
            script_vals.append(bool(row.get("amount_result")))
    fees = 0
    for row in txs:
        try:
            fees += int(row.get("fee_result") or 0)
        except (TypeError, ValueError):
            fees += 0

    pow_raw = trace.get("pow_result")
    asert_raw = trace.get("asert_result")
    fields = {
        "ki_utxo_ok": _and_or_true(utxo_vals),
        "ki_script_ok": _and_or_true(script_vals),
        "ki_pow_ok": pow_raw if isinstance(pow_raw, bool) else None,
        "ki_asert_ok": asert_raw if isinstance(asert_raw, bool) else None,
        "ki_fees": fees,
        "ki_incoming": None,
        "ki_outgoing": None,
        "ki_burn": None,
        "ki_subsidy": None,
        "ki_res_mint": None,
    }
    modelled = [fields[k] is not None for k in KI_FIELDS]
    status = "oracle_only" if any(modelled) else "not_reached"
    digest = candidate_hash(fields)
    return {
        "status": status,
        "note": ORACLE_NOTE,
        "field_class": dict(FIELD_CLASS),
        **fields,
        "candidate_hash": digest,
    }


def load_trace(path: Path) -> dict:
    obj = json.loads(path.read_text())
    if not isinstance(obj, dict):
        raise ValueError(f"{path} is not a JSON object")
    return obj


def main(argv: Optional[list[str]] = None) -> int:
    p = argparse.ArgumentParser(description="Generate KernelInputCandidate from a consensus trace")
    p.add_argument("trace", type=Path, help="consensus_trace.json")
    p.add_argument("-o", "--output", type=Path, help="kernel_candidate.json (default: stdout)")
    args = p.parse_args(argv)
    cand = generate_kernel_candidate(load_trace(args.trace))
    text = json.dumps(cand, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
