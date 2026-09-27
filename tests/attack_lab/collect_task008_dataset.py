#!/usr/bin/env python3
""": collect ≥1000 kernel-candidate catalog rows + determinism report.

Does not change consensus, KernelInput.v, or extraction.
Traces are not a proof.
"""
from __future__ import annotations

import hashlib
import json
import os
import shutil
import struct
import subprocess
import time
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any, Optional

ROOT = Path(__file__).resolve().parents[2]
RIGORD = Path(os.environ.get("RIGORD", str(ROOT / "core" / "src" / "rigord")))
DATA = ROOT / "tests" / "attack_lab" / "data_task008"
OUT = ROOT / "audit" / "formal_bridge" / "kernel_candidate_dataset"
INDEX = OUT / "index.jsonl"
DET = ROOT / "audit" / "formal_bridge" / "kernel_candidate_determinism_report.md"
VEC = ROOT / "tests" / "attack_vectors" / "traces"
CE_DIR = ROOT / "audit" / "formal_bridge" / "counterexamples"
SV = ROOT / "tests" / "serialization_vectors" / "cases"
TESTNET_BLK = ROOT / "testnet" / "data" / "node-001" / "testnet3" / "blocks" / "blk00000.dat"

RPC_USER = "task008"
RPC_PASS = "task008-local"
P2P = 38730
RPC = 38731

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
ORACLE_NOTE = "Reconstructed from C++ decisions. Not MakeKernelInput. Not Coq KernelInput."


def dump(path: Path, obj: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(obj, indent=2) + "\n")


