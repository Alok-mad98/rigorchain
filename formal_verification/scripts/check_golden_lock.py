#!/usr/bin/env python3
"""Fail-closed lock for golden-vector / Coq-harness counts and L1/L2 splits.

.
.

The differential gate must not stay green if a later change silently drops a
locked positive or negative family, if JSON and harness lemma counts drift,
or if a named L1 vs L2 surface split is dropped, moved, or demoted.

This is not a refinement proof. Honesty flags must remain false.

Usage (from repo root or formal_verification/):
  python3 formal_verification/scripts/check_golden_lock.py
"""
from __future__ import annotations

import json
import re
import sys
from collections import defaultdict
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
FV_DIR = SCRIPT_DIR.parent
REPO_ROOT = FV_DIR.parent

LOCK_PATH = FV_DIR / "golden_lock.json"
JSON_PATH = FV_DIR / "phase3_golden_vectors.json"
INDEX_PATH = FV_DIR / "coq_harness_index.json"
GOLDEN_HARNESS = FV_DIR / "coq" / "Kernel" / "GoldenHarness.v"
NEG_HARNESS = FV_DIR / "coq" / "Kernel" / "NegativeCoreHarness.v"
HONESTY_LOCK = FV_DIR / "coq" / "DrawBridgeProgress.v"
# Phase 5 archived the lottery Phase5Equivalence tree. Prefer the live
# operator-path stub; fall back to the archive copy.
_HONESTY_PHASE5_LIVE = FV_DIR / "coq" / "Phase5Equivalence" / "DrawBridgeProgress.v"
_HONESTY_PHASE5_ARCHIVED = (
    FV_DIR / "archive" / "lottery" / "coq" / "Phase5Equivalence" / "DrawBridgeProgress.v"
)
HONESTY_PHASE5 = (
    _HONESTY_PHASE5_LIVE if _HONESTY_PHASE5_LIVE.is_file() else _HONESTY_PHASE5_ARCHIVED
)

LEMMA_RE = re.compile(r"^Lemma golden_(\w+) : chk_\d+ = true\.", re.M)
ALLOWED_DISPOSITIONS = frozenset({"keep", "not_this_cycle"})


def fail(msg: str, errors: list[str]) -> None:
    errors.append(msg)


def expected_ids(prefix: str, first: int, last: int) -> list[str]:
    width = 3
    return [f"{prefix}{i:0{width}d}" for i in range(first, last + 1)]


def count_lemmas(path: Path) -> tuple[int, set[str]]:
    text = path.read_text(encoding="utf-8")
    names = set(LEMMA_RE.findall(text))
    return len(names), names


def check_honesty(errors: list[str]) -> None:
    lock = HONESTY_LOCK.read_text(encoding="utf-8")
    phase5 = HONESTY_PHASE5.read_text(encoding="utf-8")
    if "equivalence_claimed = false" not in lock:
        fail("honesty: equivalence_claimed is not false in DrawBridgeProgress.v", errors)
    if "hash_kernel_shared_binary_with_cpp = false" not in lock:
        fail(
            "honesty: hash_kernel_shared_binary_with_cpp is not false in DrawBridgeProgress.v",
            errors,
        )
    if "refinement_proved : bool := false" not in phase5:
        fail(
            "honesty: refinement_proved is not false in Phase5Equivalence/DrawBridgeProgress.v",
            errors,
        )


