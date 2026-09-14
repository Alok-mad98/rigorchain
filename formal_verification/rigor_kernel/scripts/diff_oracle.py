#!/usr/bin/env python3
"""Production-logic shadow vs extracted Coq checker.

Compares the comparable *fragment* of production decision procedures
to extracted-kernel-checker. Not a C++ refinement proof.
"""
from __future__ import annotations

import json
import os
import random
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "extracted" / "extracted-kernel-checker"
_OUT = Path(os.environ.get("RIGOR_KERNEL_OUTDIR", str(ROOT / "evidence")))
REPORT = _OUT / "DiffVerifyReport.md"

COIN = 100_000_000
MAX_MONEY = 10_000_000_000 * COIN
RES_COIN = 100_000_000
RES_MAX_SUPPLY = 419_430_400_000_000
RES_DESTROY_COST_GENESIS = 1024 * COIN
RES_DESTROY_POLE = 1 << 25
RES_DESTROY_SHIFT = 40
RES_REWARD_EPOCH = 210_000  # observational calendar only
ASERT_RADIX = 65536
ASERT_EXP_CLAMP = ASERT_RADIX * 64
# Production mainnet powLimit is 231-bit (core pow tests). The JSON driver
# and extracted checker harness use int64, so this is an int64-safe stand-in
# used only to tag the "target above a cap the model does not enforce" gap.
POW_LIMIT_STANDIN = 1 << 60


def res_destroy_cost_at_height(height: int) -> int:
    """Display destroy_cost(h). Not a consensus input."""
    if height < 0:
        return 0
    pole_h = RES_DESTROY_POLE + height
    return (RES_DESTROY_COST_GENESIS * pole_h * pole_h) >> 50


def check_res_mint_arith(res_atoms: int, burn_atoms: int, height: int) -> bool:
    if res_atoms < 0 or burn_atoms < 0 or height < 0:
        return False
    if res_atoms == 0:
        return True
    if res_atoms > RES_MAX_SUPPLY or burn_atoms > MAX_MONEY:
        return False
    pole_h = RES_DESTROY_POLE + height
    return res_atoms * pole_h * pole_h <= (burn_atoms << RES_DESTROY_SHIFT)


def min_burn_for_atoms(res_atoms: int, height: int) -> int:
    if res_atoms <= 0 or height < 0:
        return 0
    pole_h = RES_DESTROY_POLE + height
    numer = res_atoms * pole_h * pole_h
    den = 1 << RES_DESTROY_SHIFT
    return (numer + den - 1) // den


def get_res_mint_reward(height: int) -> int:
    """Compatibility alias: display destroy cost at height, not a mint grant."""
    return res_destroy_cost_at_height(height)


def money_range(v: int) -> bool:
    return 0 <= v <= MAX_MONEY


def quot(a: int, b: int) -> int:
    """Toward-zero division, matching Coq Z.quot / C++ __int128 /."""
    if b == 0:
        raise ZeroDivisionError
    q = abs(a) // abs(b)
    return -q if (a < 0) != (b < 0) else q


def compact_encode(t: int) -> int:
    if t <= 0:
        return 0
    raw_size = (t.bit_length() + 7) // 8
    if raw_size <= 3:
        m = t << (8 * (3 - raw_size))
        s = raw_size
    else:
        m = t >> (8 * (raw_size - 3))
        s = raw_size
    if m & (1 << 23):
        m >>= 8
        s += 1
    return s * 16777216 + m


def compact_decode(nbits: int) -> int:
    size = nbits // 16777216
    word = nbits % 8388608
    if size <= 3:
        return word >> (8 * (3 - size))
    return word << (8 * (size - 3))


def _split_at(n: int, xs: list[int]):
    if n < 0 or n > len(xs):
        return None
    return xs[:n], xs[n:]