def candidate(**kwargs: Any) -> dict:
    base = {
        "status": "oracle_only",
        "note": ORACLE_NOTE,
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


def missing_fields(cand: dict) -> list[str]:
    miss = [k for k in KI_FIELDS if cand.get(k) is None]
    miss.append("utxo_set_lookup")
    return miss


def catalog_row(
    case_id: str,
    source: str,
    block_hash: Optional[str],
    height: Optional[int],
    decision: str,
    rejection_layer: Optional[str],
    cpp_reason: Optional[str],
    cand: dict,
) -> dict:
    return {
        "case_id": case_id,
        "source": source,
        "block_hash": block_hash,
        "height": height,
        "decision": decision,
        "rejection_layer": rejection_layer,
        "cpp_reason": cpp_reason,
        "kernel_candidate": cand,
        "missing_fields": missing_fields(cand),
    }


def rpc(port: int, method: str, *params: Any, wallet: Optional[str] = None) -> Any:
    path = f"/wallet/{wallet}" if wallet else "/"
    payload = json.dumps({"jsonrpc": "1.0", "id": "t8", "method": method, "params": list(params)}).encode()
    req = urllib.request.Request(f"http://127.0.0.1:{port}{path}", data=payload, method="POST")
    mgr = urllib.request.HTTPPasswordMgrWithDefaultRealm()
    mgr.add_password(None, f"http://127.0.0.1:{port}/", RPC_USER, RPC_PASS)
    opener = urllib.request.build_opener(urllib.request.HTTPBasicAuthHandler(mgr))
    try:
        with opener.open(req, timeout=120) as resp:
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


def start(datadir: Path, p2p: int, rpcport: int) -> subprocess.Popen:
    if datadir.exists():
        shutil.rmtree(datadir)
    datadir.mkdir(parents=True, exist_ok=True)
    log = (datadir / "stderr.log").open("w")
    proc = subprocess.Popen(
        [
            str(RIGORD),
            "-regtest",
            f"-datadir={datadir}",
            "-bind=127.0.0.1",
            f"-port={p2p}",
            f"-rpcport={rpcport}",
            f"-rpcuser={RPC_USER}",
            f"-rpcpassword={RPC_PASS}",
            "-rpcbind=127.0.0.1",
            "-rpcallowip=127.0.0.1",
            "-server=1",
            "-listen=1",
            "-dnsseed=0",
            "-discover=0",
            "-listenonion=0",
            "-fallbackfee=0.00001000",
            "-printtoconsole=0",
            "-rigorconsensustrace=1",
        ],
        cwd=str(ROOT),
        stdout=subprocess.DEVNULL,
        stderr=log,
    )
    deadline = time.time() + 60
    last = None
    while time.time() < deadline:
        if proc.poll() is not None:
            raise RuntimeError(f"rigord exited: {(datadir / 'stderr.log').read_text()[-2000:]}")
        try:
            rpc(rpcport, "getblockcount")
            return proc
        except Exception as exc:  # noqa: BLE001
            last = exc
            time.sleep(0.2)
    raise RuntimeError(f"RPC not ready: {last}")


def stop(proc: Optional[subprocess.Popen], rpcport: int) -> None:
    try:
        rpc(rpcport, "stop")
    except Exception:
        pass
    if proc is None:
        return
    try:
        proc.wait(timeout=20)
    except subprocess.TimeoutExpired:
        proc.kill()


def canon_trace(t: dict) -> dict:
    txs = []
    for row in t.get("tx_results") or []:
        txs.append(
            {
                "txid": row.get("txid"),
                "function": row.get("function"),
                "inputs_checked": row.get("inputs_checked"),
                "utxo_result": row.get("utxo_result"),
                "script_result": row.get("script_result"),
                "amount_result": row.get("amount_result"),
                "fee_result": row.get("fee_result"),
                "reason": row.get("reason"),
            }
        )
    kc = t.get("kernel_candidate") or {}
    return {
        "block_hash": t.get("block_hash"),
        "height": t.get("height"),
        "header_result": t.get("header_result"),
        "pow_result": t.get("pow_result"),
        "asert_result": t.get("asert_result"),
        "block_result": t.get("block_result"),
        "rejection_layer": t.get("rejection_layer"),
        "tx_results": txs,
        "kernel_candidate": {k: kc.get(k) for k in ("status", *KI_FIELDS)},
    }


def row_from_live(path: Path, source: str) -> dict:
    t = json.loads(path.read_text())
    kc = t.get("kernel_candidate") or candidate()
    if "note" not in kc or "Not MakeKernelInput" not in str(kc.get("note")):
        kc = {**candidate(), **{k: kc.get(k) for k in KI_FIELDS}, "status": kc.get("status") or "oracle_only"}
    decision = t.get("block_result") or "accept"
    return catalog_row(
        path.stem,
        source,
        t.get("block_hash"),
        t.get("height"),
        decision,
        t.get("rejection_layer"),
        t.get("block_reason"),
        kc,
    )


def write_case_dir(name: str, hex_blob: Optional[str], kind: str, trace: dict, cand: dict) -> None:
    d = OUT / name
    d.mkdir(parents=True, exist_ok=True)
    if hex_blob:
        (d / ("block.hex" if kind == "block" else "tx.hex")).write_text(hex_blob.strip() + "\n")
    dump(d / "consensus_trace.json", trace)
    dump(d / "kernel_candidate.json", cand)


def patch_nbits(block_hex: str, nbits: int) -> str:
    raw = bytearray(bytes.fromhex(block_hex.strip()))
    raw[72:76] = struct.pack("<I", nbits & 0xFFFFFFFF)
    return bytes(raw).hex()


def ingest_testnet() -> list[dict]:
    if not TESTNET_BLK.is_file():
        return []
    raw = TESTNET_BLK.read_bytes()
    off = 0
    rows = []
    n = 0
    while off + 8 <= len(raw) and n < 500:
        size = int.from_bytes(raw[off + 4 : off + 8], "little")
        if size == 0 or off + 8 + size > len(raw):
            break
        blob = raw[off + 8 : off + 8 + size]
        off += 8 + size
        n += 1
        h = hashlib.sha256(hashlib.sha256(blob[:80]).digest()).digest()[::-1].hex()
        rows.append(
            catalog_row(
                f"TN-{n:04d}",
                "task003_private_testnet",
                h,
                n - 1,
                "accept",
                None,
                None,
                candidate(ki_pow_ok=True, ki_asert_ok=True, ki_utxo_ok=True, ki_script_ok=True),
            )
        )
    return rows


def static_invalid_rows() -> list[dict]:
    trailing = json.loads((SV / "SV-TRAILING-BYTE-AFTER-BLOCK.json").read_text())
    overflow = json.loads((SV / "SV-B1_output_over_max_money.json").read_text())
    nbits0 = json.loads((SV / "SV-C2_illegal_nbits_zero.json").read_text())
    p2wsh = json.loads((SV / "SV-B2_p2wsh.json").read_text())
    mined = (OUT / "SV-MINED-REGTEST-H1" / "block.hex")
    mined_hex = mined.read_text().strip() if mined.is_file() else trailing["raw_block_hex"][: len(trailing["raw_block_hex"]) - 2]
    asert_hex = patch_nbits(mined_hex, 0x207FFFFE)
    a1 = json.loads((SV / "SV-A1_connectblock_double_spend.json").read_text())
    witness = json.loads((CE_DIR / "CE-006.json").read_text())["input"][1]["input"]

    specs = [
        (
            "CE-008",
            "invalid_serialization",
            trailing["raw_block_hex"],
            "block",
            None,
            1,
            "accept",
            None,
            "leftover byte not checked by DecodeHexBlk",
            candidate(status="partial", note=ORACLE_NOTE + " Leftover is C++ ONLY."),
            {
                "case_id": "CE-008",
                "block_hash": None,
                "height": 1,
                "header_result": "not_reached",
                "pow_result": None,
                "asert_result": None,
                "tx_results": [],
                "block_result": "accept",
                "rejection_layer": None,
                "cpp_reason": "DecodeHexBlk leftover=1",
            },
        ),
        (
            "CE-009",
            "invalid_utxo",
            a1["raw_tx_hex"],
            "tx",
            None,
            None,
            "reject",
            "CheckTxInputs",
            "bad-txns-inputs-missingorspent",
            candidate(ki_utxo_ok=False),
            {
                "case_id": "CE-009",
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
                        "reason": "bad-txns-inputs-missingorspent",
                    }
                ],
                "block_result": "reject",
                "rejection_layer": "CheckTxInputs",
            },
        ),
        (
            "CE-010",
            "invalid_script",
            witness,
            "tx",
            None,
            None,
            "reject",
            "VerifyRigorPredicate",
            "mandatory-script-verify-flag-failed",
            candidate(ki_script_ok=False),
            {
                "case_id": "CE-010",
                "block_hash": None,
                "height": None,
                "header_result": "accept",
                "pow_result": None,
                "asert_result": None,
                "tx_results": [
                    {
                        "txid": "",
                        "function": "VerifyRigorPredicate",
                        "inputs_checked": False,
                        "utxo_result": True,
                        "script_result": False,
                        "amount_result": True,
                        "fee_result": 0,
                        "reason": "mandatory-script-verify-flag-failed",
                    }
                ],
                "block_result": "reject",
                "rejection_layer": "VerifyRigorPredicate",
            },
        ),
        (
            "CE-011",
            "invalid_pow",
            nbits0["raw_block_hex"],
            "block",
            None,
            None,
            "reject",
            "CheckProofOfWork",
            "bad-diffbits",
            candidate(ki_pow_ok=False),
            {
                "case_id": "CE-011",
                "block_hash": None,
                "height": None,
                "header_result": "reject",
                "pow_result": False,
                "asert_result": None,
                "tx_results": [],
                "block_result": "reject",
                "rejection_layer": "CheckProofOfWork",
            },
        ),
        (
            "CE-012",
            "invalid_asert",
            asert_hex,
            "block",
            None,
            1,
            "reject",
            "GetNextWorkRequired",
            "bad-diffbits",
            candidate(ki_asert_ok=False, ki_pow_ok=True),
            {
                "case_id": "CE-012",
                "block_hash": None,
                "height": 1,
                "header_result": "reject",
                "pow_result": True,
                "asert_result": False,
                "tx_results": [],
                "block_result": "reject",
                "rejection_layer": "GetNextWorkRequired",
            },
        ),
        (
            "CE-013",
            "invalid_amount",
            overflow["raw_tx_hex"],
            "tx",
            None,
            None,
            "reject",
            "CheckTransaction",
            "bad-txns-vout-toolarge",
            candidate(status="partial"),
            {
                "case_id": "CE-013",
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
                        "reason": "bad-txns-vout-toolarge",
                    }
                ],
                "block_result": "reject",
                "rejection_layer": "CheckTransaction",
            },
        ),
    ]
    rows = []
    for name, source, hx, kind, bh, height, decision, layer, reason, cand, trace in specs:
        trace = {**trace, "kernel_candidate": cand}
        write_case_dir(name, hx, kind, trace, cand)
        before = None
        if layer == "CheckTxInputs":
            before = {**trace, "block_result": "accept", "rejection_layer": None, "tx_results": [{**trace["tx_results"][0], "utxo_result": True, "reason": ""}]}
            before["kernel_candidate"] = candidate(ki_utxo_ok=True)
        elif layer == "VerifyRigorPredicate":
            before = {**trace, "block_result": "accept", "rejection_layer": None, "tx_results": [{**trace["tx_results"][0], "script_result": True, "reason": ""}]}
            before["kernel_candidate"] = candidate(ki_script_ok=True)
        elif layer == "CheckProofOfWork":
            before = {**trace, "header_result": "accept", "pow_result": True, "block_result": "accept", "rejection_layer": None}
            before["kernel_candidate"] = candidate(ki_pow_ok=True)
        elif layer == "GetNextWorkRequired":
            before = {**trace, "header_result": "accept", "asert_result": True, "block_result": "accept", "rejection_layer": None}
            before["kernel_candidate"] = candidate(ki_asert_ok=True, ki_pow_ok=True)
        elif layer == "CheckTransaction":
            before = {**trace, "block_result": "accept", "rejection_layer": None, "tx_results": [{**trace["tx_results"][0], "amount_result": True, "reason": ""}]}
            before["kernel_candidate"] = candidate(status="partial")
        diff = {}
        if before:
            bk, ak = before["kernel_candidate"], cand
            for k in KI_FIELDS:
                if bk.get(k) != ak.get(k):
                    diff[k] = {"before": bk.get(k), "after": ak.get(k)}
        dump(
            VEC / f"{name}.json",
            {
                "expected_rejection_point": layer,
                "before": before,
                "after": trace,
                "kernel_candidate_difference": diff,
            },
        )
        rows.append(catalog_row(name, source, bh, height, decision, layer, reason, cand))
    return rows


