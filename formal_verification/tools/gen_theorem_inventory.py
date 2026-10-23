#!/usr/bin/env python3
"""Rebuild private_docs/audits/FORMAL_THEOREM_INVENTORY.json from Coq sources.

Scans every `.v` listed in formal_verification/coq/_CoqProject for top-level
Theorem / Lemma / Corollary names. Slice assignment is path-heuristic and must
stay aligned with the coverage-audit inventory:

  Phase4* / Phase5*     → phase_catalog
  Kernel/*              → kernel_harness
  ReplayReorgCore/*     → replay
  FeeBurn/*             → replay
  ConnectBlock/*        → protocol_core  (not replay)
  Proofs/*              → proofs
  DrawBridgeProgress.v  → protocol_core  (root module only)
  everything else       → protocol_core
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
COQ_DIR = ROOT / "formal_verification" / "coq"
COQ_PROJECT = COQ_DIR / "_CoqProject"
# Moved out of docs/security by ccacc451 so the name list stays in the
# private maintenance tree. The public export allowlist does not include it.
DEFAULT_OUT = ROOT / "private_docs" / "audits" / "FORMAL_THEOREM_INVENTORY.json"

DECL_RE = re.compile(
    r"^\s*(Theorem|Lemma|Corollary)\s+([A-Za-z_][A-Za-z0-9_']*)",
    re.MULTILINE,
)


def coq_project_vfiles(project: Path) -> list[str]:
    files: list[str] = []
    for raw in project.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or line.startswith("-"):
            continue
        if line.endswith(".v"):
            files.append(line.replace("\\", "/"))
    return files


def strip_coq_comments(src: str) -> str:
    """Drop nested (* *) comments, keeping newlines so line-start matches survive."""
    out: list[str] = []
    i = 0
    n = len(src)
    while i < n:
        if src.startswith("(*", i):
            depth = 1
            i += 2
            while i < n and depth:
                if src.startswith("(*", i):
                    depth += 1
                    i += 2
                elif src.startswith("*)", i):
                    depth -= 1
                    i += 2
                else:
                    if src[i] == "\n":
                        out.append("\n")
                    i += 1
            continue
        out.append(src[i])
        i += 1
    return "".join(out)


def slice_of(rel: str) -> str:
    posix = rel.replace("\\", "/")
    name = posix.rsplit("/", 1)[-1]
    if posix.startswith("Phase4") or posix.startswith("Phase5"):
        return "phase_catalog"
    if posix.startswith("Kernel/"):
        return "kernel_harness"
    if posix.startswith("ReplayReorgCore/") or posix.startswith("FeeBurn/"):
        return "replay"
    if posix.startswith("ConnectBlock/"):
        return "protocol_core"
    if posix.startswith("Proofs/"):
        return "proofs"
    if name == "DrawBridgeProgress.v":
        return "protocol_core"
    return "protocol_core"


def scan_file(coq_dir: Path, rel: str) -> list[dict[str, str]]:
    text = strip_coq_comments((coq_dir / rel).read_text(encoding="utf-8"))
    sl = slice_of(rel)
    return [
        {"file": rel, "kind": kind, "name": name, "slice": sl}
        for kind, name in DECL_RE.findall(text)
    ]


def build_inventory(coq_dir: Path = COQ_DIR, project: Path = COQ_PROJECT) -> dict:
    theorems: list[dict[str, str]] = []
    for rel in coq_project_vfiles(project):
        path = coq_dir / rel
        if not path.is_file():
            raise FileNotFoundError(f"_CoqProject entry missing: {rel}")
        theorems.extend(scan_file(coq_dir, rel))
    return {"count": len(theorems), "theorems": theorems}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=DEFAULT_OUT,
        help="JSON path (default: private_docs/audits/FORMAL_THEOREM_INVENTORY.json)",
    )
    args = parser.parse_args()
    payload = build_inventory()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(payload, indent=1) + "\n", encoding="utf-8")
    print(f"wrote {payload['count']} named results to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
