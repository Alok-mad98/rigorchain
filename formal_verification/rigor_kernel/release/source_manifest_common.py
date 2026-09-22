"""Shared source-lock rules for freeze / check.

Used by freeze_source_manifest.sh (write) and check_source_manifest.sh
(read-only). Keep these lists identical for both directions.
"""
from __future__ import annotations

import hashlib
import os
from pathlib import Path

# Walk skips the whole certicoq/ tree so .opam-root and generated
# Clight are never hashed. Phase 8 then adds a precise extra set
# (surface / proofs / driver / scripts / SHA pin) below.
SKIP_DIRS = {".git", "_build", "certicoq"}

# Sidecar sources + the Clight SHA pin. Not generated .c, not .vo,
# not Makefile*, not .opam-root.
CERTICOQ_LOCK_DIRS = (
    "certicoq/surface",
    "certicoq/proofs",
    "certicoq/driver",
    "certicoq/scripts",
)
CERTICOQ_LOCK_EXTS = {".v", ".md", ".sh", ".c"}
CERTICOQ_LOCK_FILES = (
    "certicoq/surface/_CoqProject",
    "certicoq/proofs/_CoqProject",
    "certicoq/generated/SHA256",
    "certicoq/.gitignore",
)

# Generated / self-rewriting files. Not part of the frozen lock.
EXCLUDE = {
    "release/SourceManifest.sha256",
    "release/VersionManifest.md",
    "release/ProofSourceManifest.md",
    "extraction/ExtractedKernel.ml",
    "extraction/ExtractedKernel.mli",
    "extracted/ExtractedKernel.ml",
    "extracted/ExtractedKernel.mli",
    "evidence/DiffVerifyReport.md",
    "evidence/VerifiedCompilerPathReport.md",
    "evidence/IndependentAuditReplayReport.md",
}

EXTS = {".v", ".ml", ".sh", ".md", ".c", ".py"}