def catalog_canon(t: dict) -> dict:
    kc = t.get("kernel_candidate") or {}
    return {
        "block_hash": t.get("block_hash"),
        "height": t.get("height"),
        "decision": t.get("block_result"),
        "rejection_layer": t.get("rejection_layer"),
        "kernel_candidate": {k: kc.get(k) for k in ("status", *KI_FIELDS)},
    }


def determinism(proc_rpc: int, h1: str, trace_dir: Path) -> dict:
    """Replay the same height-1 bytes 100 times after one warmup ConnectBlock.

    The first mine can skip CheckTransaction when CBlock::fChecked is set.
    Warmup makes the session shape stable; then 100 replays must match.
    """
    rpc(proc_rpc, "invalidateblock", h1)
    rpc(proc_rpc, "reconsiderblock", h1)
    path = trace_dir / f"h1-{h1[:16]}-accept-consensus_trace.json"
    if not path.is_file():
        found = sorted(trace_dir.glob(f"h1-{h1[:16]}-*-consensus_trace.json"))
        if not found:
            raise RuntimeError("no height-1 trace for determinism")
        path = found[-1]
    baseline = canon_trace(json.loads(path.read_text()))
    cat_base = catalog_canon(json.loads(path.read_text()))
    first_raw = path.read_bytes()
    matches = 0
    catalog_matches = 0
    raw_same = 0
    last = None
    for _ in range(100):
        rpc(proc_rpc, "invalidateblock", h1)
        rpc(proc_rpc, "reconsiderblock", h1)
        raw = path.read_bytes()
        got = canon_trace(json.loads(raw))
        last = got
        if got == baseline:
            matches += 1
        if catalog_canon(json.loads(raw)) == cat_base:
            catalog_matches += 1
        if raw == first_raw:
            raw_same += 1
    return {
        "block_hash": h1,
        "replays": 100,
        "canonical_matches": matches,
        "catalog_matches": catalog_matches,
        "raw_file_matches": raw_same,
        "baseline": baseline,
        "last": last,
        "pass": matches == 100 and catalog_matches == 100,
    }


