#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Pin shipping freeze locks. Text-only. No compile, no Coq, no network.

Reproduce:
  python3 tools/lint-mainnet-freeze-locks.py
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LOCK = ROOT / "core/src/consensus/rigor_consensus_lock.h"
ISSUANCE = ROOT / "core/src/consensus/res_issuance.h"
SUBSIDY = ROOT / "core/src/consensus/rigor_issuance.cpp"
CONSENSUS_H = ROOT / "core/src/consensus/consensus.h"
CHAIN = ROOT / "core/src/kernel/chainparams.cpp"
POW = ROOT / "core/src/rigor_kernel/pow_digest.h"
AMOUNT = ROOT / "core/src/consensus/amount.h"
FREEZE = ROOT / "docs/protocol/MAINNET_PROTOCOL_FREEZE_DECLARATION.md"

GENESIS = {
    "main": "f8aceb50d974f3157c7bc2dfffab28dee0c97d99e4e3ec931b84e06862fdd717",
    "test": "0aca439758fba7fa5190b7e1cf37f9447516ef4cade07a49e7d484ea5b67b9ce",
    "regtest": "3070f47e2c1b153a3288382f3258396bb48b016eab76eae9f235d006dcc2af63",
    "devtest": "9762f59ecfe1050ca45e404dc95e32ae5a93b9a046e116f2f2f562e44f21dce0",
    "slowtest": "18ddf722cf24b35ad9e3b548ddba22a2e54535fd5932d4d699fa9602f6d3efb8",
}

HEADER_FIELDS = re.compile(
    r"\bnBits\b|\bnTime\b|\bGetNextWorkRequired\b|\bCalculateASERT\b|"
    r"\baserti3\b|\bSetCompact\b|\bGetBlockTime\b|\bVersionBits\b|"
    r"\bnSubsidyHalvingInterval\b|\bDeploymentPos\b",
    re.I,
)


def fail(errors: list[str]) -> int:
    print("MAINNET_FREEZE_LOCK_VIOLATION")
    for e in errors:
        print(f"  - {e}")
    return 1


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//.*?$", "", text, flags=re.MULTILINE)


def cpp_int(path: Path, name: str, coin: int) -> int:
    text = path.read_text(encoding="utf-8")
    m = re.search(
        rf"(?:inline\s+constexpr|static\s+const)[^=;{{]*\b{name}\s*=\s*([^;]+);",
        text,
    )
    if not m:
        m = re.search(
            rf"(?:inline\s+constexpr|static\s+const)[^=;{{]*\b{name}\s*\{{\s*([^}}]+)\s*\}};",
            text,
        )
    if not m:
        raise SystemExit(f"FAIL: missing {name} in {path.relative_to(ROOT)}")
    expr = m.group(1).strip()
    expr = re.sub(r"\bCOIN\b", str(coin), expr)
    expr = re.sub(
        r"\b1(?:ULL|LL|ull|ll)?\s*<<\s*(\d+)",
        lambda mm: str(1 << int(mm.group(1))),
        expr,
    )
    expr = expr.replace("ULL", "").replace("LL", "").replace("ull", "").replace("ll", "")
    expr = re.sub(r"\s+", "", expr)
    if not re.fullmatch(r"[0-9*+\-]+", expr):
        raise SystemExit(f"FAIL: cannot evaluate {name} = {expr}")
    return int(eval(expr, {"__builtins__": {}}, {}))  # noqa: S307 — digits and *+ only


def extract_fn(text: str, signature: str) -> str:
    start = text.find(signature)
    if start < 0:
        return ""
    brace = text.find("{", start)
    if brace < 0:
        return ""
    depth = 0
    for i, ch in enumerate(text[brace:], brace):
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return text[brace : i + 1]
    return ""


def coin_value() -> int:
    m = re.search(
        r"static\s+constexpr\s+CAmount\s+COIN\s*=\s*(\d+)",
        AMOUNT.read_text(encoding="utf-8"),
    )
    if not m:
        raise SystemExit("FAIL: missing COIN")
    return int(m.group(1))


