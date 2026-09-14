#!/usr/bin/env bash
# READ-ONLY source lock. Never writes SourceManifest.sha256 or any
# other file. Bidirectional:
#   1. every listed path exists and matches its sha256
#   2. every lock-eligible worktree file appears in the list
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$DIR/.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }

MANIFEST="$DIR/SourceManifest.sha256"
COMMON="$DIR/source_manifest_common.py"
[[ -f "$MANIFEST" ]] || fail "missing $MANIFEST (publisher must run freeze_source_manifest.sh)"
[[ -f "$COMMON" ]] || fail "missing $COMMON"

echo "== required Coq proof sources =="
python3 - "$ROOT" "$COMMON" <<'PY'
import importlib.util, pathlib, sys
root = pathlib.Path(sys.argv[1])
spec = importlib.util.spec_from_file_location("sm", sys.argv[2])
sm = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sm)
missing = [f for f in sm.REQUIRED_V if not (root / f).is_file()]
if missing:
    for f in missing:
        print("MISSING proof source:", f, file=sys.stderr)
    sys.exit(f"{len(missing)} required .v file(s) missing")
print(f"required_v={len(sm.REQUIRED_V)} present")
PY

echo "== verify SourceManifest.sha256 (read-only, bidirectional) =="
python3 - "$ROOT" "$MANIFEST" "$COMMON" <<'PY'
import importlib.util, pathlib, sys
root = pathlib.Path(sys.argv[1])
manifest = pathlib.Path(sys.argv[2])
spec = importlib.util.spec_from_file_location("sm", sys.argv[3])
sm = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sm)

listed = sm.parse_manifest(manifest.read_text())
if len(listed) < 40:
    sys.exit("too few hashed sources in manifest")

errors = []
listed_rels = {}
for rel, hx in listed:
    if rel in listed_rels:
        errors.append("duplicate manifest entry: %s" % rel)
    listed_rels[rel] = hx
    p = root / rel
    if not p.is_file():
        errors.append("missing listed source: %s" % rel)
        continue
    got = sm.sha256_file(p)
    if got != hx:
        errors.append("sha256 mismatch: %s" % rel)

on_disk = sm.iter_lock_files(root)
on_disk_rels = {rel for rel, _ in on_disk}
listed_set = set(listed_rels)
extra = sorted(on_disk_rels - listed_set)
missing_from_disk = sorted(listed_set - on_disk_rels)
if extra:
    for rel in extra:
        errors.append("unlisted lock-eligible file: %s" % rel)
if missing_from_disk:
    for rel in missing_from_disk:
        errors.append("listed but not lock-eligible or missing: %s" % rel)

if errors:
    for e in errors:
        print("FAIL:", e, file=sys.stderr)
    sys.exit("%d source-lock error(s)" % len(errors))

print(f"verified={len(listed)} bidirectional=ok")
PY

echo "SOURCE_MANIFEST_COMPLETE=TRUE"
echo "REPRODUCIBLE_SOURCE_TREE=TRUE"