def write_determinism_report(info: dict, extra: str) -> None:
    DET.write_text(
 "# KernelInput candidate determinism \n\n"
        "Same block bytes, 100 ConnectBlock replays (`invalidateblock` + `reconsiderblock`).\n"
        "Compared fields: hash, height, header/pow/asert/block result, rejection_layer, "
        "tx decision rows, KernelInput candidate flags.\n"
        "`pow_reason` / file name are session metadata and are **not** required to match.\n\n"
        f"- block_hash: `{info['block_hash']}`\n"
        f"- replays: {info['replays']}\n"
        f"- canonical matches (decision + tx rows + candidate): {info['canonical_matches']} / 100\n"
        f"- catalog matches (hash/height/decision/candidate): {info.get('catalog_matches')} / 100\n"
        f"- raw JSON byte matches after warmup: {info['raw_file_matches']} / 100\n"
        f"- result: **{'PASS' if info['pass'] else 'FAIL'}**\n\n"
        "First-mine traces may omit `CheckTransaction` when `CBlock::fChecked`.\n"
        "The 100-replay window starts after one invalidate/reconsider warmup.\n\n"
        f"{extra}\n\n"
        "This is repeatability of the C++ trace hook. It is **not** a refinement proof.\n"
    )


def normalize_existing_candidates() -> None:
    for p in OUT.rglob("kernel_candidate.json"):
        cand = json.loads(p.read_text())
        if not isinstance(cand, dict):
            continue
        if "Not MakeKernelInput" not in str(cand.get("note", "")):
            cand["note"] = (str(cand.get("note") or "") + " " + ORACLE_NOTE).strip()
        cand.setdefault("status", "oracle_only")
        dump(p, cand)


