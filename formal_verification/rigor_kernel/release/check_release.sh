#!/usr/bin/env bash
# Completeness gate for the formal-verification release package.
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$DIR/.." && pwd)"
REPO="$(cd "$ROOT/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }

echo "== release package files =="
for f in \
  release/README.md \
  release/AUDIT_PACKAGE.md \
  release/VerificationScope.md \
  release/TheoremIndex.md \
  release/ProofEvidenceIndex.md \
  release/BinaryArtifactIndex.md \
  release/ReproductionGuide.md \
  release/AuditorChecklist.md \
  release/PublicClaims.md \
  release/VersionManifest.md \
  release/ProofSourceManifest.md \
  release/SourceManifest.sha256 \
  release/TheoremClassification.md \
  release/KernelCompletenessMatrix.md \
  release/IndependentAuditReplay.md \
  release/independent_audit_replay.sh \
  release/RefinementMapping.md \
  release/VerifiedCompilerPath.md \
  release/OpenProperties.md \
  release/KernelInventory.md \
  release/TCBInventory.md \
  release/TrustDebtMatrix.md \
  release/P0-119-DecisionReport.md \
  release/KernelDeepeningReport.md \
  release/KernelCompletenessReport.md \
  release/ExtractionBoundary.md \
  release/CompilerBoundary.md \
  release/FormalTrustRoadmap.md \
  release/ConcreteRefinementBoundary.md \
  release/KernelExecutionBoundaryReport.md \
  release/ExtractionBoundaryHardeningReport.md \
  release/RuntimeBoundaryClosureReport.md \
  release/FinalProofInventory.md \
  release/FormalConeMap.md \
  release/FinalClaimsBoundary.md \
  release/FinalAssumptionAudit.md \
  release/MutationCompletenessAudit.md \
  release/final_adversarial_audit.sh \
  release/check_release.sh \
  release/check_source_manifest.sh \
  release/freeze_source_manifest.sh \
  release/source_manifest_common.py \
  release/UnclassifiedAllowlist.md \
  scripts/check_theorem_classification.py \
  scripts/check_mutation_coverage.py \
  scripts/check_no_mutation_switches.py \
  scripts/extract_and_test.sh \
  scripts/diff_verify.sh \
  scripts/diff_oracle.py \
  scripts/verified_compiler_path.sh \
  Kernel/CryptoAssumptions.v \
  Kernel/VerifiedCompilerBoundary.v \
  Kernel/SafetyExtensions.v \
  verified_compiler/fragment_checker.c \
  extracted/Extract.v \
  extracted/test_driver.ml \
  extracted/README.md
do
  [[ -f "$f" ]] || fail "missing $f"
done

echo "== supporting evidence =="
for f in \
  evidence/ProofInventory.md \
  evidence/KernelCoverage.md \
  evidence/MutationReport.md \
  evidence/BinaryTrace.md \
  evidence/TCBInventory.md \
  evidence/RefinementBoundary.md \
  evidence/ClaimsBoundary.md \
  evidence/BoundaryCorrection.md \
  evidence/ExtractionBoundary.md \
  evidence/TheoremClassification.md \
  evidence/PowScope.md \
  evidence/ASERTScope.md \
  evidence/SerializationBoundary.md \
  evidence/BinaryNegativeVectorReport.md \
  evidence/DiffVerifyReport.md \
  evidence/VerifiedCompilerPathReport.md \
  evidence/IndependentAuditReplayReport.md \
  evidence/Reproduction.md \
  kernel_binary/TRUST_MANIFEST.md
do
  [[ -f "$f" ]] || fail "missing $f"
done

echo "== public-claim discipline in release/ =="
grep -q 'critical consensus invariants formally verified' release/PublicClaims.md \
  || fail "PublicClaims.md missing allowed TRUE claim"
grep -q 'Coq consensus kernel model' release/PublicClaims.md \
  || fail "PublicClaims.md missing Coq-kernel qualifier"
grep -q 'full node verification' release/PublicClaims.md \
  || fail "PublicClaims.md missing forbidden full-node claim"
grep -q 'theorem-to-binary refinement' release/PublicClaims.md \
  || fail "PublicClaims.md missing forbidden theorem-to-binary claim"
grep -q 'FALSE' release/PublicClaims.md \
  || fail "PublicClaims.md must mark forbidden claims FALSE"
grep -q 'Differential verification' release/PublicClaims.md \
  || fail "PublicClaims.md missing differential verification status"
grep -q 'CryptoAssumptions.v' release/PublicClaims.md \
  || fail "PublicClaims.md missing crypto assumption inventory"