def check_surface_splits(
    lock: dict,
    vectors: list,
    index: list,
    errors: list[str],
) -> None:
    """Pin named L1/L2 splits so they cannot silently disappear."""
    splits = lock.get("surface_splits")
    if not isinstance(splits, dict):
        fail("golden_lock.json missing surface_splits (bump with a discrepancy task)", errors)
        return

    vec_by_id = {v["id"]: v for v in vectors}
    index_by_id = {row[1]: row for row in index}

    python_only = splits.get("python_only_families")
    if not isinstance(python_only, list) or not python_only:
        fail("surface_splits.python_only_families missing or empty", errors)
    else:
        for fam in python_only:
            spec = lock["families"].get(fam)
            if spec is None:
                fail(f"python_only family {fam} not in lock families", errors)
                continue
            if spec["coq_enforced"] != 0:
                fail(
                    f"python_only family {fam} has lock coq_enforced={spec['coq_enforced']}; must stay 0",
                    errors,
                )

    omit = splits.get("omit_coq_enforce_key_enforced")
    if not isinstance(omit, list) or not omit:
        fail("surface_splits.omit_coq_enforce_key_enforced missing or empty", errors)
        omit = []
    omit_set = set(omit)
    if len(omit_set) != len(omit):
        fail("surface_splits.omit_coq_enforce_key_enforced has duplicate ids", errors)
    if len(omit) != lock["coq_enforced_lemmas"] - lock["json_coq_enforce_true"]:
        fail(
            f"omit_coq_enforce_key_enforced length {len(omit)} != "
            f"coq_enforced_lemmas ({lock['coq_enforced_lemmas']}) - "
            f"json_coq_enforce_true ({lock['json_coq_enforce_true']})",
            errors,
        )

    extra_omit = []
    for vid in omit:
        vec = vec_by_id.get(vid)
        row = index_by_id.get(vid)
        if vec is None:
            fail(f"omit-key enforced id missing from JSON: {vid}", errors)
            continue
        if "coq_enforce" in vec:
            fail(
                f"{vid} is listed as omit-key enforced but JSON has coq_enforce="
                f"{vec.get('coq_enforce')!r}",
                errors,
            )
        if row is None or row[0] is None:
            fail(f"{vid} is listed as omit-key enforced but is not Coq-enforced", errors)

    for v in vectors:
        vid = v["id"]
        row = index_by_id.get(vid)
        enforced = row is not None and row[0] is not None
        if enforced and "coq_enforce" not in v and vid not in omit_set:
            extra_omit.append(vid)
        if (not enforced) and vid in omit_set:
            fail(f"{vid} is omit-key-enforced in the lock but harness index is python_only", errors)
    for vid in extra_omit[:20]:
        fail(f"Coq-enforced {vid} omits coq_enforce but is not in omit_coq_enforce_key_enforced", errors)
    if len(extra_omit) > 20:
        fail(f"... and {len(extra_omit) - 20} more unlisted omit-key enforced ids", errors)

    rows = splits.get("rows")
    if not isinstance(rows, list) or not rows:
        fail("surface_splits.rows missing or empty", errors)
        return

    seen_ids: set[str] = set()
    for spec in rows:
        ids = spec.get("ids") or []
        fam = spec.get("family")
        kind = spec.get("harness_kind")
        disp = spec.get("disposition")
        if disp not in ALLOWED_DISPOSITIONS:
            fail(
                f"surface_splits row {fam} {ids}: disposition {disp!r} "
                f"not in {sorted(ALLOWED_DISPOSITIONS)}",
                errors,
            )
        if not ids or not fam or not kind:
            fail(f"surface_splits row missing ids/family/harness_kind: {spec}", errors)
            continue
        for vid in ids:
            if vid in seen_ids:
                fail(f"surface_splits lists {vid} more than once", errors)
            seen_ids.add(vid)
            vec = vec_by_id.get(vid)
            row = index_by_id.get(vid)
            if vec is None:
                fail(f"surface_split {vid} missing from JSON", errors)
                continue
            if vec["module"] != fam:
                fail(f"surface_split {vid} module is {vec['module']!r}, expected {fam}", errors)
            if vec.get("coq_enforce") is False:
                fail(f"surface_split {vid} has coq_enforce:false; must stay enforced", errors)
            if row is None or row[0] is None:
                fail(f"surface_split {vid} not Coq-enforced in harness index", errors)
            elif row[3] != kind:
                fail(
                    f"surface_split {vid} harness kind is {row[3]!r}, lock wants {kind}",
                    errors,
                )


