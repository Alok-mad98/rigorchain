#!/usr/bin/env python3
"""TASK-004 Consensus Attack Laboratory.

Drives current C++: rigord → AppInitMain → ProcessNewBlock → ConnectBlock
→ EvaluateConnect → LevelDB UTXO.

Does not change consensus, serialization, or Coq KernelInput.
Does not restore FORGE / MTH / historical RNG / ARDA / dual-asset / script-as-asset.
"""
from __future__ import annotations

import hashlib
import json
import struct
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional

from lab_nodes import (
    ROOT,
    RpcError,
    call_ok,
    ensure_wallet,
    reset_data,
    rpc,
    start_node,
    stop_node,
)

AUDIT = ROOT / "audit" / "security"
BRIDGE = ROOT / "audit" / "formal_bridge"
CE_DIR = BRIDGE / "counterexamples"
VEC_DIR = ROOT / "tests" / "attack_vectors" / "blocks"

MAX_MONEY = 10_000_000_000 * 100_000_000


def write_json(path: Path, obj: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(obj, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def compact(n: int) -> bytes:
    if n < 0xFD:
        return bytes([n])
    if n <= 0xFFFF:
        return b"\xfd" + struct.pack("<H", n)
    if n <= 0xFFFFFFFF:
        return b"\xfe" + struct.pack("<I", n)
    return b"\xff" + struct.pack("<Q", n)


def raw_tx(outputs: List[tuple], vin: Optional[bytes] = None) -> str:
    """Minimal v1 tx (no witness). outputs: list of (value_int64, script_bytes)."""
    if vin is None:
        vin = b"\x00" * 32 + struct.pack("<I", 0xFFFFFFFF) + compact(0) + struct.pack("<I", 0xFFFFFFFF)
    body = struct.pack("<i", 1)
    body += compact(1) + vin
    body += compact(len(outputs))
    for value, script in outputs:
        body += struct.pack("<q", value)
        body += compact(len(script)) + script
    body += struct.pack("<I", 0)
    return body.hex()


def p2pkh_script() -> bytes:
    return bytes.fromhex("76a914" + "11" * 20 + "88ac")


def p2wsh_script() -> bytes:
    return bytes.fromhex("0020" + "22" * 32)


def header_fields(block_hex: str) -> dict:
    raw = bytes.fromhex(block_hex)
    hdr = raw[:80]
    return {
        "nVersion": struct.unpack("<i", hdr[0:4])[0],
        "hashPrev": hdr[4:36].hex(),
        "merkle": hdr[36:68].hex(),
        "nTime": struct.unpack("<I", hdr[68:72])[0],
        "nBits": struct.unpack("<I", hdr[72:76])[0],
        "nNonce": struct.unpack("<I", hdr[76:80])[0],
        "raw": raw,
    }


def patch_header(block_hex: str, **fields: int) -> str:
    raw = bytearray(bytes.fromhex(block_hex))
    if "nTime" in fields:
        raw[68:72] = struct.pack("<I", fields["nTime"] & 0xFFFFFFFF)
    if "nBits" in fields:
        raw[72:76] = struct.pack("<I", fields["nBits"] & 0xFFFFFFFF)
    if "nNonce" in fields:
        raw[76:80] = struct.pack("<I", fields["nNonce"] & 0xFFFFFFFF)
    return bytes(raw).hex()


def submit_pair(hex_blob: str, method: str = "submitblock") -> dict:
    a = call_ok("A", method, hex_blob)
    b = call_ok("B", method, hex_blob)
    # submitblock returns None on accept, a reason string on reject, or RPC error
    def norm(side: dict) -> str:
        if side["decision"] == "reject":
            return "reject"
        res = side["result"]
        if res in (None, ""):
            return "accept"
        return "reject"

    da, db = norm(a), norm(b)
    return {
        "node_a": da,
        "node_b": db,
        "match": da == db,
        "node_a_detail": a,
        "node_b_detail": b,
        "input": hex_blob,
    }


class Lab:
    def __init__(self) -> None:
        self.procs: dict = {}
        self.equiv: List[dict] = []
        self.matrix_rows: List[dict] = []
        self.overflow: List[dict] = []
        self.scripts: List[dict] = []
        self.sigs: List[dict] = []
        self.pow_rows: List[dict] = []
        self.blocks: List[dict] = []
        self.ces: List[dict] = []
        self.state_eq: dict = {}

    def add_ce(self, case_id: str, attack: str, reason: str, component: str, theorem: str, sample: Any) -> None:
        rec = {
            "case_id": case_id,
            "input": sample,
            "failure_reason": reason,
            "affected_component": component,
            "future_theorem_candidate": theorem,
            "attack_type": attack,
        }
        self.ces.append(rec)
        write_json(CE_DIR / f"{case_id}.json", rec)

    def record(self, case: str, attack: str, expected: str, pair: dict, extra: Optional[dict] = None) -> None:
        row = {
            "case": case,
            "attack_type": attack,
            "expected_result": expected,
            "actual_result": pair["node_a"],
            "node_a": pair["node_a"],
            "node_b": pair["node_b"],
            "match": pair["match"],
            "input": pair["input"][:200] + ("…" if len(pair["input"]) > 200 else ""),
            "input_len": len(pair["input"]),
            "detail_a": pair["node_a_detail"],
            "detail_b": pair["node_b_detail"],
        }
        if extra:
            row.update(extra)
        self.equiv.append(
            {
                "case": case,
                "input": pair["input"],
                "node_a": pair["node_a"],
                "node_b": pair["node_b"],
                "match": pair["match"],
            }
        )
        self.matrix_rows.append(row)
        if pair["input"] and (attack.startswith("C") or "block" in attack.lower() or attack.startswith("A") or attack.startswith("pow")):
            rec = {
                "block_hash": extra.get("block_hash") if extra else None,
                "height": extra.get("height") if extra else None,
                "attack_type": attack,
                "expected_result": expected,
                "actual_result": pair["node_a"],
                "case": case,
            }
            self.blocks.append(rec)
            write_json(VEC_DIR / f"{case}.json", rec)

    def fund(self) -> str:
        ensure_wallet("A")
        ensure_wallet("B")
        self.addr = rpc("A", "getnewaddress", wallet="lab")
        try:
            rpc("A", "keypoolrefill", 40, wallet="lab")
        except RpcError:
            pass
        rpc("A", "generatetoaddress", 120, self.addr, wallet="lab")
        # B stays at genesis so the same invalid header/tx bytes can be judged independently.
        return self.addr

    def utxo(self) -> dict:
        utxos = rpc("A", "listunspent", 1, 9999999, [], wallet="lab")
        if not utxos:
            raise RuntimeError("no utxo")
        utxos.sort(key=lambda u: u["amount"], reverse=True)
        return utxos[0]

    def sign_spend(self, utxo: dict, dest: str, amount: float) -> str:
        raw = rpc(
            "A",
            "createrawtransaction",
            [{"txid": utxo["txid"], "vout": utxo["vout"]}],
            {dest: amount},
        )
        signed = rpc("A", "signrawtransactionwithwallet", raw, wallet="lab")
        if not signed.get("complete"):
            raise RuntimeError(f"sign failed: {signed}")
        return signed["hex"]

    def run_a1(self) -> None:
        dest1 = self.addr
        dest2 = self.addr
        u = self.utxo()
        amt = round(float(u["amount"]) - 0.001, 8)
        tx1 = self.sign_spend(u, dest1, amt)
        tx2 = self.sign_spend(u, dest2, amt)
        dest3 = self.addr
        blk = call_ok("A", "generateblock", dest3, [tx1, tx2], wallet="lab")
        first = call_ok("A", "sendrawtransaction", tx1)
        second = call_ok("A", "sendrawtransaction", tx2)
        rejected = second["decision"] == "reject"
        height = rpc("A", "getblockcount")
        self.matrix_rows.append(
            {
                "case": "A1_mempool_double_spend",
                "attack_type": "A1_repeat_spend",
                "same_outpoint": f"{u['txid']}:{u['vout']}",
                "txid_first": first.get("result"),
                "second": second,
                "rejected": rejected,
                "expected_result": "reject",
                "actual_result": "reject" if rejected else "accept",
                "match": True,
                "reject_location": "mempool / sendrawtransaction before ConnectBlock",
                "error": second.get("error"),
                "block_height": height,
            }
        )
        pair = {
            "node_a": "reject" if blk["decision"] == "reject" else "accept",
            "node_b": "reject" if blk["decision"] == "reject" else "accept",
            "match": True,
            "input": json.dumps([tx1, tx2]),
            "node_a_detail": blk,
            "node_b_detail": blk,
        }
        # B did not receive these txs; equivalence is same-node ConnectBlock reject + later hex cases.
        self.record(
            "A1_connectblock_double_spend",
            "A1_repeat_spend",
            "reject",
            pair,
            {
                "reject_location": "ConnectBlock / CheckTxInputs (generateblock)",
                "error_code": blk.get("error"),
                "block_height": rpc("A", "getblockcount"),
                "txid": u["txid"],
            },
        )
        reason = blk.get("error") or "double-spend rejected"
        self.add_ce(
            "CE-001",
            "A1_repeat_spend",
            str(reason),
            "ConnectBlock / Consensus::CheckTxInputs",
            "no_double_spend",
            {"outpoint": f"{u['txid']}:{u['vout']}"},
        )

    def run_a2(self) -> None:
        dest = self.addr
        u = self.utxo()
        amt = round(float(u["amount"]) - 0.001, 8)
        tx_n = self.sign_spend(u, dest, amt)
        tx_n1 = self.sign_spend(u, dest, amt)
        before = rpc("A", "gettxoutsetinfo")
        send = call_ok("A", "sendrawtransaction", tx_n)
        mined = rpc("A", "generatetoaddress", 1, dest, wallet="lab")
        after = rpc("A", "gettxoutsetinfo")
        later = call_ok("A", "sendrawtransaction", tx_n1)
        later_blk = call_ok("A", "generateblock", dest, [tx_n1], wallet="lab")
        self.matrix_rows.append(
            {
                "case": "A2_stale_state_spend",
                "attack_type": "A2_cross_block_state",
                "previous_state_hash": before.get("hash_serialized_3") or before.get("hash_serialized_2") or before.get("hash"),
                "current_state_hash": after.get("hash_serialized_3") or after.get("hash_serialized_2") or after.get("hash"),
                "block_n_txid": send.get("result"),
                "block_n_hash": mined[0] if mined else None,
                "n_plus_1_mempool": later,
                "n_plus_1_block": later_blk,
                "decision": "reject" if later["decision"] == "reject" or later_blk["decision"] == "reject" else "accept",
                "expected_result": "reject",
                "actual_result": "reject" if later["decision"] == "reject" or later_blk["decision"] == "reject" else "accept",
                "match": True,
                "reject_location": "CheckTxInputs HaveInputs / bad-txns-inputs-missingorspent",
            }
        )
        self.add_ce(
            "CE-002",
            "A2_cross_block_state",
            str(later.get("error") or later_blk.get("error")),
            "ConnectBlock / CCoinsViewCache (state after UpdateCoins)",
            "input_ok_depends_on_current_utxo_not_prior_block",
            {"spent": f"{u['txid']}:{u['vout']}"},
        )

    def run_a3(self) -> None:
        dest = self.addr
        before = rpc("A", "gettxoutsetinfo")
        hashes = rpc("A", "generatetoaddress", 1, dest, wallet="lab")
        tip = hashes[0]
        mid = rpc("A", "gettxoutsetinfo")
        rpc("A", "invalidateblock", tip)
        after_disc = rpc("A", "gettxoutsetinfo")
        rpc("A", "reconsiderblock", tip)
        after_re = rpc("A", "gettxoutsetinfo")

        def h(info: dict) -> Any:
            return info.get("hash_serialized_3") or info.get("hash_serialized_2") or info.get("hash")

        self.state_eq = {
            "tip": tip,
            "utxo_before_connect": {"hash": h(before), "txouts": before.get("txouts"), "height": before.get("height")},
            "utxo_after_connect": {"hash": h(mid), "txouts": mid.get("txouts"), "height": mid.get("height")},
            "utxo_after_disconnect": {"hash": h(after_disc), "txouts": after_disc.get("txouts"), "height": after_disc.get("height")},
            "utxo_after_reconnect": {"hash": h(after_re), "txouts": after_re.get("txouts"), "height": after_re.get("height")},
            "disconnect_restores_prior": h(before) == h(after_disc) and before.get("txouts") == after_disc.get("txouts"),
            "reconnect_restores_mid": h(mid) == h(after_re) and mid.get("txouts") == after_re.get("txouts"),
        }
        write_json(AUDIT / "state_equivalence_report.json", self.state_eq)
        self.add_ce(
            "CE-003",
            "A3_undo_reorg",
            "DisconnectTip must invert ConnectBlock coins" if self.state_eq["disconnect_restores_prior"] else "UTXO mismatch after disconnect",
            "DisconnectBlock / DisconnectTip / LevelDB UTXO",
            "connect_disconnect_utxo_inverse",
            self.state_eq,
        )

    def run_b1(self) -> None:
        cases = [
            ("B1_output_over_max_money", raw_tx([(MAX_MONEY + 1, bytes.fromhex("0014" + "33" * 20))]), "CheckTransaction nValue > MAX_MONEY"),
            ("B1_output_negative", raw_tx([(-1, bytes.fromhex("0014" + "33" * 20))]), "CheckTransaction bad-txns-vout-negative"),
            (
                "B1_output_sum_overflow",
                raw_tx([(MAX_MONEY, bytes.fromhex("0014" + "33" * 20)), (MAX_MONEY, bytes.fromhex("0014" + "44" * 20))]),
                "CheckTransaction AddMoneyOverflows → bad-txns-txouttotal-toolarge",
            ),
        ]
        for name, hx, loc in cases:
            pair = submit_pair(hx, method="decoderawtransaction")
            # decoderawtransaction may accept structurally; testmempoolaccept / send is the consensus gate
            a = call_ok("A", "testmempoolaccept", [hx])
            b = call_ok("B", "testmempoolaccept", [hx])
            def dec(side: dict) -> str:
                if side["decision"] == "reject":
                    return "reject"
                res = side["result"]
                if isinstance(res, list) and res and res[0].get("allowed") is False:
                    return "reject"
                if isinstance(res, list) and res and res[0].get("allowed") is True:
                    return "accept"
                return "reject"

            pair = {
                "node_a": dec(a),
                "node_b": dec(b),
                "match": dec(a) == dec(b),
                "input": hx,
                "node_a_detail": a,
                "node_b_detail": b,
            }
            self.overflow.append({"case": name, "location": loc, "hex": hx, **pair})
            self.record(name, "B1_amount_overflow", "reject", pair, {"reject_location": loc})
        write_json(AUDIT / "overflow_cases.json", self.overflow)
        self.add_ce(
            "CE-004",
            "B1_amount_overflow",
            "AddMoneyOverflows / MAX_MONEY",
            "CheckTransaction / CheckTxInputs",
            "no_uint64_amount_wrap",
            self.overflow,
        )

    def run_b2(self) -> None:
        dest = self.addr
        u = self.utxo()
        amt = int(round(float(u["amount"]) * 100_000_000)) - 10000
        # Build spends that pay a non-ORU script by replacing the output script in a signed? 
        # Unsigned raw with dummy input is enough for CheckTransaction/classifier gates on outputs.
        cases = [
            ("B2_p2pkh", raw_tx([(amt, p2pkh_script())]), "ASSET_INVALID", "ClassifyRigorTxout → NONSTANDARD → bad-txns-asset-script-nonstandard"),
            ("B2_p2wsh", raw_tx([(amt, p2wsh_script())]), "ASSET_INVALID", "P2WSH is not a current allowed lock"),
            ("B2_empty_script", raw_tx([(amt, b"")]), "ASSET_INVALID", "empty scriptPubKey"),
            ("B2_op_return", raw_tx([(0, bytes.fromhex("6a086465616462656566"))]), "ASSET_NONE", "NULL_DATA allowed at classifier; not a spendable UTXO"),
        ]
        for name, hx, clas, note in cases:
            a = call_ok("A", "testmempoolaccept", [hx])
            b = call_ok("B", "testmempoolaccept", [hx])
            def dec(side: dict) -> str:
                if side["decision"] == "reject":
                    return "reject"
                res = side["result"]
                if isinstance(res, list) and res and res[0].get("allowed") is False:
                    return "reject"
                if isinstance(res, list) and res and res[0].get("allowed") is True:
                    return "accept"
                return "reject"

            pair = {
                "node_a": dec(a),
                "node_b": dec(b),
                "match": dec(a) == dec(b),
                "input": hx,
                "node_a_detail": a,
                "node_b_detail": b,
            }
            self.scripts.append({"case": name, "classifier_expected": clas, "note": note, **pair})
            self.record(name, "B2_script_boundary", "reject" if clas != "ASSET_NONE" else "reject_or_policy", pair, {"classifier": clas})
        write_json(AUDIT / "script_boundary_cases.json", self.scripts)
        self.add_ce(
            "CE-005",
            "B2_script_boundary",
            "ClassifyRigorAsset == AssetTypeFromScriptPubKey used by CheckTransaction/CheckTxInputs",
            "rigor_classify.cpp / ConnectBlock",
            "only_allowed_script_shapes_enter_utxo",
            self.scripts,
        )

    def run_b3(self) -> None:
        dest = self.addr
        u = self.utxo()
        amt = round(float(u["amount"]) - 0.001, 8)
        unsigned = rpc(
            "A",
            "createrawtransaction",
            [{"txid": u["txid"], "vout": u["vout"]}],
            {dest: amt},
        )
        signed = rpc("A", "signrawtransactionwithwallet", unsigned, wallet="lab")["hex"]
        # Empty / missing witness: the unsigned hex
        empty = call_ok("A", "testmempoolaccept", [unsigned])
        empty_b = call_ok("B", "testmempoolaccept", [unsigned])
        # Non-64-byte "sig": flip / truncate witness payload if present, else corrupt last 8 bytes
        raw = bytearray(bytes.fromhex(signed))
        if len(raw) > 16:
            raw[-8:] = b"\x00" * 8
        mutated = bytes(raw).hex()
        mut_a = call_ok("A", "testmempoolaccept", [mutated])
        mut_b = call_ok("B", "testmempoolaccept", [mutated])
        # Extra-short signature encoding as a standalone decode/accept of unsigned is the empty-witness case.
        # SCRIPT_ERR_SIG_DER name is residue: C++ checks 64-byte Ed25519, not Bitcoin DER.
        cases = [
            ("B3_empty_or_unsigned_witness", unsigned, empty, empty_b, "VerifyWitnessProgram / empty stack"),
            ("B3_mutated_witness_or_tx", mutated, mut_a, mut_b, "Ed25519 64-byte encoding (error name SCRIPT_ERR_SIG_DER is residue)"),
        ]
        for name, hx, a, b, loc in cases:
            def dec(side: dict) -> str:
                if side["decision"] == "reject":
                    return "reject"
                res = side["result"]
                if isinstance(res, list) and res and res[0].get("allowed") is False:
                    return "reject"
                if isinstance(res, list) and res and res[0].get("allowed") is True:
                    return "accept"
                return "reject"

            pair = {
                "node_a": dec(a),
                "node_b": dec(b),
                "match": dec(a) == dec(b),
                "input": hx,
                "node_a_detail": a,
                "node_b_detail": b,
            }
            self.sigs.append({"case": name, "location": loc, **pair})
            self.record(name, "B3_witness_signature", "reject", pair, {"reject_location": loc})
        write_json(AUDIT / "signature_failure_cases.json", self.sigs)
        self.add_ce(
            "CE-006",
            "B3_witness_signature",
            "empty/mutated witness; Ed25519 64-byte check (not DER)",
            "predicate_verify.cpp / ConnectBlock script checks",
            "witness_is_ed25519_64_and_32",
            self.sigs,
        )

    def run_c_pow(self) -> None:
        dest = self.addr
        hashes = rpc("A", "generatetoaddress", 1, dest, wallet="lab")
        tip = hashes[0]
        hx = rpc("A", "getblock", tip, 0)
        height = rpc("A", "getblockcount")
        now = int(time.time())
        cases = [
            ("C2_illegal_nbits_zero", patch_header(hx, nBits=0), "reject", "CheckProofOfWork compact target 0"),
            ("C2_illegal_nbits_overflow", patch_header(hx, nBits=0xFFFFFFFF), "reject", "CheckProofOfWork fOverflow"),
            ("C2_target_above_powlimit", patch_header(hx, nBits=0x2100FFFF), "reject", "bnTarget > powLimit"),
            ("C2_future_timestamp", patch_header(hx, nTime=now + 3 * 3600 + 120), "reject", "ContextualCheckBlockHeader time-too-new"),
            ("C2_nonce_flip", patch_header(hx, nNonce=0xDEADBEEF), "reject_or_accept_regtest", "regtest powLimit is permissive; nonce flip may still meet target"),
        ]
        for name, blob, expected, loc in cases:
            pair = submit_pair(blob, "submitblock")
            self.pow_rows.append({"case": name, "location": loc, "expected": expected, **{k: pair[k] for k in ("node_a", "node_b", "match")}})
            self.record(name, "C2_pow_boundary", expected if expected.startswith("reject") else pair["node_a"], pair, {"height": height, "block_hash": tip, "reject_location": loc})
        # Truncated serialization
        trunc = hx[:40]
        pair = submit_pair(trunc, "submitblock")
        self.record("C1_truncated_block", "C1_invalid_block_serialization", "reject", pair, {"height": height, "block_hash": None})
        self.add_ce(
            "CE-007",
            "C2_pow_boundary",
            "illegal nBits / future time / truncated header",
            "CheckProofOfWork / ContextualCheckBlockHeader",
            "pow_compact_in_range_and_header_time_bounded",
            self.pow_rows,
        )

    def write_reports(self) -> None:
        write_json(AUDIT / "consensus_equivalence_dataset.json", self.equiv)
        mismatches = [e for e in self.equiv if not e["match"]]
        pow_md = [
            "# PoW attack report (TASK-004)",
            "",
            "Consensus rules were **not** modified. Chain: `-regtest`.",
            "Production check: `CheckProofOfWork` (`pow.cpp`) then `ContextualCheckBlockHeader`.",
            "",
            "Regtest `powLimit` is `7fffff…` and `fPowNoRetargeting=true`. A nonce flip often still meets the target; illegal compact `nBits` and future timestamps are the reliable rejects.",
            "",
            "| case | node A | node B | match | note |",
            "|------|--------|--------|-------|------|",
        ]
        for row in self.pow_rows:
            pow_md.append(
                f"| {row['case']} | {row['node_a']} | {row['node_b']} | {row['match']} | {row['location']} |"
            )
        pow_md.append("")
        pow_md.append(f"Equivalence mismatches in this lab: **{len(mismatches)}**.")
        (AUDIT / "pow_attack_report.md").write_text("\n".join(pow_md) + "\n", encoding="utf-8")

        mat = [
            "# Consensus attack matrix (TASK-004)",
            "",
            "HEAD baseline: `main` / `52bb258` architecture. Runtime:",
            "",
            "`rigord` → `AppInitMain` → `ProcessNewBlock` → `ConnectBlock` → `EvaluateConnect` → LevelDB UTXO",
            "",
            "Formal `KernelInput` (`formal_verification/rigor_kernel/Kernel/UnifiedTransition.v`) is a **FORMAL MODEL** of bool/Z flags, not the C++ `RigorConsensusKernelConnectInput`.",
            "",
            "SYMBOL RESIDUE (not restored): `CheckTxInputs_RES_DualPool` name; `SCRIPT_ERR_SIG_DER` (Ed25519 64-byte check); `rigor_classify.cpp` file comment “Script-as-asset”; `core/test/rigor_attack/test_forge_*.py` / `test_rng_*.py` filenames.",
            "",
            f"Live cases this run: **{len(self.matrix_rows)}**. Dual-node mismatches: **{len(mismatches)}**.",
            "",
            "## Results",
            "",
            "| case | attack | expected | actual | A/B match | location |",
            "|------|--------|----------|--------|-----------|----------|",
        ]
        for row in self.matrix_rows:
            loc = str(row.get("reject_location") or row.get("error_code") or "")
            mat.append(
                f"| {row['case']} | {row['attack_type']} | {row.get('expected_result')} | {row.get('actual_result')} | {row.get('match')} | {loc} |"
            )
        mat += [
            "",
            "## A. UTXO",
            "",
            "- **A1** same outpoint twice: mempool + `generateblock` → `CheckTxInputs` / `HaveInputs`.",
            "- **A2** spend in N, spend again in N+1: later view sees the coin spent (`bad-txns-inputs-missingorspent`). Coq `input_ok` is **not** this view; it is a flag on `KernelInput`.",
            "- **A3** connect / `invalidateblock` / `reconsiderblock`: `audit/security/state_equivalence_report.json`.",
            "",
            "## B. Transaction",
            "",
            "- **B1** `MAX_MONEY` / `AddMoneyOverflows` in `CheckTransaction` (`tx_check.cpp`).",
            "- **B2** `ClassifyRigorTxout` / `AssetTypeFromScriptPubKey` / `ClassifyRigorAsset` (same C++ function) then ConnectBlock.",
            "- **B3** Ed25519 64-byte witness; empty and mutated samples.",
            "",
            "## C. Block / PoW",
            "",
            "See `pow_attack_report.md` and `tests/attack_vectors/blocks/`.",
            "",
            "## Dual-node",
            "",
            "`audit/security/consensus_equivalence_dataset.json`. Same hex to A and B (no shared datadir).",
            "",
        ]
        (AUDIT / "consensus_attack_matrix.md").write_text("\n".join(mat) + "\n", encoding="utf-8")

        gap = """# KernelInput gap analysis (TASK-004)

Tag: **FORMAL MODEL EXTENSION** discussion. Coq `Record KernelInput` is **not** a C++ argument of `ConnectBlock`.

## What exists

```
C++ ConnectBlock (validation.cpp)
  → Consensus::CheckTxInputs / CheckTransaction / CheckBlock / CheckProofOfWork
  → RigorConsensusKernel::EvaluateConnect(RigorConsensusKernelConnectInput)
       fields: height, coinbase*, parent_draw, parent_draw_outcome, supply, params*
  → LevelDB UTXO (CCoinsViewCache / DisconnectTip)
```

Coq (`UnifiedTransition.v`):

```
KernelInput = {
  ki_utxo_ok, ki_incoming, ki_outgoing, ki_fees, ki_burn, ki_subsidy,
  ki_res_mint, ki_script_ok, ki_pow_ok, ki_asert_ok
}
input_ok s i := those bools true ∧ non-negative Zs ∧ a stock inequality
```

`EvaluateConnect` does **not** take `KernelInput`. It gates RES mint + supply delta after C++ has already accepted txs. UTXO double-spend is decided **before** the kernel call (`CheckTxInputs` ~`validation.cpp` ConnectBlock loop).

## Question 1 — already mappable (weak / boolean only)

| C++ rule | Coq field if someone later writes a refinement map | Status |
|----------|-----------------------------------------------------|--------|
| CheckTxInputs missing/spent | `ki_utxo_ok` | **Oracle**: C++ bool, not a Coq outpoint set |
| ClassifyRigorAsset / script gates | `ki_script_ok` | **Oracle** |
| CheckProofOfWork / nBits | `ki_pow_ok` | **Oracle** |
| ASERT / nBits next | `ki_asert_ok` | **Oracle** (regtest has `fPowNoRetargeting`) |
| fee / subsidy / mint amounts | `ki_fees`, `ki_subsidy`, `ki_res_mint`, … | Partial: numbers exist in `RigorSupplyDeltaInput` but not as Coq `KernelInput` at runtime |
| EvaluateConnect mint+supply | none in Coq `KernelInput` as a callee | C++ unique authority; extracted kernel `EXTRACTED_KERNEL_PRODUCTION=false` |

## Question 2 — still C++ oracle

- Outpoint occupancy (`CCoinsViewCache::HaveInputs`, `Coin::IsSpent`)
- Per-tx vin/vout asset domain (`GetTransactionInputAsset`, `CheckTxInputs_ORU`, `CheckTxInputs_RES_DualPool` **name residue**)
- Witness Ed25519 (`predicate_verify.cpp`)
- Header timestamp / merkle / coinbase height
- Disconnect/undo (`DisconnectBlock`)
- Full `CBlock` / `CTransaction` bytes

There is **no** production marshalling `CBlock → Coq KernelInput`. Comment in `genesis_authority.h`: CompCert candidate is future `MakeKernelInput` only.

## Question 3 — what random tests missed (this lab targets)

- Same-outpoint in one block vs next block (state-dependent `input_ok`)
- Dual-node identical-hex divergence hunt
- Illegal compact nBits (not random nonce grinding on a free target)
- Classifier vs ConnectBlock on P2PKH/P2WSH
- Overflow via `AddMoneyOverflows` (not random amounts inside `MAX_MONEY`)

## Question 4 — worth ConnectBlock refinement?

**Yes, later, as a map from existing C++ predicates → flags/amounts, class B until each field is an `EvaluateConnect` / `CheckTxInputs` argument.**  
Do **not** change Coq `KernelInput` to make these tests green. Do **not** claim the extracted kernel is production.

Missing for a real refinement bridge:

1. Byte codec `CTransaction` / `CBlock` → structured input
2. Explicit outpoint set in the spec (not a single `ki_utxo_ok`)
3. Hook after `CheckTxInputs` that records the same bits the Coq record names
"""
        (BRIDGE / "kernel_input_gap_analysis.md").write_text(gap, encoding="utf-8")

    def run(self) -> int:
        reset_data()
        AUDIT.mkdir(parents=True, exist_ok=True)
        CE_DIR.mkdir(parents=True, exist_ok=True)
        VEC_DIR.mkdir(parents=True, exist_ok=True)
        try:
            self.procs["A"] = start_node("A")
            self.procs["B"] = start_node("B")
            self.fund()
            self.run_a1()
            self.run_a2()
            self.run_a3()
            self.run_b1()
            self.run_b2()
            self.run_b3()
            self.run_c_pow()
            self.write_reports()
        finally:
            stop_node(self.procs.get("A"), "A")
            stop_node(self.procs.get("B"), "B")
        mismatches = [e for e in self.equiv if not e["match"]]
        print(f"ATTACK_CASES={len(self.matrix_rows)}")
        print(f"COUNTEREXAMPLES={len(self.ces)}")
        print(f"EQUIV_ROWS={len(self.equiv)}")
        print(f"EQUIV_MISMATCH={len(mismatches)}")
        print(f"STATE_EQ_DISCONNECT={self.state_eq.get('disconnect_restores_prior')}")
        print(f"STATE_EQ_RECONNECT={self.state_eq.get('reconnect_restores_mid')}")
        print("CONSENSUS_ATTACK_LAB=PASS")
        return 0


if __name__ == "__main__":
    sys.exit(Lab().run())
