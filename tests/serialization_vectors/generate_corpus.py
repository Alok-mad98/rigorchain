#!/usr/bin/env python3
"""Build byte corpus + malleability report from live C++ and frozen hex.

Does not change wire format, ConnectBlock, or Coq KernelInput.
"""
from __future__ import annotations

import json
import os
import subprocess
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any, Optional

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))

from codec import (  # noqa: E402
    header_summary,
    noncanonical_vtx_count,
    superfluous_witness_tx,
    try_decode_block,
    try_decode_tx,
    tx_summary,
)

RIGORD = Path(os.environ.get("RIGORD", str(ROOT / "core" / "src" / "rigord")))
RIGOR_CLI = Path(os.environ.get("RIGOR_CLI", str(ROOT / "core" / "src" / "rigor-cli")))
OUT = Path(__file__).resolve().parent
CASES = OUT / "cases"
AUDIT = ROOT / "audit"
EQ = AUDIT / "security" / "consensus_equivalence_dataset.json"
CONNECT_VEC = ROOT / "formal_verification" / "vectors" / "rigor_connectblock_vectors.json"

RPC_USER = "servec"
RPC_PASS = "servec-local"


def _rpc(port: int, method: str, *params: Any, wallet: Optional[str] = None) -> Any:
    path = f"/wallet/{wallet}" if wallet else "/"
    payload = json.dumps({"jsonrpc": "1.0", "id": "sv", "method": method, "params": list(params)}).encode()
    req = urllib.request.Request(f"http://127.0.0.1:{port}{path}", data=payload, method="POST")
    mgr = urllib.request.HTTPPasswordMgrWithDefaultRealm()
    mgr.add_password(None, f"http://127.0.0.1:{port}/", RPC_USER, RPC_PASS)
    opener = urllib.request.build_opener(urllib.request.HTTPBasicAuthHandler(mgr))
    try:
        with opener.open(req, timeout=60) as resp:
            body = json.loads(resp.read().decode())
    except urllib.error.HTTPError as exc:
        raw = exc.read().decode()
        try:
            body = json.loads(raw)
        except json.JSONDecodeError as err:
            raise RuntimeError(f"{method} HTTP {exc.code}: {raw}") from err
    if body.get("error"):
        raise RuntimeError(body["error"])
    return body["result"]


def _start(network: str, datadir: Path, p2p: int, rpc: int) -> subprocess.Popen:
    datadir.mkdir(parents=True, exist_ok=True)
    log = (datadir / "stderr.log").open("w")
    args = [
        str(RIGORD),
        f"-{network}",
        f"-datadir={datadir}",
        "-bind=127.0.0.1",
        f"-port={p2p}",
        f"-rpcport={rpc}",
        f"-rpcuser={RPC_USER}",
        f"-rpcpassword={RPC_PASS}",
        "-rpcbind=127.0.0.1",
        "-rpcallowip=127.0.0.1",
        "-server=1",
        "-listen=1",
        "-dnsseed=0",
        "-discover=0",
        "-listenonion=0",
        "-printtoconsole=0",
    ]
    proc = subprocess.Popen(args, cwd=str(ROOT), stdout=subprocess.DEVNULL, stderr=log)
    deadline = time.time() + 60
    last = None
    while time.time() < deadline:
        if proc.poll() is not None:
            raise RuntimeError(f"{network} exited: {(datadir / 'stderr.log').read_text()[-1500:]}")
        try:
            _rpc(rpc, "getblockcount")
            return proc
        except Exception as exc:  # noqa: BLE001
            last = exc
            time.sleep(0.2)
    raise RuntimeError(f"{network} RPC not ready: {last}")


def _stop(proc: Optional[subprocess.Popen], rpc: int) -> None:
    try:
        _rpc(rpc, "stop")
    except Exception:
        pass
    if proc is None:
        return
    try:
        proc.wait(timeout=15)
    except subprocess.TimeoutExpired:
        proc.kill()


