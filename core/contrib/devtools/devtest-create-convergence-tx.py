#!/usr/bin/env python3
"""Create/sign/send a consensus-valid convergence (CVG2) transaction on devtest.

Uses ``createrawtransaction`` after ``decodescript`` for the witness-v1 CVG2 address (program
matches ``test_framework/rigor_block_builder.create_convergence_tx``: ``CVG2`` + big-endian
amount in the 32-byte program). Wire ``asset`` remains ``ORU`` where the node RPC requires it;
user-facing wording uses **ORUM** / **CONVERGENCE** only. ``signrawtransactionwithwallet`` is
called with ``prevtxs`` so the prevout amount is exact (no JSON float loss on ``listunspent``).
``listunspent`` uses ``minconf=1`` so unconfirmed wallet rows are not selected as inputs.
"""
import argparse
import json
import subprocess
import sys
from decimal import Decimal, ROUND_DOWN
from typing import List, Optional, Tuple

_COIN = 100_000_000


def make_convergence_program_32(amount_sats: int) -> bytes:
    """32-byte witness v1 program: ``CVG2`` + 8-byte **big-endian** amount + zero padding (matches ``rigor_block_builder`` / consensus)."""
    if amount_sats < 0 or amount_sats > 0xFFFFFFFFFFFFFFFF:
        raise ValueError("amount_sats must fit in uint64")
    prog = bytearray(32)
    prog[0:4] = b"CVG2"
    prog[4:12] = int(amount_sats).to_bytes(8, "big", signed=False)
    return bytes(prog)


# SECURITY-001-A / A-5: RES owner declaration carried in an OP_RETURN of the convergence tx.
RESO_MAGIC = b"RESO"
RESO_VERSION = b"\x01"
DEFAULT_RES_OWNER_PROGRAM = bytes.fromhex("22" * 32)


def make_res_owner_declaration_payload() -> bytes:
    """``RESO`` || version || 32-byte witness v1 RES program (``Consensus::RES_OWNER_PAYLOAD_SIZE``)."""
    return RESO_MAGIC + RESO_VERSION + DEFAULT_RES_OWNER_PROGRAM


def to_sats(amount: str) -> int:
    return int((Decimal(amount) * Decimal(100_000_000)).to_integral_value(rounding=ROUND_DOWN))


def is_native_orum_domain_utxo(u: dict) -> bool:
    """True if *u* is spendable native ORUM-domain coin (RPC may report asset as ORU/ORUM), not RES-only."""
    asset = u.get("asset")
    if asset is None:
        return True
    return str(asset).upper() in ("ORU", "ORUM")


def _listunspent_scriptpubkey_hex(u: dict) -> Optional[str]:
    """Normalize ``listunspent`` scriptPubKey (string or ``{"hex": ...}``) to raw hex."""
    spk = u.get("scriptPubKey")
    if isinstance(spk, dict):
        spk = spk.get("hex")
    if isinstance(spk, str) and spk:
        return spk
    return None


def is_p2wpkh_oru_utxo(u: dict) -> bool:
    """Prefer standard witness v0 keyhash inputs; BIP143 signing expects exact prevout amounts from chain."""
    h = _listunspent_scriptpubkey_hex(u)
    if h is None or len(h) < 4:
        return False
    h = h.lower()
    return h.startswith("0014") and len(h) == 44


def rpc(cli: str, chain: Optional[str], cli_extras: List[str], method: str, *params: str) -> str:
    """If chain is empty, do not pass -chain=… (use network from bitcoin.conf, e.g. devtest=1 in datadir)."""
    cmd = [cli]
    if chain:
        cmd.append(f"-chain={chain}")
    cmd.extend(cli_extras)
    cmd.extend([method, *params])
    p = subprocess.run(cmd, check=False, text=True, capture_output=True)
    if p.returncode != 0:
        raise RuntimeError(
            "rpc command failed\n"
            f"cmd: {' '.join(cmd)}\n"
            f"returncode: {p.returncode}\n"
            f"stdout: {p.stdout.strip()}\n"
            f"stderr: {p.stderr.strip()}\n"
        )
    return p.stdout.strip()


def rpc_json(cli: str, chain: Optional[str], cli_extras: List[str], method: str, *params: str):
    out = rpc(cli, chain, cli_extras, method, *params)
    return json.loads(out)


def get_raw_change_address(cli: str, chain: Optional[str], cli_extras: List[str], preferred: Optional[str]) -> Tuple[str, str]:
    """Return (address, type_used).

    When *preferred* is omitted, only **bech32** is tried: witness v0/v1 or OP_RETURN are required
    for standard vouts; legacy / P2SH-wrapped change is rejected as ``bad-txns-asset-script-nonstandard``.
    """
    types: list[str] = []
    if preferred:
        types.append(preferred)
        for t in ("legacy", "bech32", "p2sh-segwit"):
            if t not in types:
                types.append(t)
    else:
        types.append("bech32")
    last_err: Optional[RuntimeError] = None
    for t in types:
        try:
            addr = rpc(cli, chain, cli_extras, "getrawchangeaddress", t)
            return addr, t
        except RuntimeError as e:
            last_err = e
            continue
    raise RuntimeError(f"getrawchangeaddress failed for types {types}: {last_err}")


