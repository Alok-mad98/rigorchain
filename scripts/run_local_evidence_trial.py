#!/usr/bin/env python3
"""local Evidence V1 trial. Observation only. Not a consensus change.

Private -regtest. No public peers. Evidence does not affect accept/reject.
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
from evidence_collector import (  # noqa: E402
    candidate_pollution,
    compute_evidence_id,
    create_block_evidence,
    create_block_evidence_from_trace,
    create_network_observation,
    create_state_transition_evidence,
    generate_kernel_candidate,
    isolation_errors,
    validate_evidence_record,
)

RIGORD = Path(os.environ.get("RIGORD", str(ROOT / "core" / "src" / "rigord")))
DATA = ROOT / "tests" / "formal_bridge" / "data_local_trial"
OUT = ROOT / "audit" / "formal_bridge" / "local_trial_dataset"
REPORT = ROOT / "audit" / "formal_bridge" / "local_evidence_trial_report.md"
RPC_USER = "trial010e"
RPC_PASS = "trial010e-local"
PORTS = {
    "T": {"p2p": 38910, "rpc": 38911},
    "X": {"p2p": 38920, "rpc": 38921},
}
MAX_MONEY = 10_000_000_000 * 100_000_000


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
    payload = json.dumps({"jsonrpc": "1.0", "id": "e", "method": method, "params": list(params)}).encode()
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


def start(node: str, wipe: bool, trace: bool) -> subprocess.Popen:
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
    ]
    if trace:
        args.append("-rigorconsensustrace=1")
    proc = subprocess.Popen(args, cwd=str(ROOT), stdout=subprocess.DEVNULL, stderr=log)
    deadline = time.time() + 60
    last = None
    while time.time() < deadline:
        if proc.poll() is not None:
            raise RuntimeError(f"{node} exited: {(d / 'stderr.log').read_text()[-2000:]}")
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
    except Exception:
        proc.kill()


def ensure_wallet(node: str, name: str = "w") -> None:
    try:
        rpc(node, "loadwallet", name)
    except RpcError as exc:
        if "already loaded" in exc.message.lower() or "duplicate" in exc.message.lower():
            return
        try:
            rpc(node, "createwallet", name)
        except RpcError:
            pass


def utxo_hash(info: dict) -> Any:
    return info.get("hash_serialized_3") or info.get("hash_serialized_2") or info.get("hash")


def rev_digest(node: str) -> Optional[str]:
    rev = datadir(node) / "regtest" / "blocks" / "rev00000.dat"
    if not rev.is_file():
        return None
    return hashlib.sha256(rev.read_bytes()).hexdigest()


def compact(n: int) -> bytes:
    if n < 0xFD:
        return bytes([n])
    if n <= 0xFFFF:
        return b"\xfd" + struct.pack("<H", n)
    if n <= 0xFFFFFFFF:
        return b"\xfe" + struct.pack("<I", n)
    return b"\xff" + struct.pack("<Q", n)


def raw_tx(outputs: list[tuple]) -> str:
    vin = b"\x00" * 32 + struct.pack("<I", 0xFFFFFFFF) + compact(0) + struct.pack("<I", 0xFFFFFFFF)
    body = struct.pack("<i", 1) + compact(1) + vin + compact(len(outputs))
    for value, script in outputs:
        body += struct.pack("<q", value) + compact(len(script)) + script
    body += struct.pack("<I", 0)
    return body.hex()


def patch_header(block_hex: str, **fields: int) -> str:
    raw = bytearray(bytes.fromhex(block_hex))
    if "nBits" in fields:
        raw[72:76] = struct.pack("<I", fields["nBits"] & 0xFFFFFFFF)
    return bytes(raw).hex()


def header_info(node: str, blockhash: str) -> dict:
    hdr = rpc(node, "getblockheader", blockhash)
    blk = rpc(node, "getblock", blockhash, 1)
    bits = hdr.get("bits")
    nbits = int(bits, 16) if isinstance(bits, str) else bits
    return {
        "block_hash": blockhash,
        "height": hdr.get("height"),
        "prev_hash": hdr.get("previousblockhash"),
        "nBits": nbits,
        "tx_count": blk.get("nTx") or len(blk.get("tx") or []),
        "time": hdr.get("time"),
    }


def latest_trace(node: str, blockhash: str) -> Optional[dict]:
    tdir = datadir(node) / "regtest" / "consensus_trace"
    if not tdir.is_dir():
        return None
    prefix = blockhash[:16]
    for path in sorted(tdir.glob(f"*-{prefix}-*-consensus_trace.json")):
        return json.loads(path.read_text())
    for path in sorted(tdir.glob("*-consensus_trace.json"), key=lambda p: p.stat().st_mtime):
        obj = json.loads(path.read_text())
        if obj.get("block_hash") == blockhash:
            return obj
    return None


def reject_of(side: dict) -> str:
    if side.get("decision") == "reject":
        return str(side.get("error") or "reject")
    res = side.get("result")
    if isinstance(res, str) and res:
        return res
    if isinstance(res, list) and res:
        row = res[0]
        if row.get("allowed") is False:
            return str(row.get("reject-reason") or row.get("reject_reason") or "reject")
    return "reject"


def mempool_rejected(side: dict) -> bool:
    if side.get("decision") == "reject":
        return True
    res = side.get("result")
    if isinstance(res, list) and res and res[0].get("allowed") is False:
        return True
    if isinstance(res, str) and res:
        return True
    return False


def write_json(path: Path, obj: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(obj, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def addnode(a: str, b: str) -> None:
    addr = f"127.0.0.1:{PORTS[b]['p2p']}"
    try:
        rpc(a, "addnode", addr, "add")
    except RpcError as exc:
        if "already added" not in exc.message.lower():
            raise
    try:
        rpc(a, "addnode", addr, "onetry")
    except RpcError:
        pass


def wait_tip(node: str, tip: str, timeout: float = 40.0) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        if rpc(node, "getbestblockhash") == tip:
            return True
        time.sleep(0.3)
    return False


def disconnect_all(node: str) -> None:
    for p in rpc(node, "getpeerinfo"):
        try:
            rpc(node, "disconnectnode", "", p["id"])
        except RpcError:
            try:
                rpc(node, "disconnectnode", p.get("addr"))
            except RpcError:
                pass


def candidate_from_reject(*, scenario: str, pow_ok: bool, utxo_ok: bool, script_ok: bool) -> dict:
    trace = {
        "block_hash": None,
        "height": None,
        "block_result": "reject",
        "pow_result": pow_ok,
        "asert_result": True,
        "tx_results": [
            {
                "txid": "00",
                "function": "CheckTxInputs" if not utxo_ok else "VerifyRigorPredicate",
                "inputs_checked": not utxo_ok,
                "utxo_result": utxo_ok,
                "script_result": script_ok,
                "amount_result": True,
                "fee_result": 0,
            }
        ],
        "rejection_layer": scenario,
    }
    return generate_kernel_candidate(trace)


class Trial:
    def __init__(self) -> None:
        self.a: list[dict] = []
        self.b: list[dict] = []
        self.c: list[dict] = []
        self.errors: list[str] = []
        self.restore_ok = False
        self.restart_ok = False
        self.reorg: dict[str, Any] = {}

    def save(self, rec: dict, name: str) -> None:
        rec.setdefault("trial", "local_010e")
        rec["evidence_id"] = compute_evidence_id(rec)
        errs = validate_evidence_record(rec) + isolation_errors(rec)
        if rec.get("class") == "consensus":
            cand = {k: rec.get(k) for k in rec if str(k).startswith("ki_")}
            if cand:
                self.errors.extend(candidate_pollution(cand))
        if errs:
            self.errors.extend(f"{name}: {e}" for e in errs)
        write_json(OUT / f"{name}.json", rec)
        cls = rec.get("class")
        if cls == "consensus":
            self.a.append(rec)
        elif cls == "state_transition":
            self.b.append(rec)
        elif cls == "environment":
            self.c.append(rec)

    def note_peers(self, node: str, scenario: str, **extra: Any) -> None:
        peers = rpc(node, "getpeerinfo")
        rec = create_network_observation(
            peer_count=len(peers),
            connection=extra.get("connection"),
            latency=extra.get("latency"),
            fork=extra.get("fork"),
            orphan=extra.get("orphan"),
            reconnect=extra.get("reconnect"),
            block_hash=rpc(node, "getbestblockhash"),
            source="rpc",
        )
        rec["label"] = scenario
        self.save(rec, f"C_{scenario}")

    def emit_accept(self, node: str, blockhash: str, scenario: str) -> dict:
        info = header_info(node, blockhash)
        trace = latest_trace(node, blockhash)
        if trace:
            rec = create_block_evidence_from_trace(trace, source="consensus_trace", scenario=scenario)
            if not rec.get("prev_hash"):
                rec["prev_hash"] = info["prev_hash"]
            if not rec.get("tx_count"):
                rec["tx_count"] = info["tx_count"]
            if not rec.get("nBits"):
                rec["nBits"] = info["nBits"]
            rec["evidence_id"] = compute_evidence_id(rec)
        else:
            rec = create_block_evidence(
                block_hash=info["block_hash"],
                height=info["height"],
                prev_hash=info["prev_hash"],
                nBits=info["nBits"],
                tx_count=info["tx_count"],
                accept=True,
                kernel_candidate=generate_kernel_candidate(
                    {
                        "block_hash": info["block_hash"],
                        "height": info["height"],
                        "block_result": "accept",
                        "pow_result": True,
                        "asert_result": True,
                        "tx_results": [],
                    }
                ),
                source="rpc",
                scenario=scenario,
            )
        self.save(rec, f"A_{scenario}")
        return rec

    def emit_reject(self, scenario: str, reason: str, layer: str, cand: dict, nbits: Any = None) -> None:
        rec = create_block_evidence(
            block_hash=None,
            height=rpc("T", "getblockcount"),
            prev_hash=rpc("T", "getbestblockhash"),
            nBits=nbits,
            tx_count=None,
            accept=False,
            rejection_layer=layer,
            kernel_candidate=cand,
            source="rpc",
            scenario=scenario,
        )
        rec["reason"] = reason
        self.save(rec, f"A_{scenario}")


def run() -> Trial:
    if DATA.exists():
        shutil.rmtree(DATA)
    if OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir(parents=True, exist_ok=True)
    trial = Trial()
    pt = start("T", wipe=True, trace=True)
    px = None
    try:
        ensure_wallet("T")
        addr = rpc("T", "getnewaddress", wallet="w")
        rpc("T", "generatetoaddress", 101, addr, wallet="w")

        before = rpc("T", "gettxoutsetinfo")
        hashes = rpc("T", "generatetoaddress", 1, addr, wallet="w")
        tip = hashes[0]
        after = rpc("T", "gettxoutsetinfo")
        trial.emit_accept("T", tip, "valid_block")
        trial.save(
            create_state_transition_evidence(
                before_utxo_hash=utxo_hash(before),
                operation="connect",
                after_utxo_hash=utxo_hash(after),
                undo_hash=rev_digest("T"),
                block=tip,
                source="rpc",
            ),
            "B_connect_valid",
        )

        hx = rpc("T", "getblock", tip, 0)
        bad = call_ok("T", "submitblock", patch_header(hx, nBits=0))
        trial.emit_reject(
            "bad_nbits",
            reject_of(bad),
            "CheckProofOfWork",
            candidate_from_reject(scenario="bad_nbits", pow_ok=False, utxo_ok=True, script_ok=True),
            nbits=0,
        )

        over = raw_tx([(MAX_MONEY + 1, bytes.fromhex("0014" + "33" * 20))])
        inv_tx = call_ok("T", "testmempoolaccept", [over])
        trial.emit_reject(
            "invalid_tx",
            reject_of(inv_tx),
            "CheckTransaction",
            candidate_from_reject(scenario="invalid_tx", pow_ok=True, utxo_ok=True, script_ok=True),
        )

        utxos = rpc("T", "listunspent", 1, 9999999, [], wallet="w")
        utxos.sort(key=lambda u: u["amount"], reverse=True)
        u = utxos[0]
        amt = round(float(u["amount"]) - 0.001, 8)
        raw = rpc("T", "createrawtransaction", [{"txid": u["txid"], "vout": u["vout"]}], {addr: amt})
        signed = rpc("T", "signrawtransactionwithwallet", raw, wallet="w")
        tx = signed["hex"]
        ds = call_ok("T", "generateblock", addr, [tx, tx], wallet="w")
        trial.emit_reject(
            "double_spend",
            reject_of(ds),
            "CheckTxInputs",
            candidate_from_reject(scenario="double_spend", pow_ok=True, utxo_ok=False, script_ok=True),
        )
        wit = call_ok("T", "testmempoolaccept", [raw])
        trial.emit_reject(
            "invalid_witness",
            reject_of(wit),
            "VerifyWitnessProgram",
            candidate_from_reject(scenario="invalid_witness", pow_ok=True, utxo_ok=True, script_ok=False),
        )
        if not mempool_rejected(inv_tx):
            trial.errors.append("invalid_tx was not rejected")
        if not mempool_rejected(ds) and ds.get("decision") != "reject":
            trial.errors.append("double_spend was not rejected")
        if not mempool_rejected(wit):
            trial.errors.append("invalid_witness was not rejected")

        mid = rpc("T", "gettxoutsetinfo")
        rpc("T", "invalidateblock", tip)
        after_disc = rpc("T", "gettxoutsetinfo")
        trial.save(
            create_state_transition_evidence(
                before_utxo_hash=utxo_hash(mid),
                operation="disconnect",
                after_utxo_hash=utxo_hash(after_disc),
                undo_hash=rev_digest("T"),
                block=tip,
                source="rpc",
            ),
            "B_disconnect_invalidate",
        )
        trial.restore_ok = utxo_hash(before) == utxo_hash(after_disc)
        rpc("T", "reconsiderblock", tip)
        after_re = rpc("T", "gettxoutsetinfo")
        trial.save(
            create_state_transition_evidence(
                before_utxo_hash=utxo_hash(after_disc),
                operation="connect",
                after_utxo_hash=utxo_hash(after_re),
                undo_hash=rev_digest("T"),
                block=tip,
                source="rpc",
            ),
            "B_reconnect_reconsider",
        )
        if utxo_hash(mid) != utxo_hash(after_re):
            trial.errors.append("reconsider did not restore post-connect UTXO")

        before_rst = rpc("T", "gettxoutsetinfo")
        tip_rst = rpc("T", "getbestblockhash")
        stop(pt, "T")
        pt = start("T", wipe=False, trace=True)
        ensure_wallet("T")
        after_rst = rpc("T", "gettxoutsetinfo")
        tip_after = rpc("T", "getbestblockhash")
        trial.restart_ok = tip_rst == tip_after and utxo_hash(before_rst) == utxo_hash(after_rst)
        rec_rst = create_state_transition_evidence(
            before_utxo_hash=utxo_hash(before_rst),
            operation="restart",
            after_utxo_hash=utxo_hash(after_rst),
            block=tip_after,
            source="rpc",
        )
        rec_rst["before_tip"] = tip_rst
        rec_rst["after_tip"] = tip_after
        trial.save(rec_rst, "B_restart")
        if not trial.restart_ok:
            trial.errors.append("restart tip/UTXO mismatch")

        px = start("X", wipe=True, trace=False)
        ensure_wallet("X")
        addr_x = rpc("X", "getnewaddress", wallet="w")
        addnode("T", "X")
        addnode("X", "T")
        wait_tip("X", rpc("T", "getbestblockhash"), timeout=40)
        trial.note_peers("T", "peers_meshed", connection="connect")
        old_tip = rpc("T", "getbestblockhash")
        height_t = rpc("T", "getblockcount")
        disconnect_all("T")
        disconnect_all("X")
        trial.note_peers("T", "peers_partition", connection="disconnect", fork=True)
        rpc("T", "generatetoaddress", 2, addr, wallet="w")
        rpc("X", "generatetoaddress", 5, addr_x, wallet="w")
        split_t = rpc("T", "getbestblockhash")
        split_x = rpc("X", "getbestblockhash")
        height_x = rpc("X", "getblockcount")
        addnode("T", "X")
        addnode("X", "T")
        deadline = time.time() + 45
        while time.time() < deadline:
            if rpc("T", "getbestblockhash") == rpc("X", "getbestblockhash"):
                break
            time.sleep(0.4)
        new_tip = rpc("T", "getbestblockhash")
        adopted = new_tip == split_x
        trial.reorg = {
            "old_tip": old_tip,
            "chain_a": split_t,
            "chain_b": split_x,
            "new_tip": new_tip,
            "fork_height": height_t,
            "disconnect_count": 2 if adopted else None,
            "connect_count": height_x - height_t if adopted else None,
            "adopted_longer": adopted,
        }
        rec_reorg = create_state_transition_evidence(
            before_utxo_hash=None,
            operation="reorg",
            after_utxo_hash=utxo_hash(rpc("T", "gettxoutsetinfo")),
            block=new_tip,
            source="rpc",
        )
        rec_reorg.update(trial.reorg)
        trial.save(rec_reorg, "B_reorg_longest")
        trial.note_peers("T", "peers_rejoin", reconnect=True)
        if not adopted:
            trial.errors.append("longest-chain reorg did not adopt X")

        write_json(OUT / "index.json", {"a": len(trial.a), "b": len(trial.b), "c": len(trial.c), "errors": trial.errors})
        return trial
    finally:
        stop(pt, "T")
        stop(px, "X")


def write_report(trial: Trial, replay_ok: bool, pollution: str) -> None:
    status = "PASS" if not trial.errors and replay_ok and pollution == "NONE" else "FAIL"
    lines = [
        "# Local Evidence V1 trial report",
        "",
        "Private `-regtest` only. Evidence validation, not consensus validation, not refinement.",
        "",
        f"LOCAL_EVIDENCE_TRIAL_STATUS = {status}",
        f"A_CONSENSUS_EVENTS = {len(trial.a)}",
        f"B_STATE_EVENTS = {len(trial.b)}",
        f"C_NETWORK_EVENTS = {len(trial.c)}",
        f"REPLAY_STATUS = {'PASS' if replay_ok else 'FAIL'}",
        f"KERNELINPUT_POLLUTION = {pollution}",
        "",
        f"- disconnect(connect(S)) restored prior UTXO: `{trial.restore_ok}` (observation, not a theorem)",
        f"- restart same tip/UTXO: `{trial.restart_ok}`",
        f"- reorg: `{json.dumps(trial.reorg, sort_keys=True)}`",
        "",
        "## Class A",
        "",
    ]
    for rec in trial.a:
        lines.append(
            f"- `{rec.get('scenario')}` accept={rec.get('accept')} layer={rec.get('rejection_layer')} id=`{rec.get('evidence_id')[:16]}`"
        )
    lines += ["", "## Class B", ""]
    for rec in trial.b:
        lines.append(f"- `{rec.get('operation')}` id=`{rec.get('evidence_id')[:16]}`")
    lines += ["", "## Class C", ""]
    for rec in trial.c:
        ctx = rec.get("network_context") or {}
        lines.append(f"- `{rec.get('label')}` peers={ctx.get('peer_count')}")
    if trial.errors:
        lines += ["", "## Errors", ""]
        for e in trial.errors:
            lines.append(f"- {e}")
    lines.append("")
    REPORT.write_text("\n".join(lines), encoding="utf-8")


def pollution_status(trial: Trial) -> str:
    for rec in trial.a:
        if isolation_errors(rec):
            return "DETECTED"
        if any(k in rec for k in ("peer_count", "network_context", "client_action", "wallet_event")):
            return "DETECTED"
    for rec in trial.b:
        if isolation_errors(rec):
            return "DETECTED"
    return "NONE"


def main() -> int:
    trial = run()
    # Replay is a separate script; this run still checks identity of saved files.
    from replay_local_trial_evidence import replay_dataset  # local import after file exists

    replay_ok = replay_dataset(OUT) == 0
    pollution = pollution_status(trial)
    if trial.errors:
        replay_ok = replay_ok and False if trial.errors else replay_ok
    write_report(trial, replay_ok and not trial.errors, pollution)
    status = "PASS" if not trial.errors and replay_ok and pollution == "NONE" else "FAIL"
    print(f"LOCAL_EVIDENCE_TRIAL_STATUS = {status}")
    print(f"A_CONSENSUS_EVENTS = {len(trial.a)}")
    print(f"B_STATE_EVENTS = {len(trial.b)}")
    print(f"C_NETWORK_EVENTS = {len(trial.c)}")
    print(f"REPLAY_STATUS = {'PASS' if replay_ok else 'FAIL'}")
    print(f"KERNELINPUT_POLLUTION = {pollution}")
    for e in trial.errors:
        print(" -", e)
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