def _kernel(decision: str, layer: str, **flags: Any) -> dict:
    if decision == "decode_fail":
        return {"status": "not_reached", "note": "DecodeHexBlk / UnserializeTransaction failed; no KernelInput candidate."}
    candidate = {
        "status": "oracle_only",
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
        "note": "Coq KernelInput is not a C++ argument. Flags below are a future map only.",
    }
    candidate.update(flags)
    if layer in ("CheckTxInputs",) and decision == "reject":
        candidate["ki_utxo_ok"] = False
    if layer in ("ClassifyRigorAsset", "VerifyWitnessProgram") and decision == "reject":
        candidate["ki_script_ok"] = False
    if layer in ("CheckProofOfWork", "ContextualCheckBlockHeader") and decision == "reject":
        candidate["ki_pow_ok"] = False
        if "asert" in str(flags.get("note", "")).lower() or layer == "ContextualCheckBlockHeader":
            candidate["ki_asert_ok"] = False
    if decision == "accept" and layer in ("ConnectBlock", "EvaluateConnect", "CheckBlock"):
        candidate["ki_utxo_ok"] = True
        candidate["ki_script_ok"] = True
        candidate["ki_pow_ok"] = True
        candidate["ki_asert_ok"] = True
    return candidate


def _case(
    case_id: str,
    raw_hex: str,
    kind: str,
    source: str,
    expected: str,
    consensus_result: str,
    validation_layer: str,
    validation_function: str,
    reject_reason: Optional[str],
    future_theorem: Optional[str],
    extra: Optional[dict] = None,
) -> dict:
    if kind == "block":
        obj, err, leftover, roundtrip = try_decode_block(raw_hex)
        fields = header_summary(obj) if obj else {}
        cpp_type = "CBlock"
        entry = "DecodeHexBlk"
    else:
        obj, err, leftover, roundtrip = try_decode_tx(raw_hex)
        fields = tx_summary(obj) if obj else {}
        cpp_type = "CTransaction"
        entry = "UnserializeTransaction"
    decode_ok = err is None
    decision = "decode_fail" if not decode_ok else consensus_result
    row = {
        "case_id": case_id,
        "raw_block_hex": raw_hex if kind == "block" else None,
        "raw_tx_hex": raw_hex if kind == "tx" else None,
        "kind": kind,
        "source": source,
        "decode_result": {
            "status": "ok" if decode_ok else "fail",
            "cpp_type": cpp_type if decode_ok else None,
            "error": err,
            "fields": fields,
            "leftover_bytes": leftover if decode_ok else 0,
            "roundtrip_equal": roundtrip,
        },
        "consensus_result": decision,
        "expected": expected,
        "validation_rule": {
            "layer": "serialization" if not decode_ok else validation_layer,
            "function": entry if not decode_ok else validation_function,
            "reject_reason": err if not decode_ok else reject_reason,
        },
        "kernel_candidate": _kernel(
            decision,
            "serialization" if not decode_ok else validation_layer,
        ),
        "future_theorem": future_theorem,
        "trace": {
            "raw_bytes": {"hex": raw_hex, "source": source, "entry_function": entry},
            "decoded_object": {
                "status": "ok" if decode_ok else "fail",
                "cpp_type": cpp_type if decode_ok else None,
                "error": err,
                "fields": fields,
                "leftover_bytes": leftover if decode_ok else 0,
                "roundtrip_equal": roundtrip,
            },
            "validation_rule": {
                "layer": "serialization" if not decode_ok else validation_layer,
                "function": entry if not decode_ok else validation_function,
                "reject_reason": err if not decode_ok else reject_reason,
            },
            "decision": decision,
            "kernel_candidate": _kernel(
                decision,
                "serialization" if not decode_ok else validation_layer,
            ),
            "future_theorem": future_theorem,
        },
    }
    if extra:
        row.update(extra)
    return row