def _is_push_only(rest: list[int]) -> bool:
    fuel = len(rest)
    cur = rest
    while True:
        if not cur:
            return True
        if fuel <= 0:
            return False
        fuel -= 1
        op, tail = cur[0], cur[1:]
        if op <= 75:
            sp = _split_at(op, tail)
            if sp is None:
                return False
            cur = sp[1]
        elif op == 76:
            if not tail:
                return False
            sp = _split_at(tail[0], tail[1:])
            if sp is None:
                return False
            cur = sp[1]
        elif op == 77:
            if len(tail) < 2:
                return False
            sp = _split_at(tail[0] + tail[1] * 256, tail[2:])
            if sp is None:
                return False
            cur = sp[1]
        elif op == 78:
            if len(tail) < 4:
                return False
            ln = tail[0] + tail[1] * 256 + tail[2] * 65536 + tail[3] * 16777216
            sp = _split_at(ln, tail[4:])
            if sp is None:
                return False
            cur = sp[1]
        elif op <= 96:
            cur = tail
        else:
            return False


def classify_script(script: list[int]) -> str:
    n = len(script)
    if 4 <= n <= 42:
        ver, plen, rest = script[0], script[1], script[2:]
        v = None
        if ver == 0:
            v = 0
        elif 81 <= ver <= 96:
            v = ver - 80
        if v is not None and plen > 0 and len(rest) == plen:
            if v == 0 and plen == 20:
                return "WITNESS_V0"
            if v == 1 and plen == 32:
                tag = rest[:4]
                if tag != [67, 86, 71, 50] and tag != [82, 69, 83, 50]:
                    return "WITNESS_V1"
            return "NONSTANDARD"
    if script:
        sn, rest = script[0], script[1:]
        after = None
        if sn == 0:
            after = rest
        elif 1 <= sn <= 4 and len(rest) >= sn:
            after = rest[sn:]
        if after and after[0] == 20:
            body = after[1:]
            if len(body) == 21 and body[20] == 187:
                return "MINT"
    if n == 22 and script[0] == 20 and script[-1] == 189:
        return "CONVERGE"
    if script and script[0] == 106 and _is_push_only(script[1:]):
        return "NULL_DATA"
    return "NONSTANDARD"


def shadow_utxo(o: dict) -> bool:
    if o.get("coinbase"):
        return False
    inputs = o.get("inputs") or []
    outputs = o.get("outputs") or []
    if not inputs or not outputs:
        return False
    keys = [(tuple(i.get("txid", [])), int(i.get("index", 0))) for i in inputs]
    if len(keys) != len(set(keys)):
        return False
    txid = tuple(o.get("tx_id") or [])
    for i, inp in enumerate(inputs):
        if tuple(inp.get("txid", [])) == txid and int(inp.get("index", -1)) < len(outputs):
            return False
    vals = [int(x.get("value", 0)) for x in outputs]
    if any(not money_range(v) for v in vals):
        return False
    return money_range(sum(vals))


def shadow_oru(o: dict) -> bool:
    oi, oo = int(o["oru_in"]), int(o["oru_out"])
    ri, ro = int(o["res_in"]), int(o["res_out"])
    if min(oi, oo, ri, ro) < 0:
        return False
    if oo > oi:
        return False
    if o.get("dom") == "ORU":
        return ri == 0 and ro == 0
    return oi > 0 and ro <= ri


def shadow_res(o: dict) -> bool:
    # RFC-020: body-tx burn-to-mint. draw_ok is ignored (retired).
    if o.get("coinbase"):
        return False
    if not o.get("mint_ok"):
        return False
    amt = int(o["mint_amount"])
    h = int(o["height"])
    burn = int(o.get("burn_oru") or 0)
    if amt <= 0:
        return False
    return check_res_mint_arith(amt, burn, h)


def shadow_script(o: dict) -> bool:
    return classify_script(list(o.get("script") or [])) == o.get("expected")


def shadow_pow(o: dict) -> bool:
    digest, target, compact_t = int(o["digest"]), int(o["target"]), int(o["compact_t"])
    if target <= 0 or digest > target or compact_t < 0:
        return False
    return compact_decode(compact_encode(compact_t)) <= compact_t


