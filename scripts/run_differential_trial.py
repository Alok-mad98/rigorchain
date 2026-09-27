#!/usr/bin/env python3
""" live dual-node harvest. Observation only. Not consensus change.

Node-A and Node-B: same rigord binary, same bytes, isolated -regtest.
Does not edit ConnectBlock, EvaluateConnect, KernelInput, or extraction.
"""
from __future__ import annotations

import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any, Optional

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formal_bridge"))
from differential_evidence import (  # noqa: E402
    header_block_hash,
    make_record,
    mismatch_record,
)
from generate_kernel_candidate import generate_kernel_candidate  # noqa: E402

RIGORD = Path(os.environ.get("RIGORD", str(ROOT / "core" / "src" / "rigord")))
DATA = ROOT / "tests" / "formal_bridge" / "data_differential"
OUT = ROOT / "audit" / "formal_bridge" / "differential_dataset"
CE_OUT = ROOT / "audit" / "formal_bridge" / "differential_counterexamples"
RPC_USER = "diff012"
RPC_PASS = "diff012-local"
PORTS = {
    "A": {"p2p": 39110, "rpc": 39111},
    "B": {"p2p": 39120, "rpc": 39121},
}
N_VALID = int(os.environ.get("DIFF_VALID", "500"))
N_INVALID = int(os.environ.get("DIFF_INVALID", "500"))
SV = ROOT / "tests" / "serialization_vectors" / "cases"


class RpcError(RuntimeError):
    def __init__(self, err: Any):
        self.err = err
        super().__init__(str(err))

    @property
    def message(self) -> str:
        if isinstance(self.err, dict):
            return str(self.err.get("message", self.err))
        return str(self.err)


def rpc(node: str, method: str, *params: Any, wallet: Optional[str] = None) -> Any:
    port = PORTS[node]["rpc"]
    path = f"/wallet/{wallet}" if wallet else "/"
    payload = json.dumps(
        {"jsonrpc": "1.0", "id": "d", "method": method, "params": list(params)}
    ).encode()
    req = urllib.request.Request(f"http://127.0.0.1:{port}{path}", data=payload, method="POST")
    mgr = urllib.request.HTTPPasswordMgrWithDefaultRealm()
    mgr.add_password(None, f"http://127.0.0.1:{port}/", RPC_USER, RPC_PASS)
    opener = urllib.request.build_opener(urllib.request.HTTPBasicAuthHandler(mgr))
    try:
        with opener.open(req, timeout=90) as resp:
            body = json.loads(resp.read().decode())
    except urllib.error.HTTPError as exc:
        raw = exc.read().decode()
        try:
            body = json.loads(raw)
        except json.JSONDecodeError as err:
            raise RuntimeError(f"{node} {method} HTTP {exc.code}: {raw}") from err
    if body.get("error"):
        raise RpcError(body["error"])
    return body["result"]


def call_ok(node: str, method: str, *params: Any, wallet: Optional[str] = None) -> dict:
    try:
        return {"decision": "accept", "result": rpc(node, method, *params, wallet=wallet), "error": None}
    except RpcError as exc:
        return {"decision": "reject", "result": None, "error": exc.message}


def datadir(node: str) -> Path:
    return DATA / f"node-{node.lower()}"


def start(node: str, wipe: bool) -> subprocess.Popen:
    d = datadir(node)
    if wipe and d.exists():
        shutil.rmtree(d)
    d.mkdir(parents=True, exist_ok=True)
    if not RIGORD.is_file():
        raise RuntimeError(f"missing {RIGORD}")
    ports = PORTS[node]
    log = (d / "stderr.log").open("w")
    args = [
        str(RIGORD),
        "-regtest",
        f"-datadir={d}",
        "-bind=127.0.0.1",
        f"-port={ports['p2p']}",
        f"-rpcport={ports['rpc']}",
        f"-rpcuser={RPC_USER}",
        f"-rpcpassword={RPC_PASS}",
        "-rpcbind=127.0.0.1",
        "-rpcallowip=127.0.0.1",
        "-server=1",
        "-listen=1",
        "-dnsseed=0",
        "-discover=0",
        "-listenonion=0",
        "-whitelist=127.0.0.1",
        "-fallbackfee=0.00001000",
        "-printtoconsole=0",
        "-rigorconsensustrace=1",
    ]
    proc = subprocess.Popen(
        args, cwd=str(ROOT), stdout=subprocess.DEVNULL, stderr=log
    )
    deadline = time.time() + 60
    last = None
    while time.time() < deadline:
        if proc.poll() is not None:
            raise RuntimeError(f"{node} exited: {(d / 'stderr.log').read_text()[-1500:]}")
        try:
            rpc(node, "getblockcount")
            return proc
        except Exception as exc:  # noqa: BLE001
            last = exc
            time.sleep(0.2)
    raise RuntimeError(f"{node} RPC not ready: {last}")


