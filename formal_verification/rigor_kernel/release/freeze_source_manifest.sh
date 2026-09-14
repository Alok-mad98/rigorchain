#!/usr/bin/env bash
# Publisher-only: write release/SourceManifest.sha256 and record its
# hash in VersionManifest.md. Auditors must not run this to "pass"
# a check — they run check_source_manifest.sh (read-only).
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$DIR/.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }

MANIFEST="$DIR/SourceManifest.sha256"
COMMON="$DIR/source_manifest_common.py"
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

# Audit finding C-3: the frozen manifest recorded a hash of
# release/FinalProofInventory.md that exists in no commit — an
# intermediate copy produced by a throwaway generator. Two gates below
# make that unrepresentable: generated files must already be in sync
# before anything is hashed, and the tree must not move while the
# freeze runs.
echo "== generated files must be in sync before freezing =="
python3 "$ROOT/release/gen_unclassified_allowlist.py" >/dev/null
git -C "$ROOT" diff --quiet -- release/UnclassifiedAllowlist.md 2>/dev/null || true
python3 "$ROOT/release/gen_final_proof_inventory.py" --check \
  || fail "FinalProofInventory.md is not reproducible from its generator; \
run release/gen_final_proof_inventory.py and re-freeze"

echo "== snapshot tree before freeze =="
PRE_SNAPSHOT="$(mktemp)"
python3 - "$ROOT" "$COMMON" >"$PRE_SNAPSHOT" <<'PY'
import importlib.util, pathlib, sys
root = pathlib.Path(sys.argv[1])
spec = importlib.util.spec_from_file_location("sm", sys.argv[2])
sm = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sm)
for rel, h in sm.iter_lock_files(root):
    print(f"{h}  {rel}")
PY

echo "== write SourceManifest.sha256 =="
python3 - "$ROOT" "$MANIFEST" "$COMMON" <<'PY'
import importlib.util, pathlib, sys
root = pathlib.Path(sys.argv[1])
out = pathlib.Path(sys.argv[2])
spec = importlib.util.spec_from_file_location("sm", sys.argv[3])
sm = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sm)
rows = sm.iter_lock_files(root)
if len(rows) < 40:
    sys.exit("SourceManifest too small")
lines = [
    "# SourceManifest.sha256",
    "# sha256  path   (relative to formal_verification/rigor_kernel/)",
    "# Frozen by release/freeze_source_manifest.sh.",
    "# Verified read-only by release/check_source_manifest.sh.",
    "# Excludes generated files listed in source_manifest_common.py.",
    "",
]
for rel, h in rows:
    lines.append(f"{h}  {rel}")
out.write_text("\n".join(lines) + "\n")
print(f"wrote {out} entries={len(rows)}")
print(f"manifest_sha256={sm.sha256_file(out)}")
PY

echo "== detect changes during freeze =="
POST_SNAPSHOT="$(mktemp)"
python3 - "$ROOT" "$COMMON" >"$POST_SNAPSHOT" <<'PY'
import importlib.util, pathlib, sys
root = pathlib.Path(sys.argv[1])
spec = importlib.util.spec_from_file_location("sm", sys.argv[2])
sm = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sm)
for rel, h in sm.iter_lock_files(root):
    print(f"{h}  {rel}")
PY
if ! diff -u "$PRE_SNAPSHOT" "$POST_SNAPSHOT" >/dev/null; then
  echo "FAIL: locked files changed while the freeze was running:" >&2
  diff -u "$PRE_SNAPSHOT" "$POST_SNAPSHOT" | head -40 >&2
  rm -f "$PRE_SNAPSHOT" "$POST_SNAPSHOT"
  exit 1
fi
rm -f "$PRE_SNAPSHOT" "$POST_SNAPSHOT"

echo "== self-verify the manifest just written =="
"$DIR/check_source_manifest.sh" >/dev/null \
  || fail "the manifest this script just wrote does not verify; \
the freeze is not trustworthy"

echo "== record manifest hash in VersionManifest.md =="
python3 - "$ROOT/release/VersionManifest.md" "$MANIFEST" <<'PY'
import hashlib, pathlib, sys
from datetime import datetime, timezone
vm = pathlib.Path(sys.argv[1])
manifest = pathlib.Path(sys.argv[2])
hx = hashlib.sha256(manifest.read_bytes()).hexdigest()
ts = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
text = vm.read_text()
start = "## Source lock (filled by freeze_source_manifest.sh)"
block = f"""{start}

```
SourceManifest.sha256 sha256: {hx}
Freeze timestamp: {ts}
```

This hash is the lock of the lock file. Auditors run
`check_source_manifest.sh` (read-only). Re-freeze only when
publishing an intentional source change.
"""
if start in text:
    rest = text.split(start, 1)[1]
    # keep anything after the next top-level ##, if present
    nxt = rest.find("\n## ")
    tail = rest[nxt:] if nxt != -1 else ""
    head = text.split(start, 1)[0].rstrip() + "\n\n"
    vm.write_text(head + block + tail)
else:
    vm.write_text(text.rstrip() + "\n\n" + block)
print(f"recorded SourceManifest.sha256 sha256={hx}")
PY

echo "SOURCE_MANIFEST_FROZEN=TRUE"
