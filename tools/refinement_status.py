#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Single-source kernel / refinement status.

Four independent facts. PRODUCTION_KERNEL_SWITCHED is retired and must not
be reintroduced: it was used both for "C++ kernel is production" and
"extracted kernel is not production".

    PRODUCTION_CONSENSUS_AUTHORITY     = RigorConsensusKernel(C++)
    FORMAL_REFINEMENT_STATUS           = PROVED
    EXTRACTED_KERNEL_PRODUCTION        = FALSE
    COMPCERT_BINARY_VERIFIED_KERNEL    = FALSE

C++ is the unique production authority. Coq is the specification and
refinement proof. A later extracted-kernel cutover gets its own flag
(EXTRACTED_KERNEL_PRODUCTION), not a reuse of the retired name.

COQ_TO_CPP and EQUIVALENCE_PROVED stay as honesty lines: refinement of
the Coq model is not a proof that C++ equals Coq, and not CompCert.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "core/src/consensus/kernel/genesis_authority.h"

AUTHORITY = "RigorConsensusKernel(C++)"
FORMAL_REFINEMENT_STATUS = "PROVED"

AUTHORITY_RE = re.compile(
    r'inline\s+constexpr\s+(?:const\s+)?char\s+PRODUCTION_CONSENSUS_AUTHORITY\[\]\s*=\s*"([^"]+)"\s*;'
)
EXTRACTED_RE = re.compile(
    r"inline\s+constexpr\s+bool\s+EXTRACTED_KERNEL_PRODUCTION\s*=\s*(true|false)\s*;"
)
COMPCERT_RE = re.compile(
    r"inline\s+constexpr\s+bool\s+COMPCERT_BINARY_VERIFIED_KERNEL\s*=\s*(true|false)\s*;"
)
RETIRED_RE = re.compile(
    r"inline\s+constexpr\s+bool\s+PRODUCTION_KERNEL_SWITCHED\s*="
)


class StatusError(ValueError):
    """Header / JSON status is inconsistent with the four frozen facts."""


def _once(pattern: re.Pattern[str], text: str, name: str) -> str:
    matches = pattern.findall(text)
    if len(matches) != 1:
        raise StatusError(f"FAIL: {name} must appear once in {HEADER}")
    return matches[0]


def load_header(header: Path | None = None) -> tuple[str, bool, bool]:
    path = header or HEADER
    text = path.read_text(encoding="utf-8")
    if RETIRED_RE.search(text):
        raise StatusError(
            "FAIL: PRODUCTION_KERNEL_SWITCHED was reintroduced; "
            "use EXTRACTED_KERNEL_PRODUCTION / PRODUCTION_CONSENSUS_AUTHORITY"
        )
    authority = _once(AUTHORITY_RE, text, "PRODUCTION_CONSENSUS_AUTHORITY")
    extracted = _once(EXTRACTED_RE, text, "EXTRACTED_KERNEL_PRODUCTION") == "true"
    compcert = _once(COMPCERT_RE, text, "COMPCERT_BINARY_VERIFIED_KERNEL") == "true"
    if authority != AUTHORITY:
        raise StatusError(f"FAIL: PRODUCTION_CONSENSUS_AUTHORITY={authority!r} want {AUTHORITY!r}")
    if extracted:
        raise StatusError("FAIL: EXTRACTED_KERNEL_PRODUCTION must be false")
    if compcert:
        raise StatusError("FAIL: COMPCERT_BINARY_VERIFIED_KERNEL must be false")
    return authority, extracted, compcert


def status_lines() -> list[str]:
    authority, extracted, compcert = load_header()
    return [
        f"PRODUCTION_CONSENSUS_AUTHORITY={authority}",
        f"FORMAL_REFINEMENT_STATUS={FORMAL_REFINEMENT_STATUS}",
        f"EXTRACTED_KERNEL_PRODUCTION={'TRUE' if extracted else 'FALSE'}",
        f"COMPCERT_BINARY_VERIFIED_KERNEL={'TRUE' if compcert else 'FALSE'}",
        "COQ_TO_CPP=VECTOR-TESTED",
        "EQUIVALENCE_PROVED=false",
    ]


def print_status() -> None:
    for line in status_lines():
        print(line)


def json_status_fields() -> dict:
    return {
        "production_consensus_authority": AUTHORITY,
        "formal_refinement_status": FORMAL_REFINEMENT_STATUS,
        "extracted_kernel_production": False,
        "compcert_binary_verified_kernel": False,
    }


def _pick(obj: dict, *keys: str) -> tuple[bool, object]:
    for k in keys:
        if k in obj:
            return True, obj[k]
    return False, None


def cpp_authority_reason(obj: dict) -> tuple[bool, str]:
    """Require all four status fields, with C++ as production and extracted/compcert false."""
    has_a, authority = _pick(obj, "production_consensus_authority", "PRODUCTION_CONSENSUS_AUTHORITY")
    if not has_a:
        return False, "missing production_consensus_authority"
    has_f, formal = _pick(obj, "formal_refinement_status", "FORMAL_REFINEMENT_STATUS")
    if not has_f:
        return False, "missing formal_refinement_status"
    has_e, extracted = _pick(obj, "extracted_kernel_production", "EXTRACTED_KERNEL_PRODUCTION")
    if not has_e:
        return False, "missing extracted_kernel_production"
    has_c, compcert = _pick(obj, "compcert_binary_verified_kernel", "COMPCERT_BINARY_VERIFIED_KERNEL")
    if not has_c:
        return False, "missing compcert_binary_verified_kernel"
    if authority != AUTHORITY:
        return False, f"authority={authority!r}"
    if formal != FORMAL_REFINEMENT_STATUS:
        return False, f"formal_refinement_status={formal!r}"
    if extracted is not False:
        return False, f"extracted_kernel_production={extracted!r}"
    if compcert is not False:
        return False, f"compcert_binary_verified_kernel={compcert!r}"
    return True, ""


def records_cpp_authority(obj: dict) -> bool:
    """True iff obj names C++ RigorConsensusKernel as production and extracted as not."""
    ok, _ = cpp_authority_reason(obj)
    return ok


def main() -> int:
    try:
        print_status()
    except StatusError as e:
        print(e)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
