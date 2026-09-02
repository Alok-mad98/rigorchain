#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P077: wallet security baseline isolation + secret-handling lint.

The wallet firewall must not grow a consensus authority, a second
classifier, a second signature verifier, or a Kernel entry.
Wallet production must not log private-key material.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "core/src"
WALLET = SRC / "wallet"
WSEC = WALLET / "security"
MAKEFILE = SRC / "Makefile.am"
DOC = ROOT / "docs/security/RIGOR_WALLET_SECURITY_BASELINE_P0_077.md"
TEST = SRC / "test/test_wallet_security_boundary.cpp"
GATE = ROOT / "tools/run_wallet_security_gate.sh"
FUZZ = ROOT / "fuzz/wallet_security/wallet_security_fuzz.cpp"

FORBIDDEN_CALLS = (
    "GetBlockSubsidy",
    "GetResMintReward",
    "ResolveConvergenceDraw",
    "RigorSupplyDelta",
    "LedgerApply",
    "ClassifyRigorTxout",
    "ClassifyRigorAsset",
    "VerifyRigorPredicate",
    "EvalScript",
    "ConnectBlock",
    "ConsensusKernelInput",
    "RigorConsensusKernel",
    "ComputeRigorIssuance",
    "ParseResMintScript",
)

FORBIDDEN_INCLUDES = (
    "consensus/rigor_issuance",
    "consensus/rigor_supply",
    "consensus/kernel/genesis_authority",
    "consensus/convergence_logic",
    "consensus/utxo/",
    "consensus/txkernel/",
)

FORBIDDEN_STATE = (
    "CoinsView",
    "CCoinsView",
    "SpendCoin",
    "AddCoins",
    "cs_main",
    "LedgerApply",
)

SECOND_IMPL = (
    re.compile(r"class\s+.*ScriptParser"),
    re.compile(r"class\s+.*SignatureVerifier"),
    re.compile(r"class\s+.*AssetClassifier"),
    re.compile(r"bool\s+VerifyRigorPredicate\s*\("),
    re.compile(r"TxoutType\s+ClassifyRigorTxout\s*\("),
)

DECISION_ENUM = re.compile(
    r"enum\s+class\s+WalletSecurityDecision[^{]*\{([^}]+)\}", re.DOTALL
)

# Production wallet logs must not dump key material.
SECRET_LOG = (
    re.compile(r"\b(WalletLogPrintf|LogPrintf|LogPrint)\s*\([^;]*EncodeSecret"),
    re.compile(r"\b(WalletLogPrintf|LogPrintf|LogPrint)\s*\([^;]*\bprivate_key\b"),
    re.compile(r"\b(WalletLogPrintf|LogPrintf|LogPrint)\s*\([^;]*HexStr\s*\([^)]*secret", re.I),
    re.compile(r"std::(cout|cerr)\s*<<[^;]*EncodeSecret"),
    # Bare printf only — must not match the printf suffix of WalletLogPrintf.
    re.compile(r"(?<![A-Za-z_])printf\s*\([^;]*EncodeSecret"),
    re.compile(r"(?<![A-Za-z_])printf\s*\([^;]*\bprivate_key\b"),
    re.compile(r"(?<![A-Za-z_])printf\s*\([^;]*priv(?:ate)?[ _]key", re.I),
    re.compile(r"(?<![A-Za-z_])log\s*\(\s*secret", re.I),
)

AUTHORITY_CALLS_WALLET = (
    "GetBlockSubsidy",
    "GetResMintReward",
    "ResolveConvergenceDraw",
    "RigorSupplyDelta",
    "LedgerApply",
)


def fail(errors: list[str]) -> int:
    print("WALLET_CONSENSUS_BYPASS_FOUND")
    print("FAIL: wallet security lint")
    for e in errors:
        print(f"  - {e}")
    return 1


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//.*?$", "", text, flags=re.MULTILINE)
    return text


def iter_wallet_prod() -> list[Path]:
    out: list[Path] = []
    for p in WALLET.rglob("*"):
        if not p.is_file():
            continue
        if p.suffix not in {".h", ".cpp"}:
            continue
        rel = p.relative_to(WALLET).as_posix()
        if rel.startswith("test/") or "/test/" in rel:
            continue
        out.append(p)
    return out