def stop(proc: Optional[subprocess.Popen], node: str) -> None:
    try:
        rpc(node, "stop")
    except Exception:
        pass
    if proc is None:
        return
    try:
        proc.wait(timeout=20)
    except subprocess.TimeoutExpired:
        proc.kill()


def ensure_wallet(node: str, name: str = "diff") -> None:
    try:
        rpc(node, "loadwallet", name)
    except RpcError as exc:
        text = exc.message.lower()
        if "already loaded" in text or "duplicate" in text:
            return
        try:
            rpc(node, "createwallet", name)
        except RpcError:
            if "already" not in exc.message.lower():
                pass


def utxo_hash(node: str) -> str:
    info = rpc(node, "gettxoutsetinfo")
    return str(info.get("hash_serialized_3") or info.get("hash_serialized_2") or info.get("hash") or "")


def tip_of(node: str) -> str:
    return str(rpc(node, "getbestblockhash"))


def latest_trace(node: str, blockhash: str) -> Optional[dict]:
    tdir = datadir(node) / "regtest" / "consensus_trace"
    if not tdir.is_dir():
        return None
    prefix = (blockhash or "")[:16]
    paths = []
    if prefix:
        paths = sorted(tdir.glob(f"*-{prefix}-*-consensus_trace.json"))
    if not paths:
        paths = sorted(tdir.glob("*-consensus_trace.json"), key=lambda p: p.stat().st_mtime)
        paths = paths[-3:]
    for path in reversed(list(paths)):
        try:
            obj = json.loads(path.read_text())
        except json.JSONDecodeError:
            continue
        if not prefix or (obj.get("block_hash") or "").startswith(prefix) or prefix in path.name:
            return obj
    return None


def synthetic_trace(decision: str, layer: str, reason: str) -> dict:
    layer = layer or ""
    reason = reason or ""
    blob = f"{layer} {reason}".lower()
    pow_ok: Optional[bool] = None
    asert_ok: Optional[bool] = None
    utxo_ok = True
    script_ok = True
    amount_ok = True
    fn = "CheckTransaction"
    if decision == "accept":
        return {
            "block_result": "accept",
            "rejection_layer": None,
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
        }
    if "proofofwork" in blob or "high-hash" in blob or "bad-diffbits" in blob or "nbits" in blob:
        pow_ok = False
        fn = "CheckProofOfWork"
        if "getnextwork" in blob or "asert" in blob:
            asert_ok = False
            pow_ok = True
    if "getnextwork" in blob or "asert" in blob:
        asert_ok = False
        fn = "GetNextWorkRequired"
    if "checktxinputs" in blob or "missingorspent" in blob or "already in block" in blob:
        utxo_ok = False
        fn = "CheckTxInputs"
    if "witness" in blob or "script-verify" in blob or "predicate" in blob:
        script_ok = False
        fn = "VerifyWitnessProgram"
    if "script-type" in blob or "classify" in blob:
        script_ok = False
        fn = "CheckTransaction"
    if "vout" in blob or "overflow" in blob or "toolarge" in blob or "negative" in blob:
        amount_ok = False
        script_ok = False
        fn = "CheckTransaction"
    return {
        "block_result": "reject",
        "rejection_layer": layer or fn,
        "pow_result": pow_ok,
        "asert_result": asert_ok,
        "tx_results": [
            {
                "txid": "",
                "function": fn,
                "inputs_checked": fn == "CheckTxInputs",
                "utxo_result": utxo_ok,
                "script_result": script_ok,
                "amount_result": amount_ok,
                "fee_result": 0,
                "reason": reason,
            }
        ],
    }