def _blk_records(path: Path) -> list[bytes]:
    raw = path.read_bytes()
    off = 0
    blocks: list[bytes] = []
    while off + 8 <= len(raw):
        size = int.from_bytes(raw[off + 4 : off + 8], "little")
        if size == 0 or off + 8 + size > len(raw):
            break
        blocks.append(raw[off + 8 : off + 8 + size])
        off += 8 + size
    return blocks


def existing_testnet_mined() -> list[dict]:
 """ lab datadir, if present. Gitignored; optional source."""
    path = ROOT / "testnet" / "data" / "node-001" / "testnet3" / "blocks" / "blk00000.dat"
    if not path.is_file():
        return []
    blocks = _blk_records(path)
    rows = []
    if len(blocks) >= 2:
        rows.append(
            _case(
                "SV-TESTNET-H1",
                blocks[1].hex(),
                "block",
                "testnet_getblock",
                "accept",
                "accept",
                "ConnectBlock",
                "ProcessNewBlock / ConnectBlock",
                None,
                "decode_then_connect_preserves_header",
                extra={"network": "testnet", "height": 1, "source_file": str(path.relative_to(ROOT))},
            )
        )
    return rows


def live_blocks() -> list[dict]:
    rows = []
    data = OUT / "data"
    if data.exists():
        subprocess.run(["rm", "-rf", str(data)], check=False)
    data.mkdir(parents=True, exist_ok=True)

    rt = None
    tn = None
    try:
        rt = _start("regtest", data / "regtest", 38590, 38591)
        genesis = _rpc(38591, "getblock", _rpc(38591, "getblockhash", 0), 0)
        rows.append(
            _case(
                "SV-GENESIS-REGTEST",
                genesis,
                "block",
                "genesis",
                "accept",
                "accept",
                "ConnectBlock",
                "ProcessNewBlock / ConnectBlock",
                None,
                "decode_then_connect_preserves_header",
                extra={"network": "regtest", "height": 0},
            )
        )
        _rpc(38591, "createwallet", "sv")
        addr = _rpc(38591, "getnewaddress", wallet="sv")
        hashes = _rpc(38591, "generatetoaddress", 1, addr)
        mined = _rpc(38591, "getblock", hashes[0], 0)
        rows.append(
            _case(
                "SV-MINED-REGTEST-H1",
                mined,
                "block",
                "regtest_getblock",
                "accept",
                "accept",
                "ConnectBlock",
                "ProcessNewBlock / ConnectBlock",
                None,
                "decode_then_connect_preserves_header",
                extra={"network": "regtest", "height": 1},
            )
        )
        tn = _start("testnet", data / "testnet", 38600, 38601)
        tgenesis = _rpc(38601, "getblock", _rpc(38601, "getblockhash", 0), 0)
        tip = _rpc(38601, "getblockcount")
        rows.append(
            _case(
                "SV-GENESIS-TESTNET",
                tgenesis,
                "block",
                "genesis",
                "accept",
                "accept",
                "ConnectBlock",
                "ProcessNewBlock / ConnectBlock",
                None,
                "decode_then_connect_preserves_header",
                extra={"network": "testnet", "height": 0, "local_tip": tip},
            )
        )
        if tip >= 1:
            mined_tn = _rpc(38601, "getblock", _rpc(38601, "getblockhash", 1), 0)
            rows.append(
                _case(
                    "SV-TESTNET-H1",
                    mined_tn,
                    "block",
                    "testnet_getblock",
                    "accept",
                    "accept",
                    "ConnectBlock",
                    "ProcessNewBlock / ConnectBlock",
                    None,
                    "decode_then_connect_preserves_header",
                    extra={"network": "testnet", "height": 1},
                )
            )
    finally:
        _stop(rt, 38591)
        _stop(tn, 38601)
    return rows


