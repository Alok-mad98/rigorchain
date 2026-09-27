#!/usr/bin/env python3
"""Build kernel_candidate_dataset and attack_vector traces.

Does not change consensus. Reconstructs KernelInput candidates from C++
decision facts already recorded in CE / equivalence files.
"""
from __future__ import annotations

import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CE_DIR = ROOT / "audit" / "formal_bridge" / "counterexamples"
EQ = ROOT / "audit" / "security" / "consensus_equivalence_dataset.json"
OUT = ROOT / "audit" / "formal_bridge" / "kernel_candidate_dataset"
VEC = ROOT / "tests" / "attack_vectors" / "traces"
SV_GENESIS = ROOT / "tests" / "serialization_vectors" / "cases" / "SV-GENESIS-REGTEST.json"
SV_MINED = ROOT / "tests" / "serialization_vectors" / "cases" / "SV-MINED-REGTEST-H1.json"


def dump(path: Path, obj: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(obj, indent=2) + "\n")


def candidate(**kwargs) -> dict:
    base = {
        "status": "oracle_only",
        "note": "Reconstructed from C++ decisions. Not MakeKernelInput. Not Coq KernelInput.",
        "ki_utxo_ok": None,
        "ki_incoming": None,
        "ki_outgoing": None,
        "ki_fees": None,
        "ki_burn": None,
        "ki_subsidy": None,
        "ki_res_mint": None,
        "ki_script_ok": None,
        "ki_pow_ok": None,
        "ki_asert_ok": None,
    }
    base.update(kwargs)
    return base


def write_case(name: str, hex_blob: str | None, kind: str, trace: dict, cand: dict) -> None:
    d = OUT / name
    d.mkdir(parents=True, exist_ok=True)
    if hex_blob:
        (d / ("block.hex" if kind == "block" else "tx.hex")).write_text(hex_blob.strip() + "\n")
    dump(d / "consensus_trace.json", trace)
    dump(d / "kernel_candidate.json", cand)
    dump(VEC / f"{name}.json", {"before": None, "after": trace, "kernel_candidate": cand})