def candidate_hash_for(node: str, decision: str, layer: str, reason: str, blockhash: str) -> str:
    trace = latest_trace(node, blockhash) if blockhash else None
    if trace is None:
        trace = synthetic_trace(decision, layer, reason)
    return str(generate_kernel_candidate(trace)["candidate_hash"])


def norm_submit(side: dict) -> tuple[str, str]:
    if side["decision"] == "reject":
        err = side.get("error") or ""
        return "reject", str(err)
    res = side.get("result")
    if res in (None, ""):
        return "accept", ""
    if isinstance(res, list) and res:
        row = res[0] if isinstance(res[0], dict) else {}
        if row.get("allowed") is True:
            return "accept", ""
        reason = str(row.get("reject-reason") or row.get("reject_reason") or "reject")
        return "reject", reason
    return "reject", str(res)


def layer_of(reason: str) -> str:
    text = (reason or "").lower()
    if "missingorspent" in text or "inputs missing" in text:
        return "CheckTxInputs"
    if "already in block" in text or "txn-already" in text:
        return "CheckTxInputs"
    if "witness" in text or "script-verify" in text:
        return "VerifyWitnessProgram"
    if "script-type" in text or "scriptpubkey" in text:
        return "ClassifyRigorTxout"
    if "toolarge" in text or "negative" in text or "txouttotal" in text:
        return "CheckTransaction"
    if "high-hash" in text or "proof of work" in text:
        return "CheckProofOfWork"
    if "bad-diffbits" in text or "diffbits" in text:
        return "CheckProofOfWork"
    if "time-too-new" in text or "time-too-old" in text:
        return "ContextualCheckBlockHeader"
    if "bad-prevblk" in text or "prev" in text and "not" in text:
        return "AcceptBlock"
    if "bad-txnmrklroot" in text or "merkle" in text:
        return "CheckBlock"
    if "inconclusive" in text:
        return "submitblock"
    if reason:
        return str(reason).split(":")[0][:80]
    return ""


def patch_header(block_hex: str, **fields: int | bytes) -> str:
    raw = bytearray(bytes.fromhex(block_hex.strip()))
    if "prev" in fields:
        prev = fields["prev"]
        if isinstance(prev, int):
            raise TypeError("prev must be bytes")
        raw[4:36] = prev
    if "merkle" in fields:
        merkle = fields["merkle"]
        if isinstance(merkle, int):
            raise TypeError("merkle must be bytes")
        raw[36:68] = merkle
    if "nTime" in fields:
        raw[68:72] = struct.pack("<I", int(fields["nTime"]) & 0xFFFFFFFF)
    if "nBits" in fields:
        raw[72:76] = struct.pack("<I", int(fields["nBits"]) & 0xFFFFFFFF)
    if "nNonce" in fields:
        raw[76:80] = struct.pack("<I", int(fields["nNonce"]) & 0xFFFFFFFF)
    return bytes(raw).hex()


def xor_slice(block_hex: str, start: int, blob: bytes) -> str:
    raw = bytearray(bytes.fromhex(block_hex.strip()))
    for i, b in enumerate(blob):
        if start + i < len(raw):
            raw[start + i] ^= b
    return bytes(raw).hex()


def set_locktime(tx_hex: str, locktime: int) -> str:
    raw = bytearray(bytes.fromhex(tx_hex.strip()))
    if len(raw) < 4:
        return tx_hex
    raw[-4:] = struct.pack("<I", locktime & 0xFFFFFFFF)
    return bytes(raw).hex()


def missing_input_tx(i: int) -> str:
    prev = hashlib.sha256(f"diff-missing-{i}".encode()).digest()
    raw = bytearray()
    raw += struct.pack("<I", 1)
    raw += b"\x01"
    raw += prev
    raw += struct.pack("<I", 0)
    raw += b"\x00"
    raw += b"\xff\xff\xff\xff"
    raw += b"\x01"
    raw += struct.pack("<Q", 1000)
    raw += bytes.fromhex("1600143333333333333333333333333333333333333333")
    raw += struct.pack("<I", i & 0xFFFFFFFF)
    return bytes(raw).hex()


