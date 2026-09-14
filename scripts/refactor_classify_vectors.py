#!/usr/bin/env python3
"""Split the consensus golden vectors into a refactor-invariant group and an
affected group, and write the result to formal_verification/refactor_baseline/.

Group I (invariant) is the guard: every vector in it must stay byte-identical
through every phase of the RES issuance refactor. A single differing bit means
the change escaped the RES boundary, which is a hard stop.

Group II (affected) is not a failure list. These vectors encode the lottery
(sigma, the 32-block window, argmin selection, the per-mint reward W) or the
reward schedule, and they are expected to be regenerated once the new mechanism
lands. They are enumerated here so that "regenerated" can be checked against a
fixed denominator rather than against whatever the tree happens to contain.

Classification is by module, not by keyword search over the payload: every
vector file mentions RES somewhere, so a payload grep classifies everything as
affected and is useless as a guard.
"""
import argparse
import hashlib
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
FV = ROOT / "formal_verification"
OUT = FV / "refactor_baseline"

# Modules whose semantics do not mention issuance at all. Hash primitives, the
# purely syntactic script->asset classifier, block traces and the replay seed
# expansion. The classifier stays because OP_CONVERGE still exists (ORU is
# still burned) and because the script-prefix-to-asset map is syntax only: it
# does not care which transaction carries the RES output.
INVARIANT_MODULES = {
    "HashKernel",
    "Sha256Fips",
    "ScriptAsset",
    "ScriptMintNeg",
    "BlockTrace",
    "Replay",
}

# Modules that encode the draw itself, or the per-mint reward W(k).
AFFECTED_MODULES = {
    "RunSelection",
    "WindowExclusion",
    "WindowIncomplete",
    "SigmaPoW01",
    "SigmaEncoder",
    "DrawCandidateRoot",
    "ConvergenceRoot",
    "Inequality",
    "EdgeCross",
    "InvalidCvg",
    "ReorgBind",
    "NegativeCore",
    "SupplyScaffold",
}

# Vectors whose expected value does not merely change scale but inverts. These
# are called out separately because a regeneration script that only rescales
# amounts would silently keep them wrong.
POLARITY_FLIPS = {
    "NEG-002": (
        "oru_only_creates_res currently expects reject=mint_wrong_amount. Under a "
        "deterministic destroy cost this is the legal mint shape: an ordinary "
        "transaction that destroys ORU and creates RES. The expected verdict "
        "inverts from reject to accept, conditional on the burn covering "
        "res_out * RATE(height)."
    ),
    "NEG-013": (
        "without_winner_mint expects reject=without_winner. The winner concept "
        "does not survive; the vector has no referent and must be replaced by a "
        "burn-insufficiency vector rather than rescaled."
    ),
}


def classify_phase3(path):
    doc = json.loads(path.read_text())
    inv, aff, unknown = [], [], []
    for v in doc["vectors"]:
        mod = v["module"]
        entry = {"id": v["id"], "module": mod, "name": v["name"]}
        if mod in INVARIANT_MODULES:
            inv.append(entry)
        elif mod in AFFECTED_MODULES:
            aff.append(entry)
        else:
            unknown.append(entry)
    return doc, inv, aff, unknown


def invariant_digest(doc):
    """Fingerprint of the full content of the invariant vectors.

    The id/module/name listing alone would not notice an expected value being
    edited, which is exactly the failure this guard exists to catch. Hash the
    whole vector object, ordered by id so the digest does not depend on the
    order they happen to sit in the file.
    """
    keep = sorted((v for v in doc["vectors"] if v["module"] in INVARIANT_MODULES),
                  key=lambda v: v["id"])
    blob = json.dumps(keep, sort_keys=True, separators=(",", ":")).encode()
    return hashlib.sha256(blob).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="fail if the on-disk classification differs from what this run produces")
    args = ap.parse_args()

    src = FV / "phase3_golden_vectors.json"
    doc, inv, aff, unknown = classify_phase3(src)

    if unknown:
        print("unclassified modules present; extend INVARIANT_MODULES or AFFECTED_MODULES:",
              file=sys.stderr)
        for e in unknown:
            print(f"  {e['id']} {e['module']}", file=sys.stderr)
        return 2

    result = {
        "source": src.name,
        "source_version": doc["version"],
        "source_count": doc["count"],
        "invariant_modules": sorted(INVARIANT_MODULES),
        "affected_modules": sorted(AFFECTED_MODULES),
        "invariant": inv,
        "invariant_sha256": invariant_digest(doc),
        "affected": aff,
        "polarity_flips": POLARITY_FLIPS,
        # Whole files that are regenerated wholesale rather than per vector.
        "affected_files": [
            "rigor_golden_vectors.json",
            "frozen_consensus_vectors.json",
            "genesis_consensus_vectors.json",
            "release_consensus_vectors.json",
            "consensus_kernel_refinement_vectors.json",
            "adversarial_security_vectors.json",
        ],
    }

    OUT.mkdir(exist_ok=True)
    target = OUT / "vector_classification.json"
    payload = json.dumps(result, indent=2, sort_keys=True) + "\n"

    if args.check:
        if not target.exists():
            print(f"FAIL: {target} missing; run without --check", file=sys.stderr)
            return 1
        if target.read_text() != payload:
            print(f"FAIL: {target} is not what this script produces", file=sys.stderr)
            return 1
        print(f"OK: classification reproducible "
              f"({len(inv)} invariant, {len(aff)} affected)")
        return 0

    target.write_text(payload)
    total = len(inv) + len(aff)
    print(f"wrote {target}")
    print(f"  invariant : {len(inv):>3} / {total}  ({len(inv)/total:.0%})")
    print(f"  affected  : {len(aff):>3} / {total}  ({len(aff)/total:.0%})")
    print(f"  polarity flips called out: {', '.join(sorted(POLARITY_FLIPS))}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