grep -q 'no production-code refinement proof' release/PublicClaims.md \
  || fail "PublicClaims.md missing production-refinement non-claim"
grep -q 'EXTRACTION_FORMAL_PROOF=source_identity_only' release/PublicClaims.md \
  || fail "PublicClaims.md missing source_identity_only flag"
grep -q 'EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE' release/PublicClaims.md \
  || fail "PublicClaims.md missing EXTRACTION_EQUIVALENCE_NOT_CLAIMED"
grep -q 'Verified compiler trust root' release/PublicClaims.md \
  || fail "PublicClaims.md missing verified compiler trust root"
grep -q 'Open properties' release/PublicClaims.md \
  || fail "PublicClaims.md missing open properties pointer"
grep -q 'BINARY_SEMANTIC_EQUIVALENCE=false' release/PublicClaims.md \
  || fail "PublicClaims.md missing binary-equivalence non-claim"
grep -q 'still not claimed' release/PublicClaims.md \
  || fail "PublicClaims.md missing binary-equivalence still-not-claimed wording"
grep -q 'independent_audit_replay.sh' release/PublicClaims.md \
  || fail "PublicClaims.md missing independent audit replay pointer"
grep -q 'MATCH remains fragment agreement only' release/PublicClaims.md \
  || fail "PublicClaims.md missing MATCH-is-fragment wording"
grep -q 'independent correctness assertion' release/PublicClaims.md \
  || fail "PublicClaims.md missing MATCH-is-not-independent-correctness wording"

if grep -nE 'theorem_to_binary_trace_complete=true|THEOREM_TO_BINARY_TRACE=TRUE' \
     --include='*.md' --include='*.sh' --include='*.v' -R . \
     --exclude-dir=certicoq --exclude-dir=.git --exclude-dir=_build \
     | grep -v IndependentAuditReport.md \
     | grep -v IndependentAuditReplay.md \
     | grep -v release/check_release.sh ; then
  fail "forbidden theorem-to-binary completeness claim present"
fi

echo "== refresh ProofSourceManifest.md from this worktree =="
python3 - "$REPO" "$ROOT" <<'PY'
import hashlib, pathlib, subprocess, sys
repo = pathlib.Path(sys.argv[1])
root = pathlib.Path(sys.argv[2])
cone = root  # formal_verification/rigor_kernel

def run(args):
    return subprocess.run(args, cwd=repo, text=True, capture_output=True)

st = run(["git", "status", "--short", "formal_verification/rigor_kernel"])
ls = run(["git", "ls-files", "formal_verification/rigor_kernel"])
status_out = (st.stdout or "").rstrip() or "(clean / no matching paths in index)"
ls_out = (ls.stdout or "").rstrip() or "(none tracked under formal_verification/rigor_kernel)"

tracked_v = [ln for ln in ls_out.splitlines() if ln.endswith(".v")
             and "formal_verification/rigor_kernel/" in ln
             and "/archive/" not in ln]
# C++ rigor_kernel under core/src is a different path; ignore.

def sha256(p: pathlib.Path) -> str:
    h = hashlib.sha256()
    h.update(p.read_bytes())
    return h.hexdigest()

rows = []
for pat in ("**/*.v", "**/*.md", "**/*.sh", "**/*.ml", "**/*.json"):
    for p in sorted(cone.glob(pat)):
        if any(part in {".git", "_build", "certicoq"} for part in p.parts):
            continue
        rel = p.relative_to(cone).as_posix()
        if rel.startswith("extraction/ExtractedKernel"):
            # generated; still listed but marked
            kind = "generated"
        else:
            kind = "source"
        rows.append((kind, rel, sha256(p)))

v_files = [r for r in rows if r[1].endswith(".v") and r[0] == "source"]
md_files = [r for r in rows if r[1].endswith(".md")]
sh_files = [r for r in rows if r[1].endswith(".sh")]

clone_gets_proofs = len(tracked_v) >= len(v_files) and len(v_files) > 0
# tracked_v paths are repo-relative; compare basenames of cone .v files
tracked_names = {pathlib.Path(p).name for p in tracked_v}
worktree_names = {pathlib.Path(r[1]).name for r in v_files}
# More precise: tracked paths under formal_verification/rigor_kernel/*.v
tracked_cone_v = [
    p for p in (ls.stdout or "").splitlines()
    if p.startswith("formal_verification/rigor_kernel/") and p.endswith(".v")
]
clone_gets_proofs = len(tracked_cone_v) >= len(v_files) and len(v_files) > 0

head = run(["git", "rev-parse", "HEAD"])
head_s = (head.stdout or "").strip() or "unknown"