def ce_bundle() -> None:
    eq = {row["case"]: row for row in json.loads(EQ.read_text())}

    def txid_of(row: dict) -> str:
        detail = row.get("node_a_detail") or {}
        res = detail.get("result") or []
        if res and isinstance(res, list) and isinstance(res[0], dict):
            return str(res[0].get("txid") or "")
        return ""

    # CE-001
    raw = json.loads(eq["A1_connectblock_double_spend"]["input"])[0]
    after = {
        "case_id": "CE-001",
        "block_hash": None,
        "height": None,
        "header_result": "accept",
        "pow_result": None,
        "asert_result": None,
        "tx_results": [
            {
                "txid": "same-outpoint-repeat",
                "function": "CheckTxInputs",
                "inputs_checked": True,
                "utxo_result": False,
                "script_result": True,
                "amount_result": True,
                "fee_result": 0,
                "reason": "bad-txns-inputs-missingorspent",
            }
        ],
        "block_result": "reject",
        "rejection_layer": "CheckTxInputs",
        "kernel_candidate": candidate(status="oracle_only", ki_utxo_ok=False),
    }
    before = {
        **after,
        "tx_results": [
            {
                "txid": "first-spend",
                "function": "CheckTxInputs",
                "inputs_checked": True,
                "utxo_result": True,
                "script_result": True,
                "amount_result": True,
                "fee_result": 0,
                "reason": "",
            }
        ],
        "block_result": "accept",
        "rejection_layer": None,
        "kernel_candidate": candidate(ki_utxo_ok=True),
    }
    write_case("CE-001", raw, "tx", after, after["kernel_candidate"])
    dump(
        VEC / "CE-001.json",
        {
            "expected_rejection_point": "CheckTxInputs",
            "before": before,
            "after": after,
            "kernel_candidate_difference": {"ki_utxo_ok": {"before": True, "after": False}},
        },
    )

    # CE-002
    after2 = {
        "case_id": "CE-002",
        "block_hash": None,
        "height": None,
        "header_result": "accept",
        "pow_result": None,
        "asert_result": None,
        "tx_results": [
            {
                "txid": "",
                "function": "CheckTxInputs",
                "inputs_checked": True,
                "utxo_result": False,
                "script_result": True,
                "amount_result": True,
                "fee_result": 0,
                "reason": "Transaction already in block chain / inputs missing/spent",
            }
        ],
        "block_result": "reject",
        "rejection_layer": "CCoinsViewCache",
        "kernel_candidate": candidate(ki_utxo_ok=False),
    }
    write_case("CE-002", None, "tx", after2, after2["kernel_candidate"])
    dump(
        VEC / "CE-002.json",
        {
            "expected_rejection_point": "CCoinsViewCache / CheckTxInputs",
            "before": {**after2, "block_result": "accept", "tx_results": [{**after2["tx_results"][0], "utxo_result": True, "reason": ""}]},
            "after": after2,
            "kernel_candidate_difference": {"ki_utxo_ok": {"before": True, "after": False}},
        },
    )

    # CE-003 undo — no KernelInput field
    after3 = {
        "case_id": "CE-003",
        "block_hash": json.loads((CE_DIR / "CE-003.json").read_text())["input"]["tip"],
        "height": 122,
        "header_result": "accept",
        "pow_result": True,
        "asert_result": True,
        "tx_results": [],
        "block_result": "accept",
        "rejection_layer": None,
        "kernel_candidate": candidate(status="partial", note="Disconnect has no KernelInput field."),
    }
    write_case("CE-003", None, "block", after3, after3["kernel_candidate"])
    dump(
        VEC / "CE-003.json",
        {
            "expected_rejection_point": None,
            "before": after3,
            "after": after3,
            "kernel_candidate_difference": {},
        },
    )

    amount_cases = [
        ("CE-004a", "B1_output_over_max_money", "bad-txns-vout-toolarge"),
        ("CE-004b", "B1_output_negative", "bad-txns-vout-negative"),
        ("CE-004c", "B1_output_sum_overflow", "bad-txns-txouttotal-toolarge"),
    ]
    ce4_after = []
    for name, key, reason in amount_cases:
        hx = eq[key]["input"]
        tr = {
            "case_id": name,
            "block_hash": None,
            "height": None,
            "header_result": "not_reached",
            "pow_result": None,
            "asert_result": None,
            "tx_results": [
                {
                    "txid": "",
                    "function": "CheckTransaction",
                    "inputs_checked": False,
                    "utxo_result": True,
                    "script_result": True,
                    "amount_result": False,
                    "fee_result": 0,
                    "reason": reason,
                }
            ],
            "block_result": "reject",
            "rejection_layer": "CheckTransaction",
            "kernel_candidate": candidate(ki_outgoing=None, note="Amount reject before ConnectBlock."),
        }
        write_case(name, hx, "tx", tr, tr["kernel_candidate"])
        ce4_after.append(tr)
    dump(
        VEC / "CE-004.json",
        {
            "expected_rejection_point": "CheckTransaction / AddMoneyOverflows",
            "before": None,
            "after": ce4_after,
            "kernel_candidate_difference": {"amount_result": {"before": True, "after": False}},
        },
    )

    script_keys = [
        ("CE-005a", "B2_p2pkh", "bad-rigor-script-type"),
        ("CE-005b", "B2_p2wsh", "bad-rigor-script-type"),
        ("CE-005c", "B2_empty_script", "bad-rigor-script-type"),
    ]
    ce5 = []
    for name, key, reason in script_keys:
        hx = eq[key]["input"]
        tr = {
            "case_id": name,
            "block_hash": None,
            "height": None,
            "header_result": "not_reached",
            "pow_result": None,
            "asert_result": None,
            "tx_results": [
                {
                    "txid": "",
                    "function": "CheckTransaction",
                    "inputs_checked": False,
                    "utxo_result": True,
                    "script_result": False,
                    "amount_result": True,
                    "fee_result": 0,
                    "reason": reason,
                }
            ],
            "block_result": "reject",
            "rejection_layer": "ClassifyRigorTxout / CheckTransaction",
            "kernel_candidate": candidate(ki_script_ok=False),
        }
        write_case(name, hx, "tx", tr, tr["kernel_candidate"])
        ce5.append(tr)
    dump(
        VEC / "CE-005.json",
        {
            "expected_rejection_point": "ClassifyRigorTxout",
            "before": None,
            "after": ce5,
            "kernel_candidate_difference": {"ki_script_ok": {"before": True, "after": False}},
        },
    )

    for name, key, reason, fn in (
        ("CE-006a", "B3_empty_or_unsigned_witness", "Witness program hash mismatch", "VerifyRigorPredicate"),
        ("CE-006b", "B3_mutated_witness_or_tx", "RIGOR_RESERVED_LEGACY_EQUALVERIFY", "VerifyRigorPredicate"),
    ):
        hx = eq[key]["input"]
        tr = {
            "case_id": name,
            "block_hash": None,
            "height": None,
            "header_result": "not_reached",
            "pow_result": None,
            "asert_result": None,
            "tx_results": [
                {
                    "txid": "",
                    "function": fn,
                    "inputs_checked": True,
                    "utxo_result": True,
                    "script_result": False,
                    "amount_result": True,
                    "fee_result": 0,
                    "reason": reason,
                }
            ],
            "block_result": "reject",
            "rejection_layer": "VerifyWitnessProgram",
            "kernel_candidate": candidate(ki_script_ok=False),
        }
        write_case(name, hx, "tx", tr, tr["kernel_candidate"])
    dump(
        VEC / "CE-006.json",
        {
            "expected_rejection_point": "VerifyWitnessProgram / CheckInputScripts",
            "before": None,
            "after": "see CE-006a / CE-006b",
            "kernel_candidate_difference": {"ki_script_ok": {"before": True, "after": False}},
        },
    )

    pow_map = [
        ("CE-007a", "C2_illegal_nbits_zero", "CheckProofOfWork", False, None),
        ("CE-007b", "C2_illegal_nbits_overflow", "CheckProofOfWork", False, None),
        ("CE-007c", "C2_target_above_powlimit", "CheckProofOfWork", False, None),
        ("CE-007d", "C2_future_timestamp", "ContextualCheckBlockHeader", True, False),
        ("CE-007e", "C2_nonce_flip", "CheckProofOfWork", False, True),
        ("CE-007f", "C1_truncated_block", "DecodeHexBlk", None, None),
    ]
    ce7 = []
    for name, key, layer, pow_ok, asert_ok in pow_map:
        hx = eq[key]["input"]
        decode_fail = key.startswith("C1")
        tr = {
            "case_id": name,
            "block_hash": None,
            "height": None,
            "header_result": "reject" if not decode_fail else "not_reached",
            "pow_result": pow_ok,
            "asert_result": asert_ok if not decode_fail else None,
            "tx_results": [],
            "block_result": "decode_fail" if decode_fail else "reject",
            "rejection_layer": layer,
            "kernel_candidate": candidate(
                status="not_reached" if decode_fail else "oracle_only",
                ki_pow_ok=False if pow_ok is False else None,
                ki_asert_ok=False if asert_ok is False else None,
            ),
        }
        write_case(name, hx, "block", tr, tr["kernel_candidate"])
        ce7.append(tr)
    dump(
        VEC / "CE-007.json",
        {
            "expected_rejection_point": "CheckProofOfWork / ContextualCheckBlockHeader / DecodeHexBlk",
            "before": None,
            "after": ce7,
            "kernel_candidate_difference": {"ki_pow_ok": {"before": True, "after": False}},
        },
    )