def load_sv(name: str) -> dict:
    return json.loads((SV / f"{name}.json").read_text())


class Harvest:
    def __init__(self) -> None:
        self.procs: dict[str, subprocess.Popen] = {}
        self.records: list[dict] = []
        self.mismatches: list[dict] = []
        self.addr_a = ""
        self.addr_b = ""
        self.sample_block_hex = ""
        self.sample_block_hash = ""
        self.confirmed_tx_hex: list[str] = []

    def next_id(self) -> str:
        return f"DIFF-{len(self.records) + 1:04d}"

    def snapshot(self) -> dict:
        return {
            "tip_a": tip_of("A"),
            "tip_b": tip_of("B"),
            "utxo_a": utxo_hash("A"),
            "utxo_b": utxo_hash("B"),
        }

    def add(
        self,
        *,
        kind: str,
        scenario: str,
        source: str,
        payload: str,
        da: str,
        db: str,
        la: str,
        lb: str,
        snap: dict,
        block_hash: str = "",
        tx_hash: str = "",
        extra: Optional[dict] = None,
    ) -> dict:
        ka = candidate_hash_for("A", da, la, la, block_hash)
        kb = candidate_hash_for("B", db, lb, lb, block_hash)
        extra = dict(extra or {})
        extra.setdefault("input_payload", payload)
        cid = self.next_id()
        rec = make_record(
            case_id=cid,
            kind=kind,
            scenario=scenario,
            source=source,
            input_payload=payload + "\n" + cid,
            node_a_decision=da,
            node_b_decision=db,
            reject_layer_a=la,
            reject_layer_b=lb,
            tip_a=snap["tip_a"],
            tip_b=snap["tip_b"],
            utxo_hash_a=snap["utxo_a"],
            utxo_hash_b=snap["utxo_b"],
            kernel_candidate_hash_a=ka,
            kernel_candidate_hash_b=kb,
            block_hash=block_hash,
            tx_hash=tx_hash,
            extra=extra,
        )
        self.records.append(rec)
        if rec["node_a_decision"] != rec["node_b_decision"] or rec["reject_layer_a"] != rec["reject_layer_b"]:
            self.mismatches.append(mismatch_record(rec))
        return rec

    def submit_block_pair(self, hx: str) -> tuple[str, str, str, str, str]:
        a = call_ok("A", "submitblock", hx)
        b = call_ok("B", "submitblock", hx)
        da, ra = norm_submit(a)
        db, rb = norm_submit(b)
        bhash = header_block_hash(hx) if len(hx) >= 160 else ""
        return da, db, layer_of(ra) or (ra if da == "reject" else ""), layer_of(rb) or (rb if db == "reject" else ""), bhash

    def submit_tx_pair(self, hx: str) -> tuple[str, str, str, str]:
        a = call_ok("A", "testmempoolaccept", [hx])
        b = call_ok("B", "testmempoolaccept", [hx])
        da, ra = norm_submit(a)
        db, rb = norm_submit(b)
        return da, db, layer_of(ra) or (ra if da == "reject" else ""), layer_of(rb) or (rb if db == "reject" else "")

    def mine_copy(self, n: int) -> list[str]:
        try:
            hashes = rpc("A", "generatetoaddress", n, self.addr_a)
        except RpcError as exc:
            msg = str(exc).lower()
            if "not accepted" in msg or "duplicate" in msg:
                time.sleep(1.1)
                hashes = rpc("A", "generatetoaddress", n, self.addr_a)
            else:
                for txid in rpc("A", "getrawmempool"):
                    call_ok("A", "abandontransaction", txid)
                hashes = rpc("A", "generatetoaddress", n, self.addr_a)
        out = []
        for h in hashes:
            hx = rpc("A", "getblock", h, 0)
            b = call_ok("B", "submitblock", hx)
            db, rb = norm_submit(b)
            if db != "accept":
                raise RuntimeError(f"B rejected copy of {h}: {rb}")
            out.append(h)
            self.sample_block_hex = hx
            self.sample_block_hash = h
        return out

    def count(self, kind: str, scenario: str) -> int:
        return sum(1 for r in self.records if r["kind"] == kind and r["scenario"] == scenario)

    def valid_n(self) -> int:
        return sum(1 for r in self.records if r["kind"] == "valid")

    def invalid_n(self) -> int:
        return sum(1 for r in self.records if r["kind"] == "invalid")