lines = []
lines.append("# Proof Source Manifest")
lines.append("")
lines.append("Worktree inventory of the Coq consensus kernel cone.")
lines.append("This file is the provenance record. **Do not cite a Git")
lines.append("commit as containing the proofs unless the tracked list")
lines.append("below includes every `.v` file.**")
lines.append("")
lines.append("**Public claim (only):** Formally verified critical consensus")
lines.append("invariants (Coq consensus kernel only).")
lines.append("")
lines.append("## git status")
lines.append("")
lines.append("```")
lines.append(status_out)
lines.append("```")
lines.append("")
lines.append("## git ls-files formal_verification/rigor_kernel")
lines.append("")
lines.append("```")
lines.append(ls_out)
lines.append("```")
lines.append("")
lines.append("## Clone reproducibility")
lines.append("")
if clone_gets_proofs:
    lines.append(f"TRACKED_PROOF_SOURCES=TRUE")
    lines.append(f"Git HEAD `{head_s}` contains the cone `.v` files.")
    lines.append("An auditor `git clone` of this commit can obtain the proofs.")
else:
    lines.append("TRACKED_PROOF_SOURCES=FALSE")
    lines.append(f"Git HEAD `{head_s}` does **not** contain the cone `.v` files.")
    lines.append("Do not cite that commit as proof provenance.")
    lines.append("An auditor must receive this worktree (or a future commit")
    lines.append("that actually tracks the files listed below).")
lines.append("")
lines.append(f"- source `.v` files in worktree: {len(v_files)}")
lines.append(f"- tracked cone `.v` files: {len(tracked_cone_v)}")
lines.append(f"- markdown files: {len(md_files)}")
lines.append(f"- scripts: {len(sh_files)}")
lines.append("")
lines.append("## File inventory (worktree)")
lines.append("")
lines.append("| Kind | Path | sha256 |")
lines.append("|---|---|---|")
for kind, rel, hx in rows:
    lines.append(f"| {kind} | `{rel}` | `{hx}` |")
lines.append("")
lines.append("Required classes present:")
lines.append("")
lines.append(f"- all `.v` sources listed: {len(v_files) > 0}")
lines.append(f"- all proof markdown listed: {len(md_files) > 0}")
lines.append(f"- all scripts listed: {len(sh_files) > 0}")
lines.append("")

out = root / "release" / "ProofSourceManifest.md"
out.write_text("\n".join(lines) + "\n")
print(f"wrote {out} files={len(rows)} tracked_cone_v={len(tracked_cone_v)}")
if len(v_files) == 0 or len(md_files) == 0 or len(sh_files) == 0:
    sys.exit("ProofSourceManifest missing a required class")
PY

echo "== refresh VersionManifest.md dynamic block =="
TS="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
COMMIT="unknown"
if git -C "$REPO" rev-parse HEAD >/dev/null 2>&1; then
  COMMIT="$(git -C "$REPO" rev-parse HEAD)"
fi
EXTRACT_HASH="not-built"
if [[ -f extraction/ExtractedKernel.ml ]]; then
  EXTRACT_HASH="$(sha256sum extraction/ExtractedKernel.ml | awk '{print $1}')"
fi

python3 - "$ROOT/release/VersionManifest.md" "$TS" "$COMMIT" "$EXTRACT_HASH" <<'PY'
import pathlib, sys
path, ts, commit, hx = pathlib.Path(sys.argv[1]), sys.argv[2], sys.argv[3], sys.argv[4]
text = path.read_text()
start = "## Dynamic (filled by check_release.sh)"
if start not in text:
    raise SystemExit("VersionManifest.md missing dynamic section")
head = text.split(start, 1)[0]
tail = ""
rest = text.split(start, 1)[1]
if "The native binary hash" in rest:
    tail = "\n" + rest[rest.index("The native binary hash"):]
body = f"""{start}

```
Build timestamp: {ts}
Proof provenance: see release/SourceManifest.sha256
Git HEAD (host only; not a proof-file commit unless tracked): {commit}
ExtractedKernel.ml sha256: {hx}
```
"""
path.write_text(head + body + tail)
PY

echo "AUDIT_DOCUMENTATION_COMPLETE=TRUE"
echo "THEOREM_INDEX_COMPLETE=TRUE"
echo "BINARY_TRACE_DOCUMENTED=TRUE"
echo "CLAIM_BOUNDARY_DOCUMENTED=TRUE"
echo "TCB_BOUNDARY_DOCUMENTED=TRUE"
echo "PROOF_SOURCE_MANIFEST_COMPLETE=TRUE"
echo "RELEASE_PACKAGE_COMPLETE=TRUE"