TX_RULES = {
    "A1_connectblock_double_spend": (
        "CheckTxInputs",
        "Consensus::CheckTxInputs",
        "bad-txns-inputs-missingorspent",
        "no_double_spend",
        "reject",
    ),
    "B1_output_over_max_money": (
        "CheckTransaction",
        "CheckTransaction",
        "bad-txns-vout-toolarge",
        "no_uint64_amount_wrap",
        "reject",
    ),
    "B1_output_negative": (
        "CheckTransaction",
        "CheckTransaction",
        "bad-txns-vout-negative",
        "no_uint64_amount_wrap",
        "reject",
    ),
    "B1_output_sum_overflow": (
        "CheckTransaction",
        "CheckTransaction",
        "bad-txns-txouttotal-toolarge",
        "no_uint64_amount_wrap",
        "reject",
    ),
    "B2_p2pkh": (
        "ClassifyRigorAsset",
        "ClassifyRigorTxout",
        "bad-rigor-script-type",
        "only_allowed_script_shapes_enter_utxo",
        "reject",
    ),
    "B2_p2wsh": (
        "ClassifyRigorAsset",
        "ClassifyRigorTxout",
        "bad-rigor-script-type",
        "only_allowed_script_shapes_enter_utxo",
        "reject",
    ),
    "B2_empty_script": (
        "ClassifyRigorAsset",
        "ClassifyRigorTxout",
        "bad-rigor-script-type",
        "only_allowed_script_shapes_enter_utxo",
        "reject",
    ),
    "B2_op_return": (
        "CheckTransaction",
        "CheckTransaction / coinbase length",
        "bad-cb-length",
        "only_allowed_script_shapes_enter_utxo",
        "reject",
    ),
    "B3_empty_or_unsigned_witness": (
        "VerifyWitnessProgram",
        "VerifyWitnessProgram",
        "Witness program hash mismatch",
        "witness_is_ed25519_64_and_32",
        "reject",
    ),
    "B3_mutated_witness_or_tx": (
        "VerifyWitnessProgram",
        "predicate_verify.cpp",
        "RIGOR_RESERVED_LEGACY_EQUALVERIFY",
        "witness_is_ed25519_64_and_32",
        "reject",
    ),
    "C2_illegal_nbits_zero": (
        "CheckProofOfWork",
        "CheckProofOfWork",
        "high-hash / bad-diffbits",
        "pow_compact_in_range_and_header_time_bounded",
        "reject",
    ),
    "C2_illegal_nbits_overflow": (
        "CheckProofOfWork",
        "CheckProofOfWork",
        "high-hash / bad-diffbits",
        "pow_compact_in_range_and_header_time_bounded",
        "reject",
    ),
    "C2_target_above_powlimit": (
        "CheckProofOfWork",
        "CheckProofOfWork",
        "high-hash / bad-diffbits",
        "pow_compact_in_range_and_header_time_bounded",
        "reject",
    ),
    "C2_future_timestamp": (
        "ContextualCheckBlockHeader",
        "ContextualCheckBlockHeader",
        "time-too-new",
        "pow_compact_in_range_and_header_time_bounded",
        "reject",
    ),
    "C2_nonce_flip": (
        "CheckProofOfWork",
        "CheckProofOfWork",
        "high-hash (regtest may still accept)",
        "pow_compact_in_range_and_header_time_bounded",
        "reject",
    ),
    "C1_truncated_block": (
        "serialization",
        "DecodeHexBlk",
        "truncated header",
        "decode_rejects_truncated_block",
        "decode_fail",
    ),
}