def get_new_mining_address(cli: str, chain: Optional[str], cli_extras: List[str]) -> str:
    """Descriptor wallets often have no legacy; try bech32 then legacy."""
    for t in ("bech32", "legacy"):
        try:
            return rpc(cli, chain, cli_extras, "getnewaddress", "convergence-miner", t)
        except RuntimeError:
            continue
    raise RuntimeError("getnewaddress failed for bech32 and legacy")


def main():
    ap = argparse.ArgumentParser(
        description="Create/sign/send a consensus-valid convergence transaction on devtest. "
        "Uses createrawtransaction (after decodescript for the CVG2 v1 address) so the wallet "
        "matches normal sends; RFC-ADDRESS-001 allows v1+ORU on the wire for CVG2 convergence scripts."
    )
    ap.add_argument("--cli", default="./src/rigor-cli", help="Path to rigor-cli")
    ap.add_argument("--chain", default="devtest", help="Chain name for -chain=… (default: devtest)")
    ap.add_argument(
        "--no-chain-arg",
        action="store_true",
        help="Do not pass -chain (read network from bitcoin.conf in -datadir). Avoids conflict with devtest=1 in conf.",
    )
    ap.add_argument("--cli-extra", action="append", default=[], help="Extra rigor-cli arg (repeatable), e.g. --cli-extra=-datadir=/tmp/x")
    ap.add_argument(
        "--convergence-orum",
        default="1.0",
        help="Amount locked in the convergence output, in ORUM display units (default: 1.0). RPC uses asset ORU.",
    )
    ap.add_argument(
        "--fee-sats",
        type=int,
        default=1000,
        help="Desired ORUM-domain miner fee in sats (relay policy only; convergence has no consensus fee floor).",
    )
    ap.add_argument(
        "--change-type",
        default=None,
        choices=("legacy", "bech32", "p2sh-segwit"),
        help="getrawchangeaddress type; if omitted, use bech32 only (required for valid vouts)",
    )
    ap.add_argument("--mine", action="store_true", help="Mine 1 block after broadcast")
    ap.add_argument(
        "--no-broadcast",
        action="store_true",
        help="Sign the convergence tx and print JSON (with txid / raw_signed) but do not sendrawtransaction. "
        "Useful for grinding txids off-chain (e.g. RES proof tests).",
    )
    ap.add_argument("--json", action="store_true", help="Print machine-readable JSON only")
    ap.add_argument(
        "--fund-address",
        default=None,
        metavar="ADDR",
        help="Only consider UTXOs paid to this address (pass to listunspent addresses filter). "
        "Use when the wallet has multiple ORUM-shaped P2WPKH rows and the first match is not "
        "descriptor-signable (avoids witness program hash mismatch on devtest).",
    )
    args = ap.parse_args()

    convergence_sats = to_sats(args.convergence_orum)
    if convergence_sats <= 0:
        print("convergence amount must be > 0", file=sys.stderr)
        sys.exit(1)
    if args.fee_sats < 0:
        print("fee-sats must be >= 0", file=sys.stderr)
        sys.exit(1)

    fee_sats = int(args.fee_sats)

    effective_chain = "" if args.no_chain_arg else args.chain

    cli_extras = args.cli_extra
    blockcount = int(rpc(args.cli, effective_chain, cli_extras, "getblockcount"))

    # Match ``rigor_block_builder.create_convergence_tx``: optional address pin + minimumAmount
    # so the first list row is not a dust / odd row that breaks BIP143 amount binding.
    query_opts = json.dumps({"minimumAmount": 2})
    if args.fund_address:
        utxos = rpc_json(
            args.cli,
            effective_chain,
            cli_extras,
            "listunspent",
            "1",
            "9999999",
            json.dumps([args.fund_address]),
            "true",
            query_opts,
        )
    else:
        utxos = rpc_json(
            args.cli,
            effective_chain,
            cli_extras,
            "listunspent",
            "1",
            "9999999",
            "[]",
            "true",
            query_opts,
        )
    selected = None
    target = convergence_sats + fee_sats
    for u in utxos:
        if not u.get("spendable", False):
            continue
        if not is_native_orum_domain_utxo(u):
            continue
        if not is_p2wpkh_oru_utxo(u):
            continue
        sats = to_sats(str(u["amount"]))
        if sats >= target:
            selected = (u, sats)
            break
    if selected is None:
        print(
            "no suitable spendable ORUM-domain (ORU) P2WPKH utxo found; mine coinbase to the wallet "
            "(e.g. getnewaddress bech32) so listunspent has balance > convergence amount + fee",
            file=sys.stderr,
        )
        sys.exit(1)

    utxo, input_sats = selected
    change_sats = input_sats - target
    if change_sats <= 0:
        print("selected utxo does not cover convergence+fee", file=sys.stderr)
        sys.exit(1)

    change_addr, change_typ = get_raw_change_address(args.cli, effective_chain, cli_extras, args.change_type)
    prog_hex = "5120" + make_convergence_program_32(convergence_sats).hex()
    dec = rpc_json(args.cli, effective_chain, cli_extras, "decodescript", prog_hex)
    conv_addr = dec.get("address")
    if not isinstance(conv_addr, str) or not conv_addr:
        print(
            "decodescript did not return an address for the convergence (CVG2) script; cannot use createrawtransaction",
            file=sys.stderr,
        )
        sys.exit(1)

    convergence_amt_str = format(Decimal(convergence_sats) / Decimal(_COIN), "f")
    change_amt_str = format(Decimal(change_sats) / Decimal(_COIN), "f")
    # Match ``test_framework/rigor_block_builder.create_convergence_tx``: convergence vout first, then change.
    # SECURITY-001-A / A-5 R1: without a RESO owner declaration the convergence is a
    # valid ORU burn but never enters F, so no draw would ever have a candidate.
    outputs_payload = {
        conv_addr: {"amount": convergence_amt_str, "asset": "ORU"},
        change_addr: {"amount": change_amt_str, "asset": "ORU"},
        "data": make_res_owner_declaration_payload().hex(),
    }
    # createrawtransaction returns raw hex, not JSON.
    raw_unsigned = rpc(
        args.cli,
        effective_chain,
        cli_extras,
        "createrawtransaction",
        json.dumps([{"txid": utxo["txid"], "vout": int(utxo["vout"])}]),
        json.dumps(outputs_payload),
    )
    # Exact prevout amount for BIP143 (avoid float from listunspent in JSON-RPC).
    prev_amount_str = format(Decimal(str(utxo["amount"])), ".8f")
    spk_hex = _listunspent_scriptpubkey_hex(utxo)
    if not spk_hex:
        print("listunspent row missing scriptPubKey hex; cannot sign convergence input", file=sys.stderr)
        sys.exit(1)
    prevtxs = json.dumps(
        [
            {
                "txid": utxo["txid"],
                "vout": int(utxo["vout"]),
                "scriptPubKey": spk_hex,
                "amount": prev_amount_str,
            }
        ]
    )
    signed = rpc_json(
        args.cli,
        effective_chain,
        cli_extras,
        "signrawtransactionwithwallet",
        raw_unsigned,
        prevtxs,
    )
    if not signed.get("complete", False):
        print(f"sign failed: {json.dumps(signed, ensure_ascii=False)}", file=sys.stderr)
        sys.exit(1)
    raw_signed = signed["hex"]
    decoded = rpc_json(args.cli, effective_chain, cli_extras, "decoderawtransaction", raw_signed)
    txid = decoded["txid"]

    convergence_meta: dict = {}
    if args.no_broadcast:
        if args.mine:
            print("--no-broadcast cannot be combined with --mine", file=sys.stderr)
            sys.exit(1)
        convergence_meta = {"is_convergence": True, "meets_min_convergence": True, "no_broadcast": True}
    else:
        txid = rpc(
            args.cli,
            effective_chain,
            cli_extras,
            "sendrawtransaction",
            raw_signed,
            "0",
            str(args.convergence_orum),
        )
        entry = rpc_json(args.cli, effective_chain, cli_extras, "getmempoolentry", txid)
        convergence_meta = entry.get("convergence", {})
        if not convergence_meta.get("is_convergence", False):
            print("tx not recognized as convergence by mempool metadata", file=sys.stderr)
            sys.exit(1)
        if not convergence_meta.get("meets_min_convergence", False):
            print("convergence amount below required threshold (meets_min_convergence false)", file=sys.stderr)
            sys.exit(1)

    if args.mine:
        miner_addr = get_new_mining_address(args.cli, effective_chain, cli_extras)
        rpc(args.cli, effective_chain, cli_extras, "generatetoaddress", "1", miner_addr)

    result = {
        "height": blockcount,
        "change_address_type": change_typ,
        "txid": txid,
        "raw_unsigned": raw_unsigned,
        "raw_signed": raw_signed,
        "convergence_meta": convergence_meta,
        "no_broadcast": bool(args.no_broadcast),
        "utxo_txid": utxo["txid"],
        "utxo_vout": int(utxo["vout"]),
        "convergence_sats": convergence_sats,
        "fee_sats": fee_sats,
        "fee_sats_requested": args.fee_sats,
        "change_sats": change_sats,
        "mined": bool(args.mine),
    }
    if args.json:
        print(json.dumps(result, ensure_ascii=False))
    else:
        print(f"height={blockcount}")
        print(f"txid={txid}")
        print(f"raw_unsigned={raw_unsigned}")
        print(f"raw_signed={raw_signed}")
        print(f"convergence_meta={json.dumps(convergence_meta, ensure_ascii=False)}")
        print("convergence transaction created/broadcast successfully")


if __name__ == "__main__":
    main()