def accepted_from_vectors() -> None:
    for src, name in ((SV_GENESIS, "SV-GENESIS-REGTEST"), (SV_MINED, "SV-MINED-REGTEST-H1")):
        if not src.is_file():
            continue
        row = json.loads(src.read_text())
        hx = row.get("raw_block_hex")
        fields = (row.get("decode_result") or {}).get("fields") or {}
        tr = {
            "case_id": name,
            "block_hash": None,
            "height": row.get("height", 0 if "GENESIS" in name else 1),
            "header_result": "accept",
            "pow_result": True,
            "asert_result": True,
            "tx_results": [
                {
                    "txid": "",
                    "function": "CheckTransaction",
                    "inputs_checked": False,
                    "utxo_result": True,
                    "script_result": True,
                    "amount_result": True,
                    "fee_result": 0,
                    "reason": "",
                }
            ],
            "block_result": "accept",
            "rejection_layer": None,
            "kernel_candidate": candidate(
                ki_utxo_ok=True,
                ki_script_ok=True,
                ki_pow_ok=True,
                ki_asert_ok=True,
            ),
            "nBits": fields.get("nBits"),
        }
        write_case(name, hx, "block", tr, tr["kernel_candidate"])


def harvest_live(live_dir: Path) -> None:
    if not live_dir.is_dir():
        return
    dest = OUT / "live_connectblock"
    if dest.is_dir() and any(dest.iterdir()):
        return
    dest.mkdir(parents=True, exist_ok=True)
    traces = sorted(live_dir.glob("*-consensus_trace.json"))
    if not traces:
        return
    picks = []
    h0 = [p for p in traces if p.name.startswith("h0-") and "-accept-" in p.name]
    h1 = [p for p in traces if p.name.startswith("h1-") and "-accept-" in p.name]
    rej = [p for p in traces if "-reject-" in p.name]
    if h0:
        picks.append(h0[0])
    if h1:
        picks.append(h1[0])
    if rej:
        picks.append(rej[0])
    for src in picks:
        stem = src.name.replace("-consensus_trace.json", "")
        case = dest / stem
        case.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, case / "consensus_trace.json")
        cand = src.with_name(src.name.replace("-consensus_trace.json", "-kernel_candidate.json"))
        if cand.is_file():
            shutil.copy2(cand, case / "kernel_candidate.json")


def main() -> int:
    if OUT.exists():
        for child in OUT.iterdir():
            if child.name in ("README.md", "live_connectblock", "index.jsonl"):
                continue
            if child.name.startswith("CE-00") and child.name[3:6].isdigit() and int(child.name[3:6]) >= 8:
                continue
            if child.is_dir() and child.name.startswith("CE-00") and int(child.name[3:6]) < 8:
                shutil.rmtree(child)
            elif child.is_dir() and child.name.startswith("SV-"):
                shutil.rmtree(child)
    OUT.mkdir(parents=True, exist_ok=True)
    VEC.mkdir(parents=True, exist_ok=True)
    ce_bundle()
    accepted_from_vectors()
    harvest_live(ROOT / "tests" / "attack_lab" / "data_task007" / "on" / "regtest" / "consensus_trace")
    harvest_live(ROOT / "tests" / "serialization_vectors" / "data" / "regtest" / "regtest" / "consensus_trace")
    (OUT / "README.md").write_text(
 " KernelInput candidate dataset.\n\n"
        "Each case: hex + consensus_trace.json + kernel_candidate.json.\n"
        "Live C++ files appear under live_connectblock/ after "
        "`rigord -rigorconsensustrace=1`.\n"
    )
    print("wrote", OUT, "and", VEC)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