def frozen_rows() -> list[dict]:
    rows = []
    eq = json.loads(EQ.read_text())
    for item in eq:
        name = item["case"]
        raw = item["input"]
        layer, fn, reason, theorem, expected = TX_RULES[name]
        kind = "block" if name.startswith("C") else "tx"
        if name == "A1_connectblock_double_spend":
            hexs = json.loads(raw)
            raw = hexs[0]
            kind = "tx"
        rows.append(
            _case(
                f"SV-{name}",
                raw,
                kind,
                "attack_lab",
                expected if expected != "decode_fail" else "reject",
                item.get("node_a", "reject"),
                layer,
                fn,
                reason,
                theorem,
            )
        )
    if CONNECT_VEC.is_file():
        for item in json.loads(CONNECT_VEC.read_text()):
            expect = "accept" if item.get("expect") else "reject"
            rows.append(
                _case(
                    f"SV-FV-{item['name']}",
                    item["block_hex"],
                    "block",
                    "constructed",
                    expect,
                    expect,
                    "CheckBlock" if not item.get("expect") else "ConnectBlock",
                    item.get("reason") or "ConnectBlock vector",
                    item.get("reason"),
                    "merkle_root_matches_vtx" if not item.get("expect") else "decode_then_connect_preserves_header",
                )
            )
    return rows


def synthetic_rows(legal_block_hex: str) -> list[dict]:
    rows = []
    nc = noncanonical_vtx_count(legal_block_hex)
    rows.append(
        _case(
            "SV-NONCANONICAL-COMPACTSIZE-VTX",
            nc,
            "block",
            "constructed",
            "reject",
            "reject",
            "serialization",
            "ReadCompactSize",
            "non-canonical ReadCompactSize()",
            "canonical_compactsize_unique",
        )
    )
    trailing = legal_block_hex + "00"
    rows.append(
        _case(
            "SV-TRAILING-BYTE-AFTER-BLOCK",
            trailing,
            "block",
            "constructed",
            "observe",
            "accept",
            "serialization",
            "DecodeHexBlk",
            "leftover byte not checked by DecodeHexBlk",
            "decode_hex_blk_consumes_prefix_only",
        )
    )
    rows.append(
        _case(
            "SV-SUPERFLUOUS-WITNESS-RECORD",
            superfluous_witness_tx(),
            "tx",
            "constructed",
            "reject",
            "reject",
            "serialization",
            "UnserializeTransaction",
            "Superfluous witness record",
            "no_empty_witness_marker",
        )
    )
    rows.append(
        _case(
            "SV-TRUNCATED-HEADER",
            "00000020ca16f2253c7e45c5af9ed49c68144112",
            "block",
            "attack_lab",
            "reject",
            "reject",
            "serialization",
            "DecodeHexBlk",
            "truncated header",
            "decode_rejects_truncated_block",
        )
    )
    return rows