def main() -> int:
    errors: list[str] = []
    for p in (LOCK_PATH, JSON_PATH, INDEX_PATH, GOLDEN_HARNESS, NEG_HARNESS, HONESTY_LOCK, HONESTY_PHASE5):
        if not p.is_file():
            fail(f"missing {p.relative_to(REPO_ROOT)}", errors)
    if errors:
        print("FAIL: golden lock")
        for e in errors:
            print(f"  - {e}")
        return 1

    lock = json.loads(LOCK_PATH.read_text(encoding="utf-8"))
    data = json.loads(JSON_PATH.read_text(encoding="utf-8"))
    index = json.loads(INDEX_PATH.read_text(encoding="utf-8"))
    vectors = data["vectors"]

    check_honesty(errors)

    if data.get("tag") != lock["tag"]:
        fail(f"JSON tag {data.get('tag')!r} != lock tag {lock['tag']!r}", errors)
    if data.get("count") != lock["shared_vectors"]:
        fail(f"JSON count field {data.get('count')} != lock {lock['shared_vectors']}", errors)
    if len(vectors) != lock["shared_vectors"]:
        fail(f"JSON len(vectors)={len(vectors)} != lock {lock['shared_vectors']}", errors)
    if len(index) != lock["shared_vectors"]:
        fail(f"harness index length {len(index)} != lock {lock['shared_vectors']}", errors)

    json_ids = [v["id"] for v in vectors]
    index_ids = [row[1] for row in index]
    if json_ids != index_ids:
        fail("JSON vector ids and coq_harness_index.json ids drifted (order or membership)", errors)

    flag_true = sum(1 for v in vectors if v.get("coq_enforce") is True)
    if flag_true != lock["json_coq_enforce_true"]:
        fail(
            f"JSON coq_enforce:true = {flag_true} != lock {lock['json_coq_enforce_true']} "
            "(this is NOT the L1↔L2 lock; the lock is coq_enforced_lemmas)",
            errors,
        )

    golden_n, golden_names = count_lemmas(GOLDEN_HARNESS)
    neg_n, neg_names = count_lemmas(NEG_HARNESS)
    if golden_n != lock["golden_harness_lemmas"]:
        fail(f"GoldenHarness.v lemmas={golden_n} != lock {lock['golden_harness_lemmas']}", errors)
    if neg_n != lock["negative_core_harness_lemmas"]:
        fail(
            f"NegativeCoreHarness.v lemmas={neg_n} != lock {lock['negative_core_harness_lemmas']}",
            errors,
        )
    lemma_total = golden_n + neg_n
    if lemma_total != lock["coq_enforced_lemmas"]:
        fail(
            f"harness lemmas {lemma_total} (golden {golden_n} + neg {neg_n}) "
            f"!= lock {lock['coq_enforced_lemmas']}",
            errors,
        )

    index_coq = [row for row in index if row[0] is not None]
    if len(index_coq) != lock["coq_enforced_lemmas"]:
        fail(
            f"index Coq-enforced rows={len(index_coq)} != lock {lock['coq_enforced_lemmas']}",
            errors,
        )
    index_neg = [row for row in index_coq if row[3] == "neg_core"]
    index_kernel = [row for row in index_coq if row[3] != "neg_core"]
    if len(index_neg) != lock["negative_core_harness_lemmas"]:
        fail(
            f"index NegativeCore lemmas={len(index_neg)} != lock {lock['negative_core_harness_lemmas']}",
            errors,
        )
    if len(index_kernel) != lock["golden_harness_lemmas"]:
        fail(
            f"index GoldenHarness lemmas={len(index_kernel)} != lock {lock['golden_harness_lemmas']}",
            errors,
        )

    by_mod: dict[str, dict[str, int]] = defaultdict(lambda: {"vectors": 0, "coq_enforced": 0})
    for row in index:
        mod = row[2]
        by_mod[mod]["vectors"] += 1
        if row[0] is not None:
            by_mod[mod]["coq_enforced"] += 1

    expected_fams = lock["families"]
    if set(by_mod) != set(expected_fams):
        extra = sorted(set(by_mod) - set(expected_fams))
        missing = sorted(set(expected_fams) - set(by_mod))
        if extra:
            fail(f"unexpected JSON modules (bump golden_lock.json if intentional): {extra}", errors)
        if missing:
            fail(f"missing locked modules: {missing}", errors)
    for mod, exp in expected_fams.items():
        got = by_mod.get(mod, {"vectors": 0, "coq_enforced": 0})
        if got["vectors"] != exp["vectors"] or got["coq_enforced"] != exp["coq_enforced"]:
            fail(
                f"family {mod}: vectors={got['vectors']} coq={got['coq_enforced']} "
                f"!= lock vectors={exp['vectors']} coq={exp['coq_enforced']}",
                errors,
            )

    vec_by_id = {v["id"]: v for v in vectors}
    index_by_id = {row[1]: row for row in index}

    def assert_enforced_ids(fam: str, ids: list[str], label: str) -> None:
        for vid in ids:
            if vid not in vec_by_id:
                fail(f"{fam} {label} missing from JSON: {vid}", errors)
                continue
            if vec_by_id[vid]["module"] != fam:
                fail(f"{vid} module is {vec_by_id[vid]['module']!r}, expected {fam}", errors)
            row = index_by_id.get(vid)
            if row is None or row[0] is None:
                fail(f"{fam} {label} not Coq-enforced in harness index: {vid}", errors)
            if vec_by_id[vid].get("coq_enforce") is False:
                fail(f"{fam} {vid} has coq_enforce:false; this family must stay enforced", errors)

    for fam, spec in lock["must_enforce_families"].items():
        if "id_prefix" in spec:
            ids = expected_ids(spec["id_prefix"], spec["id_first"], spec["id_last"])
            if len(ids) != spec["vectors"]:
                fail(f"{fam}: id range length {len(ids)} != lock vectors {spec['vectors']}", errors)
            assert_enforced_ids(fam, ids, "row")
        if "negative_prefix" in spec:
            neg_ids = expected_ids(spec["negative_prefix"], spec["negative_first"], spec["negative_last"])
            assert_enforced_ids(fam, neg_ids, "negative")

    check_surface_splits(lock, vectors, index, errors)

    # Harness source must name every Coq-enforced index id (sanitized).
    def sanitize(vid: str) -> str:
        return "".join(c if c.isalnum() else "_" for c in vid)

    missing_lemmas = []
    for row in index_coq:
        name = sanitize(row[1])
        if row[3] == "neg_core":
            if name not in neg_names:
                missing_lemmas.append(f"NegativeCoreHarness missing Lemma golden_{name}")
        else:
            if name not in golden_names:
                missing_lemmas.append(f"GoldenHarness missing Lemma golden_{name}")
    for msg in missing_lemmas[:20]:
        fail(msg, errors)
    if len(missing_lemmas) > 20:
        fail(f"... and {len(missing_lemmas) - 20} more missing harness lemmas", errors)

    if errors:
        print("FAIL: golden lock")
        for e in errors:
            print(f"  - {e}")
        return 1

    print("golden lock: PASS")
    print(
        f"    vectors={lock['shared_vectors']}  Coq-enforced lemmas={lock['coq_enforced_lemmas']} "
        f"(GoldenHarness {lock['golden_harness_lemmas']} + NegativeCore {lock['negative_core_harness_lemmas']})"
    )
    must = "  ".join(
        f"{fam} {spec['vectors']}/{spec['coq_enforced']}"
        for fam, spec in lock["must_enforce_families"].items()
        if "negative_prefix" not in spec
    )
    print(f"    must-enforce: {must}")
    split_ids = [vid for spec in lock["surface_splits"]["rows"] for vid in spec["ids"]]
    print(
        f"    surface-splits ({len(split_ids)}): {', '.join(split_ids)}  "
        f"omit-coq_enforce-key (enforced)={len(lock['surface_splits']['omit_coq_enforce_key_enforced'])}  "
        f"python-only families={len(lock['surface_splits']['python_only_families'])}"
    )
    print("    honesty flags: all false")
    return 0


if __name__ == "__main__":
    sys.exit(main())