def shadow_asert(o: dict) -> bool:
    te, tau, q = int(o["te"]), int(o["tau"]), int(o["expected_q"])
    if te >= 0 or tau <= 0:
        return False
    got = quot(te * ASERT_RADIX, tau)
    return got == q


SHADOWS = {
    "utxo": shadow_utxo,
    "oru": shadow_oru,
    "res": shadow_res,
    "script": shadow_script,
    "pow": shadow_pow,
    "asert": shadow_asert,
}


def model_verdict(path: Path) -> str:
    out = subprocess.check_output([str(BIN), str(path)], text=True).strip()
    return out


def write_json(path: Path, obj: dict) -> None:
    path.write_text(json.dumps(obj, indent=1) + "\n")


def extra_vectors(work: Path) -> list[Path]:
    work.mkdir(parents=True, exist_ok=True)
    specs = []

    def add(name: str, obj: dict) -> None:
        p = work / name
        write_json(p, obj)
        specs.append(p)

    add("extra_utxo_max_money.json", {
        "domain": "utxo", "expect": "VALID",
        "tx_id": [9], "inputs": [{"txid": [8], "index": 0}],
        "outputs": [{"value": MAX_MONEY, "spk": []}], "coinbase": False,
    })
    add("extra_utxo_zero.json", {
        "domain": "utxo", "expect": "VALID",
        "tx_id": [9], "inputs": [{"txid": [8], "index": 0}],
        "outputs": [{"value": 0, "spk": []}], "coinbase": False,
    })
    add("extra_utxo_coinbase.json", {
        "domain": "utxo", "expect": "INVALID",
        "tx_id": [1], "inputs": [{"txid": [2], "index": 0}],
        "outputs": [{"value": 1, "spk": []}], "coinbase": True,
    })
    add("extra_oru_equal.json", {
        "domain": "oru", "expect": "VALID", "dom": "ORU",
        "oru_in": 5, "oru_out": 5, "res_in": 0, "res_out": 0,
        "old_supply": 0, "subsidy": 0,
    })
    add("extra_oru_res_leak.json", {
        "domain": "oru", "expect": "INVALID", "dom": "ORU",
        "oru_in": 10, "oru_out": 7, "res_in": 1, "res_out": 0,
        "old_supply": 0, "subsidy": 0,
    })
    add("extra_res_exact_w0.json", {
        "domain": "res", "expect": "VALID", "coinbase": False, "mint_ok": True,
        "mint_amount": RES_COIN, "burn_oru": RES_DESTROY_COST_GENESIS,
        "height": 0, "draw_ok": False,
        "dom": "ORU", "res_in": 0, "res_out": 0, "oru_in": 0,
    })
    add("extra_res_neg_height.json", {
        "domain": "res", "expect": "INVALID", "coinbase": True, "mint_ok": True,
        "mint_amount": 1, "height": -1, "draw_ok": True,
        "dom": "ORU", "res_in": 0, "res_out": 0, "oru_in": 0,
    })
    add("extra_pow_equal.json", {
        "domain": "pow", "expect": "VALID", "digest": 100, "target": 100, "compact_t": 100,
    })
    add("extra_pow_zero_target.json", {
        "domain": "pow", "expect": "INVALID", "digest": 0, "target": 0, "compact_t": 100,
    })
    add("extra_asert_te_pos.json", {
        "domain": "asert", "expect": "INVALID", "te": 1, "tau": 43200, "expected_q": 1,
    })
    add("extra_asert_te_neg1.json", {
        "domain": "asert", "expect": "VALID",
        "te": -1, "tau": 43200, "expected_q": quot(-1 * ASERT_RADIX, 43200),
    })
    add("extra_script_empty.json", {
        "domain": "script", "expect": "VALID",
        "script": [], "expected": "NONSTANDARD",
    })
    add("extra_script_wrong.json", {
        "domain": "script", "expect": "INVALID",
        "script": [], "expected": "WITNESS_V0",
    })

    # --- ConnectBlock-adjacent structural fragment (not ConnectBlock) ---
    add("gap_utxo_empty_in.json", {
        "domain": "utxo", "coverage": "connectblock_fragment",
        "tx_id": [1], "inputs": [],
        "outputs": [{"value": 1, "spk": []}], "coinbase": False,
    })
    add("gap_utxo_empty_out.json", {
        "domain": "utxo", "coverage": "connectblock_fragment",
        "tx_id": [1], "inputs": [{"txid": [2], "index": 0}],
        "outputs": [], "coinbase": False,
    })
    add("gap_utxo_dup_in.json", {
        "domain": "utxo", "coverage": "connectblock_fragment",
        "tx_id": [1],
        "inputs": [{"txid": [2], "index": 0}, {"txid": [2], "index": 0}],
        "outputs": [{"value": 1, "spk": []}], "coinbase": False,
    })
    add("gap_utxo_self_spend.json", {
        "domain": "utxo", "coverage": "connectblock_fragment",
        "tx_id": [7],
        "inputs": [{"txid": [7], "index": 0}],
        "outputs": [{"value": 1, "spk": []}], "coinbase": False,
    })
    add("gap_utxo_sum_overflow.json", {
        "domain": "utxo", "coverage": "connectblock_fragment",
        "tx_id": [1], "inputs": [{"txid": [2], "index": 0}],
        "outputs": [{"value": MAX_MONEY, "spk": []},
                    {"value": 1, "spk": []}], "coinbase": False,
    })
    add("gap_utxo_neg_value.json", {
        "domain": "utxo", "coverage": "connectblock_fragment",
        "tx_id": [1], "inputs": [{"txid": [2], "index": 0}],
        "outputs": [{"value": -1, "spk": []}], "coinbase": False,
    })
    add("gap_utxo_model_valid_not_connectblock.json", {
        "domain": "utxo", "coverage": "connectblock_gap",
        "note": "Fragment VALID; ConnectBlock would still require prevouts/scripts/sigs",
        "tx_id": [3], "inputs": [{"txid": [4], "index": 1}],
        "outputs": [{"value": 50, "spk": []}], "coinbase": False,
    })

    # --- ORU / DomRES ---
    add("extra_oru_domres_ok.json", {
        "domain": "oru", "coverage": "oru_domain",
        "dom": "RES", "oru_in": 3, "oru_out": 1, "res_in": 8, "res_out": 5,
        "old_supply": 0, "subsidy": 0,
    })
    add("extra_oru_domres_zero_in.json", {
        "domain": "oru", "coverage": "oru_domain",
        "dom": "RES", "oru_in": 0, "oru_out": 0, "res_in": 1, "res_out": 0,
        "old_supply": 0, "subsidy": 0,
    })
    add("extra_oru_domres_inflate.json", {
        "domain": "oru", "coverage": "oru_domain",
        "dom": "RES", "oru_in": 2, "oru_out": 1, "res_in": 1, "res_out": 2,
        "old_supply": 0, "subsidy": 0,
    })
    add("extra_oru_neg_leg.json", {
        "domain": "oru", "coverage": "oru_domain",
        "dom": "ORU", "oru_in": -1, "oru_out": -2, "res_in": 0, "res_out": 0,
        "old_supply": 0, "subsidy": 0,
    })

    # --- RES / S_RES (C-3): running scalar is not a reject ---
    h1 = RES_REWARD_EPOCH
    burn1 = min_burn_for_atoms(RES_COIN, h1)
    add("extra_res_epoch1_exact.json", {
        "domain": "res", "coverage": "res_schedule",
        "coinbase": False, "mint_ok": True, "mint_amount": RES_COIN,
        "burn_oru": burn1, "height": h1, "draw_ok": False,
        "dom": "ORU", "res_in": 0, "res_out": 0, "oru_in": 0,
    })
    add("extra_res_epoch1_plus.json", {
        "domain": "res", "coverage": "res_schedule",
        "coinbase": False, "mint_ok": True, "mint_amount": RES_COIN,
        "burn_oru": burn1 - 1, "height": h1, "draw_ok": False,
        "dom": "ORU", "res_in": 0, "res_out": 0, "oru_in": 0,
    })
    add("extra_res_no_draw.json", {
        "domain": "res", "coverage": "res_draw",
        "note": "draw_ok is retired; a paid body mint stays valid",
        "coinbase": False, "mint_ok": True, "mint_amount": RES_COIN,
        "burn_oru": RES_DESTROY_COST_GENESIS, "height": 0, "draw_ok": False,
        "dom": "ORU", "res_in": 0, "res_out": 0, "oru_in": 0,
    })
    add("extra_res_no_mint_shape.json", {
        "domain": "res", "coverage": "res_draw",
        "coinbase": False, "mint_ok": False, "mint_amount": RES_COIN,
        "burn_oru": RES_DESTROY_COST_GENESIS, "height": 0, "draw_ok": False,
        "dom": "ORU", "res_in": 0, "res_out": 0, "oru_in": 0,
    })
    add("extra_res_coinbase.json", {
        "domain": "res", "coverage": "res_draw",
        "coinbase": True, "mint_ok": True, "mint_amount": RES_COIN,
        "burn_oru": RES_DESTROY_COST_GENESIS, "height": 0, "draw_ok": False,
        "dom": "ORU", "res_in": 0, "res_out": 0, "oru_in": 0,
    })
    add("extra_res_epoch251.json", {
        "domain": "res", "coverage": "res_schedule",
        "note": "no mint horizon; observational k=251 is still authorized",
        "coinbase": False, "mint_ok": True, "mint_amount": RES_COIN,
        "burn_oru": min_burn_for_atoms(RES_COIN, 251 * RES_REWARD_EPOCH),
        "height": 251 * RES_REWARD_EPOCH, "draw_ok": False,
        "dom": "ORU", "res_in": 0, "res_out": 0, "oru_in": 0,
    })
    add("gap_res_sres_ignored.json", {
        "domain": "res", "coverage": "s_res_gap",
        "note": "s_res_running is not a model input; C++ ConnectBlock does not reject on it (C-3)",
        "s_res_running": 999999999,
        "coinbase": False, "mint_ok": True,
        "mint_amount": RES_COIN, "burn_oru": RES_DESTROY_COST_GENESIS,
        "height": 0, "draw_ok": False,
        "dom": "ORU", "res_in": 0, "res_out": 0, "oru_in": 0,
    })

    # --- Classifier ladder fragment (C-7 still open vs C++) ---
    mint = [0, 20] + [0] * 20 + [187]
    converge = [20] + [0] * 20 + [189]
    v1 = [81, 32] + [1] * 32
    v1_cvg2 = [81, 32] + [67, 86, 71, 50] + [0] * 28
    v0_short = [0, 16] + [0] * 16
    add("extra_script_mint.json", {
        "domain": "script", "coverage": "classifier",
        "script": mint, "expected": "MINT",
    })
    add("extra_script_converge.json", {
        "domain": "script", "coverage": "classifier",
        "script": converge, "expected": "CONVERGE",
    })
    add("extra_script_null_data.json", {
        "domain": "script", "coverage": "classifier",
        "script": [106], "expected": "NULL_DATA",
    })
    add("extra_script_v0_wrong_len.json", {
        "domain": "script", "coverage": "classifier",
        "script": v0_short, "expected": "NONSTANDARD",
    })
    add("extra_script_v1_native.json", {
        "domain": "script", "coverage": "classifier",
        "script": v1, "expected": "WITNESS_V1",
    })
    add("extra_script_v1_cvg2.json", {
        "domain": "script", "coverage": "classifier",
        "script": v1_cvg2, "expected": "NONSTANDARD",
    })
    add("extra_script_witness_no_fallthrough.json", {
        "domain": "script", "coverage": "classifier",
        "note": "Unsupported witness terminates; does not fall through to mint",
        "script": v0_short, "expected": "MINT",
    })
    add("extra_script_opreturn_not_pushonly.json", {
        "domain": "script", "coverage": "classifier",
        "script": [106, 172], "expected": "NONSTANDARD",
    })

    # --- PoW integer fragment (keccak / GetRigorPowHash not compared) ---
    add("extra_pow_above_target.json", {
        "domain": "pow", "coverage": "pow_integer",
        "digest": 101, "target": 100, "compact_t": 100,
    })
    add("extra_pow_compact_zero.json", {
        "domain": "pow", "coverage": "pow_integer",
        "digest": 1, "target": 100, "compact_t": 0,
    })
    add("extra_pow_neg_target.json", {
        "domain": "pow", "coverage": "pow_integer",
        "digest": 1, "target": -1, "compact_t": 100,
    })
    add("gap_pow_keccak_not_compared.json", {
        "domain": "pow", "coverage": "keccak_gap",
        "note": "digest is a supplied integer; production GetRigorPowHash is not evaluated",
        "digest": 1, "target": 100, "compact_t": 100,
    })

    # --- ASERT exp_q fragment (full nBits not compared) ---
    add("extra_asert_quot_tau.json", {
        "domain": "asert", "coverage": "asert_quot",
        "te": -43200, "tau": 43200,
        "expected_q": quot(-43200 * ASERT_RADIX, 43200),
    })
    add("extra_asert_quot_mismatch.json", {
        "domain": "asert", "coverage": "asert_quot",
        "te": -43200, "tau": 43200, "expected_q": 0,
    })
    add("extra_asert_tau1.json", {
        "domain": "asert", "coverage": "asert_quot",
        "te": -1, "tau": 1, "expected_q": quot(-1 * ASERT_RADIX, 1),
    })
    add("gap_asert_nbits_not_compared.json", {
        "domain": "asert", "coverage": "asert_nbits_gap",
        "note": "Fragment checks exp_q only; production continues to 256-bit nBits",
        "te": -1, "tau": 43200, "expected_q": quot(-1 * ASERT_RADIX, 43200),
    })

    # --- production-only branches (model-mirrored shadow still agrees) ---
    add("gap_pow_target_above_powlimit.json", {
        "domain": "pow", "coverage": "powlimit_gap", "gap": True,
        "note": "target > int64-safe stand-in cap (2^60). Production "
                "CheckProofOfWork rejects bnTarget > powLimit (~2^231). "
                "Model has no powLimit check.",
        "digest": 1, "target": POW_LIMIT_STANDIN + 1, "compact_t": 100,
    })
    add("gap_pow_compact_signbit.json", {
        "domain": "pow", "coverage": "setcompact_gap", "gap": True,
        "note": "compact_t = 1<<23. If interpreted as nBits mantissa, "
                "SetCompact fNegative. Model treats it as a non-negative integer.",
        "digest": 1, "target": 100, "compact_t": 1 << 23,
    })
    add("gap_pow_compact_overflow.json", {
        "domain": "pow", "coverage": "setcompact_gap", "gap": True,
        "note": "Huge compact_t: production SetCompact can set fOverflow. "
                "Model has no overflow flag; encode/decode is unbounded Z.",
        "digest": 1, "target": 100, "compact_t": 1 << 60,
    })
    te_over = -3_000_000
    tau_over = 43200
    q_over = quot(te_over * ASERT_RADIX, tau_over)
    assert abs(q_over) > ASERT_EXP_CLAMP
    add("gap_asert_exp_q_over_clamp.json", {
        "domain": "asert", "coverage": "asert_clamp_gap", "gap": True,
        "note": "Model compares unclamped exp_q_raw. Production clamps to "
                "±ASERT_EXP_CLAMP before use.",
        "te": te_over, "tau": tau_over, "expected_q": q_over,
    })

    rng = random.Random(117)
    for i in range(20):
        v = rng.randint(0, 1000)
        add(f"extra_rand_utxo_{i}.json", {
            "domain": "utxo", "expect": "AUTO",
            "tx_id": [i + 20], "inputs": [{"txid": [i + 100], "index": 0}],
            "outputs": [{"value": v, "spk": []}], "coinbase": False,
        })
        oi = rng.randint(0, 50)
        oo = rng.randint(0, 50)
        add(f"extra_rand_oru_{i}.json", {
            "domain": "oru", "expect": "AUTO", "dom": "ORU",
            "oru_in": oi, "oru_out": oo, "res_in": 0, "res_out": 0,
            "old_supply": 0, "subsidy": 0,
        })
    rng2 = random.Random(119)
    for i in range(10):
        h = rng2.choice([0, 1, RES_REWARD_EPOCH, 100])
        amt = rng2.choice([0, 1, RES_COIN])
        need = min_burn_for_atoms(amt, h) if amt else 0
        burn = need if rng2.random() < 0.6 else max(0, need - 1)
        add(f"extra_rand_res_{i}.json", {
            "domain": "res", "coverage": "random",
            "coinbase": False, "mint_ok": True, "mint_amount": amt,
            "burn_oru": burn, "height": h, "draw_ok": False,
            "dom": "ORU", "res_in": 0, "res_out": 0, "oru_in": 0,
        })
        te = -rng2.randint(1, 200000)
        tau = rng2.choice([1, 43200, 65536])
        q = quot(te * ASERT_RADIX, tau) if rng2.random() < 0.6 else 0
        add(f"extra_rand_asert_{i}.json", {
            "domain": "asert", "coverage": "random",
            "te": te, "tau": tau, "expected_q": q,
        })
        tgt = rng2.randint(1, 1000)
        add(f"extra_rand_pow_{i}.json", {
            "domain": "pow", "coverage": "random",
            "digest": rng2.randint(0, tgt + 5), "target": tgt, "compact_t": tgt,
        })
        slen = rng2.randint(0, 24)
        script = [rng2.randint(0, 189) for _ in range(slen)]
        add(f"extra_rand_script_{i}.json", {
            "domain": "script", "coverage": "random",
            "script": script, "expected": classify_script(script),
        })
    return specs