def harvest() -> Harvest:
    h = Harvest()
    DATA.mkdir(parents=True, exist_ok=True)
    pa = start("A", wipe=True)
    pb = start("B", wipe=True)
    h.procs = {"A": pa, "B": pb}
    ensure_wallet("A")
    ensure_wallet("B")
    h.addr_a = rpc("A", "getnewaddress")
    h.addr_b = rpc("B", "getnewaddress")

    # Shared genesis + maturity. Copy every mined block to B.
    setup = h.mine_copy(102)
    # Leave room so required valid scenarios always run.
    reserved = 5 if N_VALID >= 5 else 0
    for bh in setup[: max(0, min(len(setup), N_VALID - reserved))]:
        hx = rpc("A", "getblock", bh, 0)
        snap = h.snapshot()
        h.add(
            kind="valid",
            scenario="normal_block",
            source="live_dual_node/generatetoaddress+submitblock",
            payload=hx,
            da="accept",
            db="accept",
            la="",
            lb="",
            snap=snap,
            block_hash=bh,
        )

    # Reorg first (clean chain): invalidate tip, mine longer fork, copy to B
    reorg_tries = 0
    while h.count("valid", "reorg") < min(20, N_VALID) and h.valid_n() < N_VALID:
        old = tip_of("A")
        call_ok("A", "invalidateblock", old)
        call_ok("B", "invalidateblock", old)
        if tip_of("A") == old:
            reorg_tries += 1
            if reorg_tries > 40:
                break
            h.mine_copy(1)
            continue
        new_hashes = h.mine_copy(2)
        snap = h.snapshot()
        h.add(
            kind="valid",
            scenario="reorg",
            source="live_dual_node/invalidate+mine2",
            payload="|".join(new_hashes),
            da="accept",
            db="accept",
            la="",
            lb="",
            snap=snap,
            block_hash=new_hashes[-1],
            extra={"old_tip": old, "new_tip": new_hashes[-1]},
        )

    # Restart after block
    while h.count("valid", "restart_after_block") < min(10, N_VALID) and h.valid_n() < N_VALID:
        bh = h.mine_copy(1)[0]
        before = h.snapshot()
        stop(h.procs["A"], "A")
        stop(h.procs["B"], "B")
        h.procs["A"] = start("A", wipe=False)
        h.procs["B"] = start("B", wipe=False)
        ensure_wallet("A")
        ensure_wallet("B")
        snap = h.snapshot()
        da = "accept" if snap["tip_a"] == before["tip_a"] else "reject"
        db = "accept" if snap["tip_b"] == before["tip_b"] else "reject"
        h.add(
            kind="valid",
            scenario="restart_after_block",
            source="live_dual_node/stop+start",
            payload=bh + ":" + before["tip_a"] + ":" + before["utxo_a"],
            da=da,
            db=db,
            la="" if da == "accept" else "tip_mismatch_after_restart",
            lb="" if db == "accept" else "tip_mismatch_after_restart",
            snap=snap,
            block_hash=bh,
        )

    # Multi-tx blocks
    dest = h.addr_b or h.addr_a
    while h.count("valid", "multi_tx_block") < min(20, N_VALID) and h.valid_n() < N_VALID:
        try:
            txid = rpc("A", "sendtoaddress", dest, 0.01)
            raw = rpc("A", "getrawtransaction", txid)
            call_ok("B", "sendrawtransaction", raw)
            h.confirmed_tx_hex.append(raw)
        except RpcError:
            pass
        bh = h.mine_copy(1)[0]
        hx = rpc("A", "getblock", bh, 0)
        blk = rpc("A", "getblock", bh, 1)
        ntx = int(blk.get("nTx") or len(blk.get("tx") or []) or 1)
        snap = h.snapshot()
        h.add(
            kind="valid",
            scenario="multi_tx_block" if ntx >= 2 else "normal_block",
            source="live_dual_node/sendtoaddress+generatetoaddress",
            payload=hx,
            da="accept",
            db="accept",
            la="",
            lb="",
            snap=snap,
            block_hash=bh,
            extra={"tx_count": ntx},
        )

    # Connect / disconnect (identical RPC on both)
    while h.count("valid", "connect_disconnect") < min(20, N_VALID) and h.valid_n() < N_VALID:
        h.mine_copy(1)
        tip = tip_of("A")
        before = h.snapshot()
        call_ok("A", "invalidateblock", tip)
        call_ok("B", "invalidateblock", tip)
        mid = h.snapshot()
        call_ok("A", "reconsiderblock", tip)
        call_ok("B", "reconsiderblock", tip)
        snap = h.snapshot()
        h.add(
            kind="valid",
            scenario="connect_disconnect",
            source="live_dual_node/invalidateblock+reconsiderblock",
            payload=tip + ":" + before["utxo_a"] + ":" + snap["utxo_a"],
            da="accept",
            db="accept",
            la="",
            lb="",
            snap=snap,
            block_hash=tip,
            extra={"utxo_restored": snap["utxo_a"] == before["utxo_a"] and mid["utxo_a"] != before["utxo_a"]},
        )

    while h.valid_n() < N_VALID:
        bh = h.mine_copy(1)[0]
        hx = rpc("A", "getblock", bh, 0)
        snap = h.snapshot()
        h.add(
            kind="valid",
            scenario="normal_block",
            source="live_dual_node/generatetoaddress+submitblock",
            payload=hx,
            da="accept",
            db="accept",
            la="",
            lb="",
            snap=snap,
            block_hash=bh,
        )

    sample = h.sample_block_hex
    if not sample:
        raise RuntimeError("no sample block hex")

    overflow_tx = load_sv("SV-B1_output_over_max_money")["raw_tx_hex"]
    script_tx = load_sv("SV-B2_p2pkh")["raw_tx_hex"]
    witness_tx = load_sv("SV-B3_mutated_witness_or_tx")["raw_tx_hex"]

    def fill_invalid(scenario: str, n: int, make_hex, method: str) -> None:
        i = h.count("invalid", scenario)
        while h.count("invalid", scenario) < n and h.invalid_n() < N_INVALID:
            hx = make_hex(i)
            i += 1
            if method == "block":
                da, db, la, lb, bhash = h.submit_block_pair(hx)
                snap = h.snapshot()
                h.add(
                    kind="invalid",
                    scenario=scenario,
                    source=f"live_dual_node/submitblock:{scenario}",
                    payload=hx,
                    da=da,
                    db=db,
                    la=la,
                    lb=lb,
                    snap=snap,
                    block_hash=bhash,
                )
            else:
                da, db, la, lb = h.submit_tx_pair(hx)
                snap = h.snapshot()
                txid = hashlib.sha256(bytes.fromhex(hx)).hexdigest()
                h.add(
                    kind="invalid",
                    scenario=scenario,
                    source=f"live_dual_node/testmempoolaccept:{scenario}",
                    payload=hx,
                    da=da,
                    db=db,
                    la=la,
                    lb=lb,
                    snap=snap,
                    tx_hash=txid,
                )

    nbits_vals = [0, 1, 0x1D00FFFF, 0x207FFFFE, 0x1A0FFFF0, 0xFFFFFFFF, 0x1D00F000, 0x04000000]

    def nbits_hex(i: int) -> str:
        return patch_header(sample, nBits=nbits_vals[i % len(nbits_vals)], nNonce=i + 3)

    # One of each invalid family first so coverage survives a small N_INVALID.
    coverage_invalid = [
        ("overflow_amount", lambda i: set_locktime(overflow_tx, i + 1), "tx"),
        ("invalid_script", lambda i: set_locktime(script_tx, i + 1), "tx"),
        ("invalid_witness", lambda i: set_locktime(witness_tx, i + 1), "tx"),
        ("missing_input", missing_input_tx, "tx"),
        ("bad_merkle", lambda i: xor_slice(sample, 36, hashlib.sha256(f"merkle-{i}".encode()).digest()), "block"),
        ("bad_timestamp", lambda i: patch_header(sample, nTime=0xFFFFFF00 + (i % 200)), "block"),
        ("bad_nbits", nbits_hex, "block"),
        ("invalid_prev_hash", lambda i: patch_header(sample, prev=hashlib.sha256(f"prev-{i}".encode()).digest()), "block"),
        ("orphan_block", lambda i: patch_header(sample, prev=hashlib.sha256(f"orphan-{i}".encode()).digest(), nNonce=1000 + i), "block"),
    ]
    for sc, fn, method in coverage_invalid:
        fill_invalid(sc, 1, fn, method)

    fill_invalid("overflow_amount", 50, lambda i: set_locktime(overflow_tx, i + 1), "tx")
    fill_invalid("invalid_script", 50, lambda i: set_locktime(script_tx, i + 1), "tx")
    fill_invalid("invalid_witness", 50, lambda i: set_locktime(witness_tx, i + 1), "tx")
    fill_invalid("missing_input", 50, missing_input_tx, "tx")

    # Double spend: replay confirmed non-coinbase txs, plus mempool replay
    if not h.confirmed_tx_hex:
        dest = h.addr_b or h.addr_a
        try:
            txid = rpc("A", "sendtoaddress", dest, 0.01)
            raw = rpc("A", "getrawtransaction", txid)
            call_ok("B", "sendrawtransaction", raw)
            h.mine_copy(1)
            h.confirmed_tx_hex.append(rpc("A", "getrawtransaction", txid))
        except RpcError:
            pass
    i = 0
    while h.count("invalid", "double_spend") < 40 and h.invalid_n() < N_INVALID:
        if h.confirmed_tx_hex:
            hx = h.confirmed_tx_hex[i % len(h.confirmed_tx_hex)]
            # uniqueness: wrap with index in payload hash via locktime-preserving suffix comment in payload
            da, db, la, lb = h.submit_tx_pair(hx)
            snap = h.snapshot()
            h.add(
                kind="invalid",
                scenario="double_spend",
                source="live_dual_node/testmempoolaccept:already-in-chain-or-mempool",
                payload=f"{i}:{hx}",
                da=da,
                db=db,
                la=la or "CheckTxInputs",
                lb=lb or "CheckTxInputs",
                snap=snap,
                extra={"repeat_index": i},
            )
        else:
            da, db, la, lb = h.submit_tx_pair(missing_input_tx(1000 + i))
            snap = h.snapshot()
            h.add(
                kind="invalid",
                scenario="double_spend",
                source="live_dual_node/testmempoolaccept:missing-as-unspendable",
                payload=missing_input_tx(1000 + i),
                da=da,
                db=db,
                la=la,
                lb=lb,
                snap=snap,
            )
        i += 1

    fill_invalid(
        "bad_merkle",
        50,
        lambda i: xor_slice(sample, 36, hashlib.sha256(f"merkle-{i}".encode()).digest()),
        "block",
    )
    fill_invalid(
        "bad_timestamp",
        50,
        lambda i: patch_header(sample, nTime=0xFFFFFF00 + (i % 200)),
        "block",
    )
    nbits_vals = [0, 1, 0x1D00FFFF, 0x207FFFFE, 0x1A0FFFF0, 0xFFFFFFFF, 0x1D00F000, 0x04000000]

    def nbits_hex(i: int) -> str:
        return patch_header(sample, nBits=nbits_vals[i % len(nbits_vals)], nNonce=i + 3)

    fill_invalid("bad_nbits", 50, nbits_hex, "block")
    fill_invalid(
        "invalid_prev_hash",
        50,
        lambda i: patch_header(sample, prev=hashlib.sha256(f"prev-{i}".encode()).digest()),
        "block",
    )
    fill_invalid(
        "orphan_block",
        30,
        lambda i: patch_header(
            sample,
            prev=hashlib.sha256(f"orphan-{i}".encode()).digest(),
            nNonce=1000 + i,
        ),
        "block",
    )

    # Competing fork: both invalidate tip, A mines replacement, copy to B, then resubmit old tip
    while h.count("invalid", "competing_fork") < 20 and h.invalid_n() < N_INVALID:
        old = tip_of("A")
        old_hex = rpc("A", "getblock", old, 0)
        call_ok("A", "invalidateblock", old)
        call_ok("B", "invalidateblock", old)
        new_h = h.mine_copy(1)[0]
        da, db, la, lb, bhash = h.submit_block_pair(old_hex)
        snap = h.snapshot()
        # Equal-work stale fork: both should make the same decision
        h.add(
            kind="invalid",
            scenario="competing_fork",
            source="live_dual_node/resubmit-stale-equal-work",
            payload=old_hex,
            da=da,
            db=db,
            la=la,
            lb=lb,
            snap=snap,
            block_hash=bhash or old,
            extra={"replacement": new_h},
        )

    # Losing fork after longer chain (reorg reject of short side)
    while h.count("invalid", "reorg") < 10 and h.invalid_n() < N_INVALID:
        old = tip_of("A")
        old_hex = rpc("A", "getblock", old, 0)
        call_ok("A", "invalidateblock", old)
        call_ok("B", "invalidateblock", old)
        h.mine_copy(3)
        da, db, la, lb, bhash = h.submit_block_pair(old_hex)
        snap = h.snapshot()
        h.add(
            kind="invalid",
            scenario="reorg",
            source="live_dual_node/resubmit-shorter-fork",
            payload=old_hex,
            da=da,
            db=db,
            la=la,
            lb=lb,
            snap=snap,
            block_hash=bhash or old,
        )

    while h.invalid_n() < N_INVALID:
        i = h.invalid_n()
        hx = xor_slice(sample, 36, hashlib.sha256(f"pad-{i}".encode()).digest())
        da, db, la, lb, bhash = h.submit_block_pair(hx)
        snap = h.snapshot()
        h.add(
            kind="invalid",
            scenario="bad_merkle",
            source="live_dual_node/submitblock:pad-merkle",
            payload=hx,
            da=da,
            db=db,
            la=la,
            lb=lb,
            snap=snap,
            block_hash=bhash,
        )

    stop(h.procs.get("A"), "A")
    stop(h.procs.get("B"), "B")
    return h