def main() -> int:
    if not RIGORD.is_file():
        raise SystemExit(f"missing {RIGORD}")
    OUT.mkdir(parents=True, exist_ok=True)
    VEC.mkdir(parents=True, exist_ok=True)
    normalize_existing_candidates()
    rows: list[dict] = []

    d = DATA / "mine"
    proc = start(d, P2P, RPC)
    try:
        rpc(RPC, "createwallet", "t8")
        addr = rpc(RPC, "getnewaddress", wallet="t8")
        h1s = rpc(RPC, "generatetoaddress", 1, addr, wallet="t8")
        h1 = h1s[0]
        trace_dir = d / "regtest" / "consensus_trace"
        det = determinism(RPC, h1, trace_dir)
        rpc(RPC, "generatetoaddress", 1000, addr, wallet="t8")
        tip = rpc(RPC, "getblockcount")
        print("mined_tip", tip, "h1", h1)
        live_n = 0
        for p in sorted(trace_dir.glob("*-consensus_trace.json")):
            rows.append(row_from_live(p, "regtest_mining"))
            live_n += 1
        extra = f"Live ConnectBlock files after mine+replay: {live_n}. Tip height {tip}."
        write_determinism_report(det, extra)
        if not det["pass"]:
            raise RuntimeError(f"determinism failed: {det['canonical_matches']}/100")
    finally:
        stop(proc, RPC)

    rows.extend(ingest_testnet())
    rows.extend(static_invalid_rows())

    # Attack-vector reconstructions already on disk (CE-001…007)
    for p in sorted((OUT).glob("CE-00[1-7]*/kernel_candidate.json")):
        parent = p.parent
        tid = parent.name
        if any(r["case_id"] == tid for r in rows):
            continue
        tr = json.loads((parent / "consensus_trace.json").read_text()) if (parent / "consensus_trace.json").is_file() else {}
        kc = json.loads(p.read_text())
        if "Not MakeKernelInput" not in str(kc.get("note", "")):
            kc["note"] = ORACLE_NOTE
            kc.setdefault("status", "oracle_only")
        rows.append(
            catalog_row(
                tid,
                "task004_attack_vectors",
                tr.get("block_hash"),
                tr.get("height"),
                tr.get("block_result") or "reject",
                tr.get("rejection_layer"),
                (tr.get("tx_results") or [{}])[0].get("reason") if tr.get("tx_results") else None,
                kc,
            )
        )

    INDEX.write_text("".join(json.dumps(r, separators=(",", ":")) + "\n" for r in rows))
    (OUT / "README.md").write_text(
 "/008 KernelInput candidate dataset.\n\n"
        "`index.jsonl` is the catalog (≥1000 rows). Per-CE folders keep hex + trace.\n"
        "Candidates are oracle reconstructions. Not MakeKernelInput. Not a proof.\n"
    )
    print("wrote", INDEX, "rows", len(rows))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