def load(path: Path) -> dict:
    return json.loads(path.read_text())


def main() -> int:
    if not BIN.is_file() or not os.access(BIN, os.X_OK):
        print("FAIL: extracted-kernel-checker missing; run extract_and_test.sh", file=sys.stderr)
        return 2

    golden = [
        ROOT / "kernel_binary/test_vectors/utxo_valid.json",
        ROOT / "kernel_binary/test_vectors/oru_conservation.json",
        ROOT / "kernel_binary/test_vectors/res_schedule.json",
        ROOT / "kernel_binary/test_vectors/script_classification.json",
        ROOT / "kernel_binary/test_vectors/pow_target.json",
        ROOT / "kernel_binary/test_vectors/asert_negative_time.json",
        ROOT / "kernel_binary/vectors/invalid/INVALID-001.json",
        ROOT / "kernel_binary/vectors/invalid/INVALID-002.json",
        ROOT / "kernel_binary/vectors/invalid/INVALID-003.json",
        ROOT / "kernel_binary/vectors/invalid/INVALID-004.json",
        ROOT / "kernel_binary/vectors/invalid/INVALID-005.json",
    ]
    extras = extra_vectors(ROOT / "extracted" / "diff_work")
    rows = []
    mismatches = 0
    expect_checked = 0
    expect_miss = 0
    for path in golden + extras:
        obj = load(path)
        domain = obj["domain"]
        shadow_ok = SHADOWS[domain](obj)
        shadow = "VALID" if shadow_ok else "INVALID"
        model = model_verdict(path)
        match = model == shadow
        if not match:
            mismatches += 1
        expect = obj.get("expect")
        expect_col = ""
        if expect in ("VALID", "INVALID"):
            expect_checked += 1
            if model != expect or shadow != expect:
                expect_miss += 1
                expect_col = "EXPECT_MISS"
            else:
                expect_col = "EXPECT_HIT"
        rows.append(
            (path.name, domain, model, shadow, "MATCH" if match else "DIFF", expect_col)
        )
        print(
            f"diff vector={path.name} model={model} production_shadow={shadow} "
            f"{rows[-1][4]}"
            + (f" {expect_col}" if expect_col else "")
        )

    ok = mismatches == 0 and expect_miss == 0
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Differential Verification Report",
        "",
        "Extracted Coq checker (executable specification) vs the shadow of",
        "the *comparable fragment* (see `release/RefinementMapping.md` for",
        "production-derived vs model-mirrored columns).",
        "",
        "**Not a production refinement proof.**",
        "",
        "`MATCH` means the two *implementations agreed*. It is **not** an",
        "independent correctness assertion and **not** a C++ refinement.",
        "When `expect` is `VALID` or `INVALID`, that absolute verdict is",
        "checked separately (`EXPECT_HIT` / `EXPECT_MISS`). `AUTO` or a",
        "missing `expect` is agreement-only.",
        "",
        "`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`",
        "`EXTRACTION_FORMAL_PROOF=source_identity_only`",
        "`EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE`",
        "",
        "Crypto assumptions: `Kernel/CryptoAssumptions.v`.",
        "Mapping: `release/RefinementMapping.md`.",
        "",
        f"Vectors: {len(rows)}  Matches: {sum(1 for r in rows if r[4]=='MATCH')}  "
        f"Diffs: {mismatches}  expect_checked: {expect_checked}  "
        f"expect_miss: {expect_miss}",
        "",
        "| Vector | Domain | Model | Shadow | Result | expect |",
        "|---|---|---|---|---|---|",
    ]
    for name, domain, model, shadow, res, exp in rows:
        lines.append(f"| `{name}` | {domain} | {model} | {shadow} | {res} | {exp} |")
    gap_names = [r[0] for r in rows if r[0].startswith("gap_")]
    lines += [
        "",
        "## Structured coverage of mapped gaps (fragment only)",
        "",
        "These vectors exercise the *comparable fragment* around points",
        "that `release/RefinementMapping.md` marks as **not refined**:",
        "ConnectBlock extras, running `S_RES`, classifier ladder, keccak",
        "PoW hash, full ASERT nBits, `powLimit`, SetCompact sign/overflow,",
        "unclamped `exp_q_raw` vs production `±ASERT_EXP_CLAMP`.",
        "A `MATCH` here is still fragment agreement only.",
        "",
        "Gap-tagged vectors: " + ", ".join(f"`{n}`" for n in gap_names),
        "",
        "## Known gaps (not compared)",
        "",
        "- Production `ConnectBlock` prevouts / scripts / signatures / locktime.",
        "- Production `CheckProofOfWork` keccak / `GetRigorPowHash`.",
        "- Production `powLimit` upper bound and `SetCompact` fNegative / fOverflow.",
        "- Production `GetNextWorkASERT` clamps `exp_q` to `±ASERT_EXP_CLAMP`, then",
        "  256-bit scale / clamp / compact nBits. The model compares unclamped `exp_q_raw`.",
        "- Running `S_RES` scalar (C-3): not a model reject and not a C++ reject.",
        "- Classifier ≡ `rigor_classify.cpp` (C-7) is open.",
        "",
        "`MATCH` means the two implementations agreed. It is **not** a C++",
        "refinement and **not** an independent correctness assertion.",
        "",
        f"DIFF_VERIFY={'PASS' if ok else 'FAIL'}",
        "",
    ]
    REPORT.write_text("\n".join(lines))
    print(f"wrote {REPORT}")
    print("DIFF_VERIFY=" + ("PASS" if ok else "FAIL"))
    print("PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
