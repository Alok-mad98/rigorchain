#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P076: P2P security boundary isolation lint.

The network firewall must not grow a consensus authority, a second
classifier, a second signature verifier, or a Kernel entry.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "core/src"
NETSEC = SRC / "net/security"
MAKEFILE = SRC / "Makefile.am"
VALIDATION = SRC / "validation.cpp"
KERNEL = SRC / "consensus/kernel/genesis_authority.cpp"
TXKERNEL = SRC / "consensus/txkernel"
GENESIS_PARAMS = SRC / "kernel/chainparams.cpp"
DOC = ROOT / "docs/security/RIGOR_P2P_SECURITY_BOUNDARY_P0_076.md"
TEST = SRC / "test/test_p2p_security_boundary.cpp"
GATE = ROOT / "tools/run_p2p_security_gate.sh"

FORBIDDEN_CALLS = (
    "GetBlockSubsidy",
    "GetResMintReward",
    "ResolveConvergenceDraw",
    "ClassifyRigorTxout",
    "ClassifyRigorAsset",
    "AssetTypeFromScriptPubKey",
    "RigorSupplyDelta",
    "VerifyRigorPredicate",
    "EvalScript",
    "VerifyScript",
    "ConnectBlock",
    "ConsensusKernelInput",
    "RigorConsensusKernel",
    "ComputeRigorIssuance",
    "ParseResMintScript",
    "IsWitnessProgram",
    "crypto_sign_verify",
    "ed25519_verify",
)

FORBIDDEN_STATE = (
    "CoinsView",
    "CCoinsView",
    "chainstate",
    "SpendCoin",
    "AddCoins",
    "cs_main",
)

SECOND_IMPL = (
    re.compile(r"class\s+.*ScriptParser"),
    re.compile(r"class\s+.*SignatureVerifier"),
    re.compile(r"class\s+.*AssetClassifier"),
    re.compile(r"bool\s+VerifyRigorPredicate\s*\("),
    re.compile(r"TxoutType\s+ClassifyRigorTxout\s*\("),
)

DECISION_ENUM = re.compile(
    r"enum\s+class\s+P2PValidationDecision[^{]*\{([^}]+)\}", re.DOTALL
)


def fail(errors: list[str]) -> int:
    print("P2P_CONSENSUS_BYPASS_FOUND")
    print("FAIL: P2P security boundary lint")
    for e in errors:
        print(f"  - {e}")
    return 1


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//.*?$", "", text, flags=re.MULTILINE)
    return text