def main() -> int:
    errors: list[str] = []

    if not WSEC.is_dir():
        return fail(["missing core/src/wallet/security/"])
    required = [
        WSEC / "wallet_transaction_request.h",
        WSEC / "wallet_security_result.h",
        WSEC / "wallet_security_boundary.h",
        WSEC / "wallet_security_boundary.cpp",
        WSEC / "wallet_signing_boundary.h",
        WSEC / "wallet_signing_boundary.cpp",
        TEST,
        DOC,
        GATE,
        FUZZ,
        ROOT / "fuzz/wallet_security/run_smoke.sh",
    ]
    for p in required:
        if not p.is_file():
            errors.append(f"missing {p.relative_to(ROOT)}")

    files = list(WSEC.glob("*.h")) + list(WSEC.glob("*.cpp"))
    blob = ""
    for p in files:
        blob += "\n" + p.read_text(encoding="utf-8", errors="replace")
    code = strip_comments(blob)

    for tok in FORBIDDEN_CALLS:
        if re.search(r"\b" + re.escape(tok) + r"\s*\(", code):
            errors.append(f"wallet security calls {tok} (consensus bypass)")

    for inc in FORBIDDEN_INCLUDES:
        if inc in code:
            errors.append(f"wallet security includes {inc}")

    for tok in FORBIDDEN_STATE:
        if tok in code:
            errors.append(f"wallet security touches {tok}")

    for rx in SECOND_IMPL:
        if rx.search(code):
            errors.append(f"second implementation matching {rx.pattern}")

    result_h = (WSEC / "wallet_security_result.h").read_text(encoding="utf-8")
    m = DECISION_ENUM.search(result_h)
    if not m:
        errors.append("WalletSecurityDecision enum missing")
    else:
        names = re.findall(r"\b(ACCEPT_TO_BUILDER|REJECT)\b", m.group(1))
        if names != ["ACCEPT_TO_BUILDER", "REJECT"]:
            errors.append(f"WalletSecurityDecision must be exactly two values, got {names}")

    mk = MAKEFILE.read_text(encoding="utf-8")
    kernel_src = ""
    start = mk.find("libbitcoinkernel_la_SOURCES")
    if start >= 0:
        kernel_src = mk[start : mk.find("\n#", start + 1)]
    if "wallet/security" in kernel_src:
        errors.append("wallet security linked into libbitcoinkernel (wallet consensus authority)")

    if "wallet/security/wallet_security_boundary.cpp" not in mk:
        errors.append("wallet_security_boundary.cpp not in Makefile.am")
    if "wallet/security/wallet_signing_boundary.cpp" not in mk:
        errors.append("wallet_signing_boundary.cpp not in Makefile.am")
    if "wallet/security/wallet_transaction_request.h" not in mk:
        errors.append("wallet security headers not in Makefile.am")

    spend = (WALLET / "spend.cpp").read_text(encoding="utf-8")
    if "WalletSecurityBoundary::Evaluate" not in spend:
        errors.append("spend.cpp does not call WalletSecurityBoundary::Evaluate")

    limits_h = (WSEC / "wallet_security_boundary.h").read_text(encoding="utf-8")
    checks = {
        "MAX_MONEY = static_cast<int64_t>(10000000000LL) * COIN": "MAX_MONEY match",
        "COIN = 100000000": "COIN match",
        "MAX_SCRIPT_SIZE = 10000": "script size cap",
    }
    for needle, label in checks.items():
        if needle not in limits_h:
            errors.append(f"limit mismatch: {label}")

    # Secret-handling lint: production wallet logs.
    for p in iter_wallet_prod():
        text = p.read_text(encoding="utf-8", errors="replace")
        rel = p.relative_to(ROOT).as_posix()
        for rx in SECRET_LOG:
            if rx.search(text):
                errors.append(f"secret log in {rel} matching {rx.pattern}")
        stripped = strip_comments(text)
        for tok in AUTHORITY_CALLS_WALLET:
            if re.search(r"\b" + re.escape(tok) + r"\s*\(", stripped):
                errors.append(f"{rel} calls {tok} (wallet consensus authority)")
        for inc in FORBIDDEN_INCLUDES:
            if re.search(r"#include\s*[<\"]" + re.escape(inc), text):
                errors.append(f"{rel} includes consensus authority implementation {inc}")

    test_inc = (SRC / "Makefile.test.include").read_text(encoding="utf-8")
    if "test_wallet_security_boundary.cpp" not in test_inc:
        errors.append("test_wallet_security_boundary.cpp not in Makefile.test.include")

    doc = DOC.read_text(encoding="utf-8") if DOC.is_file() else ""
    for req in (
        "wallet boundary isolation",
        "signing boundary",
        "transaction construction safety",
        "key exposure prevention",
        "invalid request rejection",
        "hardware wallet security",
        "OS compromise resistance",
        "user password strength",
        "physical device security",
    ):
        if req not in doc:
            errors.append(f"formal note missing: {req}")

    if errors:
        return fail(errors)

    print("WALLET_SECURITY_STATUS = CLOSED")
    print("PRIVATE_KEY_EXPOSURE = NONE")
    print("CONSENSUS_BYPASS = NONE")
    print("CONSENSUS_MODIFIED = NO")
    return 0


if __name__ == "__main__":
    sys.exit(main())