def main() -> int:
    errors: list[str] = []
    coin = coin_value()
    lock = LOCK.read_text(encoding="utf-8")
    issuance = ISSUANCE.read_text(encoding="utf-8")
    subsidy = SUBSIDY.read_text(encoding="utf-8")
    consensus_h = CONSENSUS_H.read_text(encoding="utf-8")
    chain = CHAIN.read_text(encoding="utf-8")
    pow_h = POW.read_text(encoding="utf-8")
    freeze = FREEZE.read_text(encoding="utf-8")

    pins = {
        (LOCK, "RES_COIN", 100_000_000),
        (LOCK, "RES_MAX_SUPPLY", 419_430_400_000_000),
        (LOCK, "RES_DESTROY_POLE", 33_554_432),
        (LOCK, "RES_DESTROY_SHIFT", 40),
        (LOCK, "ORU_FLAT_BLOCK_SUBSIDY", 128 * coin),
        (LOCK, "ORU_GENESIS_BLOCK_SUBSIDY", 0),
        (LOCK, "MIN_CONVERGENCE_AMOUNT", 1),
        (LOCK, "RIGOR_CONSENSUS_LOCK_VERSION", 3),
        (LOCK, "ORU_RES_TRANSFER_FEE_BURN_NUM", 1),
        (LOCK, "ORU_RES_TRANSFER_FEE_BURN_DEN", 2),
        (LOCK, "BLOCK_SERIALIZED_SIZE_LIMIT", 1_000_000),
        (LOCK, "BLOCK_WEIGHT_LIMIT", 1_000_000),
        (LOCK, "BLOCK_SIGOPS_COST_LIMIT", 20_000),
        (LOCK, "MAINNET_POW_ASERT_HALF_LIFE", 43_200),
        (LOCK, "TARGET_BLOCK_TIME", 600),
        (ISSUANCE, "RES_DESTROY_COST_GENESIS", 1024 * coin),
        (CONSENSUS_H, "COINBASE_MATURITY", 100),
    }
    for path, name, expected in pins:
        try:
            got = cpp_int(path, name, coin)
        except SystemExit as exc:
            errors.append(str(exc))
            continue
        if got != expected:
            errors.append(f"{path.relative_to(ROOT)} {name}={got}, freeze requires {expected}")

    if 'inline constexpr const char* CHAIN_NAME = "RIGOR"' not in lock:
        errors.append("CHAIN_NAME must remain RIGOR")
    if 'inline constexpr const char* BECH32_HRP = "rg"' not in lock:
        errors.append("BECH32_HRP must remain rg")

    mint = extract_fn(strip_comments(issuance), "CheckResMintArithmetic")
    if not mint:
        errors.append("CheckResMintArithmetic body missing")
    else:
        if "RES_DESTROY_POLE" not in mint or "RES_DESTROY_SHIFT" not in mint:
            errors.append("CheckResMintArithmetic must keep the pole-shift inequality")
        compacted = re.sub(r"\s+", "", mint)
        if "required<=supplied" not in compacted:
            errors.append("CheckResMintArithmetic must keep required <= supplied")
        if HEADER_FIELDS.search(mint):
            errors.append("CheckResMintArithmetic must not read header, ASERT, or nBits")

    cost = extract_fn(strip_comments(issuance), "ResDestroyCostAtHeight")
    if cost and HEADER_FIELDS.search(cost):
        errors.append("ResDestroyCostAtHeight must not read header, ASERT, or nBits")
    if "!CheckResMintArithmetic(1,0,0)" not in re.sub(r"\s+", "", issuance):
        errors.append("genesis one-atom mint against zero burn must stay rejected")
    if "ResDestroyCostAtHeight(0) == RES_DESTROY_COST_GENESIS" not in issuance:
        errors.append("display cost at height 0 must stay genesis 1024 ORU")

    sub = extract_fn(strip_comments(subsidy), "CAmount GetBlockSubsidy")
    if not sub:
        errors.append("GetBlockSubsidy body missing")
    else:
        if "ORU_GENESIS_BLOCK_SUBSIDY" not in sub or "ORU_FLAT_BLOCK_SUBSIDY" not in sub:
            errors.append("GetBlockSubsidy must return the two lock constants")
        if HEADER_FIELDS.search(sub) or ">>" in sub:
            errors.append("GetBlockSubsidy must stay genesis-0 / flat 128, no halving")

    if "static const int RES_MINT_MATURITY = COINBASE_MATURITY" not in consensus_h:
        errors.append("RES_MINT_MATURITY must stay equal to COINBASE_MATURITY")

    for label, digest in GENESIS.items():
        if f'uint256S("0x{digest}")' not in chain:
            errors.append(f"missing {label} genesis {digest}")

    if "POW_DOMAIN_TAG[] = {'R', 'G', 'R', 'P', 'O', 'W', '0', '1'}" not in pow_h:
        errors.append("OpuHash domain tag must remain RGRPOW01")
    if "POW_PREIMAGE_LEN == 88" not in pow_h:
        errors.append("OpuHash preimage must remain 88 bytes")

    for needle in (
        "100_000_000",
        "419_430_400_000_000",
        "1024 * COIN",
        "2²⁵ = 33_554_432",
        "RGRPOW01",
        GENESIS["main"],
        "RIGOR_CONSENSUS_LOCK_VERSION = 3",
    ):
        if needle not in freeze:
            errors.append(f"freeze declaration missing {needle!r}")

    if errors:
        return fail(errors)
    print("MAINNET_FREEZE_LOCK=PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