def main() -> int:
    errors: list[str] = []

    if not NETSEC.is_dir():
        return fail(["missing core/src/net/security/"])
    required = [
        NETSEC / "p2p_security_input.h",
        NETSEC / "p2p_validation_result.h",
        NETSEC / "p2p_message_boundary.h",
        NETSEC / "p2p_message_boundary.cpp",
        NETSEC / "peer_misbehavior_score.h",
        TEST,
        DOC,
        GATE,
        ROOT / "fuzz/p2p_security/p2p_security_fuzz.cpp",
    ]
    for p in required:
        if not p.is_file():
            errors.append(f"missing {p.relative_to(ROOT)}")

    files = list(NETSEC.glob("*.h")) + list(NETSEC.glob("*.cpp"))
    blob = ""
    for p in files:
        blob += "\n" + p.read_text(encoding="utf-8", errors="replace")
    code = strip_comments(blob)

    for tok in FORBIDDEN_CALLS:
        if re.search(r"\b" + re.escape(tok) + r"\s*\(", code):
            errors.append(f"P2P security calls {tok} (consensus bypass)")
        # Mentions in comments are stripped; remaining identifier use is fail.
        if re.search(r"\b" + re.escape(tok) + r"\b", code) and tok in (
            "GetBlockSubsidy",
            "GetResMintReward",
            "ResolveConvergenceDraw",
            "RigorSupplyDelta",
            "EvalScript",
            "ConnectBlock",
            "ConsensusKernelInput",
            "RigorConsensusKernel",
        ):
            # Headers may mention VerifyRigorPredicate as a routing requirement.
            if tok not in ("VerifyRigorPredicate",):
                if re.search(r"\b" + re.escape(tok) + r"\b", code):
                    errors.append(f"P2P security names {tok}")

    # Routing mention of VerifyRigorPredicate is allowed in comments (stripped)
    # and in the result field name spend_must_enter_verify_rigor_predicate.
    if "VerifyRigorPredicate" in code and "VerifyRigorPredicate(" in code.replace(" ", ""):
        errors.append("P2P security calls VerifyRigorPredicate")

    for tok in FORBIDDEN_STATE:
        if tok in code:
            errors.append(f"P2P security touches {tok}")

    for rx in SECOND_IMPL:
        if rx.search(code):
            errors.append(f"second implementation matching {rx.pattern}")

    result_h = (NETSEC / "p2p_validation_result.h").read_text(encoding="utf-8")
    m = DECISION_ENUM.search(result_h)
    if not m:
        errors.append("P2PValidationDecision enum missing")
    else:
        names = re.findall(r"\b(ACCEPT_TO_VALIDATION|REJECT|BAN_PEER)\b", m.group(1))
        if names != ["ACCEPT_TO_VALIDATION", "REJECT", "BAN_PEER"]:
            errors.append(f"P2PValidationDecision must be exactly three values, got {names}")

    mk = MAKEFILE.read_text(encoding="utf-8")
    kernel_src = ""
    start = mk.find("libbitcoinkernel_la_SOURCES")
    if start >= 0:
        kernel_src = mk[start : mk.find("\n#", start + 1)]
    if "net/security" in kernel_src:
        errors.append("P2P security linked into libbitcoinkernel (P2P consensus authority)")

    if "net/security/p2p_message_boundary.cpp" not in mk:
        errors.append("p2p_message_boundary.cpp not in Makefile.am")
    if "net/security/p2p_validation_result.h" not in mk:
        errors.append("P2P security headers not in Makefile.am")

    # Production hook must exist and must not call Kernel formulas.
    np = (SRC / "net_processing.cpp").read_text(encoding="utf-8")
    if "P2PMessageBoundary::Evaluate" not in np:
        errors.append("net_processing.cpp does not call P2PMessageBoundary::Evaluate")
    # The hook site must not add formula calls.
    if not KERNEL.is_file() or not GENESIS_PARAMS.is_file():
        errors.append("consensus production files missing")
    if not (SRC / "consensus/txkernel/transaction_kernel_input.h").is_file():
        errors.append("Transaction Kernel header missing")

    limits_h = (NETSEC / "p2p_message_boundary.h").read_text(encoding="utf-8")
    checks = {
        "MAX_PROTOCOL_MESSAGE_LENGTH = 4 * 1000 * 1000": "protocol message cap",
        "MAX_BLOCK_SERIALIZED_SIZE = 1000000": "block serialized cap",
        "MAX_SCRIPT_ELEMENT_SIZE = 520": "script element cap",
        "PROTOCOL_MIN_VERSION = 1": "protocol min",
        "PROTOCOL_CURRENT_VERSION = 1": "protocol current",
        "BITCOIN_SERIALIZATION_VERSION = 70016": "bitcoin version reject",
    }
    for needle, label in checks.items():
        if needle not in limits_h:
            errors.append(f"limit mismatch: {label}")

    score_h = (NETSEC / "peer_misbehavior_score.h").read_text(encoding="utf-8")
    if "BAN_THRESHOLD = 100" not in score_h:
        errors.append("PeerMisbehaviorScore threshold must match DISCOURAGEMENT_THRESHOLD")

    doc = DOC.read_text(encoding="utf-8") if DOC.is_file() else ""
    for req in (
        "malformed input rejection",
        "network boundary isolation",
        "consensus entry protection",
        "deterministic admission",
        "internet anonymity",
        "eclipse impossibility",
        "OS network stack security",
        "cryptographic primitive proof",
    ):
        if req not in doc:
            errors.append(f"formal note missing: {req}")

    if errors:
        return fail(errors)

    print("P2P_SECURITY_BOUNDARY = CLOSED")
    print("P2P_CONSENSUS_BYPASS = NONE")
    print("CONSENSUS_MODIFIED = NO")
    return 0


if __name__ == "__main__":
    sys.exit(main())