def write_malleability(rows: list[dict]) -> None:
    legal = []
    fail_decode = []
    leftover = []
    unequal = []
    for row in rows:
        dec = row["decode_result"]
        if dec["status"] != "ok":
            fail_decode.append(row)
            continue
        if dec.get("leftover_bytes"):
            leftover.append(row)
        if dec.get("roundtrip_equal") is False:
            unequal.append(row)
        elif dec.get("roundtrip_equal") is True and not dec.get("leftover_bytes"):
            legal.append(row)

    lines = [
 "# Serialization malleability ",
        "",
        "Check: `encode(decode(bytes)) == original` using the C++ rules in `serialize.h` / `transaction.h` / `block.h`.",
        "",
        "Python codec: `tests/serialization_vectors/codec.py` (canonical CompactSize + witness rules).",
        "",
        "## Legal inputs (must round-trip)",
        "",
        f"Checked **{len(legal)}** fully consumed legal encodings. All of them satisfy `encode(decode(H)) == H`.",
        "",
        "| case_id | kind | source |",
        "|---|---|---|",
    ]
    for row in legal:
        lines.append(f"| {row['case_id']} | {row['kind']} | {row['source']} |")
    lines += [
        "",
        "## Non-canonical / decode-fail (never become consensus objects)",
        "",
        "`ReadCompactSize` throws `non-canonical ReadCompactSize()`.",
        "`UnserializeTransaction` throws `Superfluous witness record`.",
        "Truncated streams fail before `ProcessNewBlock`.",
        "",
        "| case_id | error |",
        "|---|---|",
    ]
    for row in fail_decode:
        lines.append(f"| {row['case_id']} | {row['decode_result'].get('error')} |")
    lines += [
        "",
        "## Prefix decode (DecodeHexBlk leftover)",
        "",
        "`DecodeHexBlk` (`core_read.cpp` L220–235) does **not** require the stream to be empty.",
        "Trailing bytes after a complete `CBlock` still return `true`. Consensus then uses the **object**, not the extra bytes.",
        "Therefore `encode(decode(bytes)) != original` when leftover > 0, even though the object is canonical.",
        "",
        "| case_id | leftover_bytes | roundtrip_equal |",
        "|---|---|---|",
    ]
    for row in leftover:
        dec = row["decode_result"]
        lines.append(f"| {row['case_id']} | {dec['leftover_bytes']} | {dec['roundtrip_equal']} |")
    lines += [
        "",
        "## Other non-roundtrip (should be empty for legal full consumes)",
        "",
    ]
    if not unequal:
        lines.append("None besides leftover-prefix cases.")
    else:
        lines.append("| case_id | leftover | note |")
        lines.append("|---|---|---|")
        for row in unequal:
            dec = row["decode_result"]
            lines.append(f"| {row['case_id']} | {dec.get('leftover_bytes')} | object re-encodes to canonical form |")
    lines += [
        "",
        "## Consensus-used vs malleable",
        "",
        "| Channel | Affects consensus object? |",
        "|---|---|",
        "| Non-canonical CompactSize | No — decode throws |",
        "| Superfluous empty witness | No — decode throws |",
        "| Truncated header / tx | No — decode fails |",
        "| Trailing junk after a complete block | **Object unchanged**; extra bytes dropped |",
        "| Witness present vs stripped | Different wtxid; txid is no-witness hash |",
        "| `Coin` VARINT on disk | Different codec; not `submitblock` hex |",
        "",
        "VARINT is **not** a block/tx wire encoding. It is UTXO disk (`coins.h`).",
        "",
        "## Future theorems (not proved here)",
        "",
        "- `canonical_compactsize_unique`",
        "- `no_empty_witness_marker`",
        "- `decode_rejects_truncated_block`",
        "- `decode_hex_blk_consumes_prefix_only` (document leftover, or later tighten C++)",
        "- `decode_then_connect_preserves_header`",
        "",
    ]
    path = AUDIT / "security" / "serialization_malleability.md"
    path.write_text("\n".join(lines) + "\n")


def main() -> int:
    if not RIGORD.is_file():
        raise SystemExit(f"missing {RIGORD}")
    CASES.mkdir(parents=True, exist_ok=True)
    rows = []
    rows.extend(live_blocks())
    rows.extend(existing_testnet_mined())
    legal_block = next(r["raw_block_hex"] for r in rows if r["case_id"] == "SV-MINED-REGTEST-H1")
    rows.extend(frozen_rows())
    rows.extend(synthetic_rows(legal_block))

    manifest = []
    for row in rows:
        (CASES / f"{row['case_id']}.json").write_text(json.dumps(row, indent=2) + "\n")
        manifest.append(
            {
                "case_id": row["case_id"],
                "kind": row["kind"],
                "source": row["source"],
                "decode": row["decode_result"]["status"],
                "roundtrip_equal": row["decode_result"]["roundtrip_equal"],
                "consensus_result": row["consensus_result"],
                "expected": row["expected"],
            }
        )
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    write_malleability(rows)

    legal_ok = sum(
        1
        for r in rows
        if r["decode_result"]["status"] == "ok"
        and not r["decode_result"].get("leftover_bytes")
        and r["decode_result"].get("roundtrip_equal") is True
    )
    legal_bad = [
        r["case_id"]
        for r in rows
        if r["decode_result"]["status"] == "ok"
        and not r["decode_result"].get("leftover_bytes")
        and r["decode_result"].get("roundtrip_equal") is not True
    ]
    print(f"cases={len(rows)} legal_roundtrip={legal_ok} legal_fail={legal_bad}")
    if legal_bad:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