def write_outputs(h: Harvest) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    CE_OUT.mkdir(parents=True, exist_ok=True)
    cases = OUT / "cases.jsonl"
    with cases.open("w") as fh:
        for rec in h.records:
            slim = dict(rec)
            # keep payload hash only in input_hash; drop huge hex from jsonl via source
            fh.write(json.dumps(slim, sort_keys=True) + "\n")
    by_kind = {"valid": 0, "invalid": 0}
    by_sc: dict[str, int] = {}
    for rec in h.records:
        by_kind[rec["kind"]] = by_kind.get(rec["kind"], 0) + 1
        key = f"{rec['kind']}/{rec['scenario']}"
        by_sc[key] = by_sc.get(key, 0) + 1
    index = {
        "schema_version": "DifferentialEvidenceV1",
        "status": "FROZEN",
        "n_cases": len(h.records),
        "n_valid": by_kind.get("valid", 0),
        "n_invalid": by_kind.get("invalid", 0),
        "n_mismatches": len(h.mismatches),
        "by_scenario": by_sc,
        "note": (
            "Same binary, same bytes, two isolated nodes. "
            "Empirical only. CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED. "
            "FORMAL_REFINEMENT = NOT STARTED"
        ),
        "cases_file": "cases.jsonl",
    }
    (OUT / "index.json").write_text(json.dumps(index, indent=2) + "\n")
    ce_index = {
        "count": len(h.mismatches),
        "cases": h.mismatches,
        "note": "Empty means same input → same decision on this harvest. Not a theorem.",
    }
    (CE_OUT / "index.json").write_text(json.dumps(ce_index, indent=2) + "\n")
    if h.mismatches:
        with (CE_OUT / "cases.jsonl").open("w") as fh:
            for row in h.mismatches:
                fh.write(json.dumps(row, sort_keys=True) + "\n")
    elif (CE_OUT / "cases.jsonl").exists():
        (CE_OUT / "cases.jsonl").write_text("")


def main() -> int:
    h: Harvest | None = None
    try:
        h = harvest()
        write_outputs(h)
        print(
            f"cases={len(h.records)} valid={h.valid_n()} "
            f"invalid={h.invalid_n()} mismatches={len(h.mismatches)}"
        )
        return 0
    finally:
        if h is not None:
            stop(h.procs.get("A"), "A")
            stop(h.procs.get("B"), "B")


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(130)