REQUIRED_V = [
    "Core/Bytes.v",
    "Core/Amount.v",
    "Core/Height.v",
    "Script/ScriptTypes.v",
    "Script/ScriptClassifier.v",
    "Script/Semantics/Spend.v",
    "Script/Semantics/ScriptState.v",
    "Script/Semantics/ScriptStep.v",
    "Script/Semantics/ScriptTrace.v",
    "Script/Semantics/Execution.v",
    "Supply/ORUModel.v",
    "Supply/ORUConservation.v",
    "Supply/SupplySchedule.v",
    "Supply/FeeDomain.v",
    "Supply/SupplyTransition.v",
    "Pow/AbstractHash.v",
    "Pow/PowModel.v",
    "Pow/Uint256Model.v",
    "Pow/CompactBits.v",
    "Pow/BlockProof.v",
    "Pow/ASERTModel.v",
    "Pow/TargetRelation.v",
    "Pow/ASERTBoundary.v",
    "Pow/Acceptance.v",
    "Pow/ASERTMatrix.v",
    "proofs/UTXO_Soundness/UTXOSoundness.v",
    "proofs/UTXO_Soundness/UTXOTransitionSoundness.v",
    "proofs/ORU_Conservation/ORUConservation.v",
    "Kernel/Chain/ReorgReplay.v",
    "Kernel/Chain/ForkChoice.v",
    "proofs/RES_Issuance/RESIssuance.v",
    "proofs/Script_Correctness/ScriptCorrectness.v",
    "proofs/Pow_Correctness/PowCorrectness.v",
    "proofs/Pow_Correctness/ASERTCorrectness.v",
    "Kernel/UTXOValidator.v",
    "Kernel/ORUValidator.v",
    "Kernel/RESValidator.v",
    "Kernel/ScriptValidator.v",
    "Kernel/PowValidator.v",
    "Kernel/ASERTValidator.v",
    "Kernel/KernelInterface.v",
    "Kernel/KernelSoundness.v",
    "Kernel/SurfaceCorrespondence.v",
    "Kernel/RefinementBoundary.v",
    "Kernel/ExtractionBoundary.v",
    "Kernel/SerializationBoundary.v",
    "Audit/KernelNonClaims.v",
    "Audit/Task32AFlags.v",
    "Audit/Task32BFlags.v",
    "Audit/Task32CFlags.v",
    "Audit/Task32DFlags.v",
    "Audit/Task32EFlags.v",
    "Audit/Task32FFlags.v",
    "Audit/Task32GFlags.v",
    "Audit/Task32HFlags.v",
    "Audit/Task32IFlags.v",
    "Audit/Task32JFlags.v",
    "Audit/Task32KFlags.v",
    "Audit/Task33AFlags.v",
    "Audit/Task33A2Flags.v",
    "Audit/Task33A3Flags.v",
    "Audit/Task33A4Flags.v",
    "Audit/Task33A5Flags.v",
    "Audit/Task33A6Flags.v",
    "Audit/Task33A7Flags.v",
    "Audit/Task33A8Flags.v",
    "Audit/Task33A9Flags.v",
    "Audit/Task33A10Flags.v",
    "Audit/Task33BFlags.v",
    "Audit/TaskBoundaryMapFlags.v",
    "Kernel/CryptoAssumptions.v",
    "Kernel/VerifiedCompilerBoundary.v",
    "Kernel/SafetyExtensions.v",
    "Kernel/Transaction/Types.v",
    "Kernel/Transaction/Validity.v",
    "Kernel/Tx/Maturity.v",
    "Kernel/EconomicInvariant/Economic.v",
    "Kernel/Block/Types.v",
    "Kernel/Block/Acceptance.v",
    "Audit/KernelAssumptions.v",
    "Refinement/AbstractKernel.v",
    "Refinement/ConcreteKernel.v",
    "Refinement/RefinementRelation.v",
    "Refinement/TransitionRefinement.v",
    "Refinement/EconomicState.v",
    "Refinement/EconomicRefinement.v",
    "Refinement/KernelComposition.v",
    "Execution/KernelMachine.v",
    "Economic/ORU/ORUDomain.v",
    "Economic/ORU/ORUTransition.v",
    "Economic/ORU/ORUSoundness.v",
    "Economic/DomainSeparation.v",
    "Kernel/UnifiedState.v",
    "Kernel/UnifiedTransition.v",
    "Kernel/KernelInputBridge.v",
    "Kernel/InvariantBundle.v",
    "proofs/KernelTransitionSoundness.v",
    "Economic/UTXOEconomicBridge.v",
    "Refinement/ConcreteKernel/ConcreteKernelState.v",
    "Refinement/ConcreteKernel/ConcreteKernelMapping.v",
    "Refinement/ConcreteKernel/ConcreteKernelTransition.v",
    "ExecutionBoundary/ConcreteExecutionState.v",
    "ExecutionBoundary/ExecutableTransition.v",
    "SerializationBoundary/KernelSerialization.v",
    "ExtractionBoundary/ExtractionInterface.v",
    "ExtractionBoundary/ExtractionModel.v",
    "ExtractionBoundary/ExtractionTrace.v",
    "ExtractionBoundary/GoldenNegatives.v",
    "RuntimeModel/IntegerBoundary.v",
    "Runtime/RuntimeState.v",
    "Runtime/RuntimeTransition.v",
    "Runtime/RuntimeSemantics.v",
    "Runtime/RuntimeFailure.v",
    "CheckerContract/CheckerSpecification.v",
]


def sha256_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def iter_lock_files(root: Path) -> list[tuple[str, str]]:
    rows: list[tuple[str, str]] = []
    seen: set[str] = set()

    def add(rel: str, path: Path) -> None:
        if rel in EXCLUDE or rel in seen:
            return
        if not path.is_file():
            return
        seen.add(rel)
        rows.append((rel, sha256_file(path)))

    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for name in filenames:
            p = Path(dirpath) / name
            if p.suffix not in EXTS:
                continue
            add(p.relative_to(root).as_posix(), p)

    for sub in CERTICOQ_LOCK_DIRS:
        base = root / sub
        if not base.is_dir():
            continue
        for f in base.rglob("*"):
            if not f.is_file() or f.suffix not in CERTICOQ_LOCK_EXTS:
                continue
            add(f.relative_to(root).as_posix(), f)

    for rel in CERTICOQ_LOCK_FILES:
        add(rel, root / rel)

    rows.sort()
    return rows


def parse_manifest(text: str) -> list[tuple[str, str]]:
    rows = []
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        hx, rel = line.split(None, 1)
        rows.append((rel, hx))
    return rows
