# Proof Source Manifest

Worktree inventory of the Coq consensus kernel cone.
This file is the provenance record. **Do not cite a Git
commit as containing the proofs unless the tracked list
below includes every `.v` file.**

**Public claim (only):** Formally verified critical consensus
invariants (Coq consensus kernel only).

## git status

```
 M formal_verification/rigor_kernel/release/FinalProofInventory.md
 M formal_verification/rigor_kernel/release/SourceManifest.sha256
 M formal_verification/rigor_kernel/release/UnclassifiedAllowlist.md
 M formal_verification/rigor_kernel/release/VersionManifest.md
```

## git ls-files formal_verification/rigor_kernel

```
formal_verification/rigor_kernel/ASERT_Model/.gitkeep
formal_verification/rigor_kernel/Asset_Model/.gitkeep
formal_verification/rigor_kernel/Audit/.gitkeep
formal_verification/rigor_kernel/Audit/KernelAssumptions.v
formal_verification/rigor_kernel/Audit/KernelNonClaims.v
formal_verification/rigor_kernel/Audit/Task32AFlags.v
formal_verification/rigor_kernel/Audit/Task32BFlags.v
formal_verification/rigor_kernel/Audit/Task32CFlags.v
formal_verification/rigor_kernel/Audit/Task32DFlags.v
formal_verification/rigor_kernel/Audit/Task32EFlags.v
formal_verification/rigor_kernel/Audit/Task32FFlags.v
formal_verification/rigor_kernel/Audit/Task32GFlags.v
formal_verification/rigor_kernel/Audit/Task32HFlags.v
formal_verification/rigor_kernel/Audit/Task32IFlags.v
formal_verification/rigor_kernel/Audit/Task32JFlags.v
formal_verification/rigor_kernel/Audit/Task32KFlags.v
formal_verification/rigor_kernel/Audit/Task33A10Flags.v
formal_verification/rigor_kernel/Audit/Task33A2Flags.v
formal_verification/rigor_kernel/Audit/Task33A3Flags.v
formal_verification/rigor_kernel/Audit/Task33A4Flags.v
formal_verification/rigor_kernel/Audit/Task33A5Flags.v
formal_verification/rigor_kernel/Audit/Task33A6Flags.v
formal_verification/rigor_kernel/Audit/Task33A7Flags.v
formal_verification/rigor_kernel/Audit/Task33A8Flags.v
formal_verification/rigor_kernel/Audit/Task33A9Flags.v
formal_verification/rigor_kernel/Audit/Task33AFlags.v
formal_verification/rigor_kernel/Audit/Task33BFlags.v
formal_verification/rigor_kernel/Audit/TaskBoundaryMapFlags.v
formal_verification/rigor_kernel/CheckerContract/CheckerSpecification.v
formal_verification/rigor_kernel/Core/Amount.v
formal_verification/rigor_kernel/Core/Bytes.v
formal_verification/rigor_kernel/Core/Height.v
formal_verification/rigor_kernel/Economic/DomainSeparation.v
formal_verification/rigor_kernel/Economic/ORU/ORUDomain.v
formal_verification/rigor_kernel/Economic/ORU/ORUSoundness.v
formal_verification/rigor_kernel/Economic/ORU/ORUTransition.v
formal_verification/rigor_kernel/Economic/UTXOEconomicBridge.v
formal_verification/rigor_kernel/Execution/KernelMachine.v
formal_verification/rigor_kernel/ExecutionBoundary/ConcreteExecutionState.v
formal_verification/rigor_kernel/ExecutionBoundary/ExecutableInvariance.v
formal_verification/rigor_kernel/ExecutionBoundary/ExecutableTransition.v
formal_verification/rigor_kernel/ExtractionBoundary/ExtractionInterface.v
formal_verification/rigor_kernel/ExtractionBoundary/ExtractionModel.v
formal_verification/rigor_kernel/ExtractionBoundary/ExtractionTrace.v
formal_verification/rigor_kernel/ExtractionBoundary/GoldenNegatives.v
formal_verification/rigor_kernel/Kernel/ASERTValidator.v
formal_verification/rigor_kernel/Kernel/Block/Acceptance.v
formal_verification/rigor_kernel/Kernel/Block/Types.v
formal_verification/rigor_kernel/Kernel/Chain/ChainRefinement.v
formal_verification/rigor_kernel/Kernel/Chain/ChainTransition.v
formal_verification/rigor_kernel/Kernel/Chain/ForkChoice.v
formal_verification/rigor_kernel/Kernel/Chain/ReorgReplay.v
formal_verification/rigor_kernel/Kernel/CryptoAssumptions.v
formal_verification/rigor_kernel/Kernel/EconomicInvariant/Economic.v
formal_verification/rigor_kernel/Kernel/ExtractionBoundary.v
formal_verification/rigor_kernel/Kernel/InvariantBundle.v
formal_verification/rigor_kernel/Kernel/KernelInputBridge.v
formal_verification/rigor_kernel/Kernel/KernelInterface.v
formal_verification/rigor_kernel/Kernel/KernelSoundness.v
formal_verification/rigor_kernel/Kernel/ORUValidator.v
formal_verification/rigor_kernel/Kernel/PowValidator.v
formal_verification/rigor_kernel/Kernel/RESValidator.v
formal_verification/rigor_kernel/Kernel/RefinementBoundary.v
formal_verification/rigor_kernel/Kernel/SafetyExtensions.v
formal_verification/rigor_kernel/Kernel/ScriptValidator.v
formal_verification/rigor_kernel/Kernel/SerializationBoundary.v
formal_verification/rigor_kernel/Kernel/SurfaceCorrespondence.v
formal_verification/rigor_kernel/Kernel/Transaction/Types.v
formal_verification/rigor_kernel/Kernel/Transaction/Validity.v
formal_verification/rigor_kernel/Kernel/Tx/Maturity.v
formal_verification/rigor_kernel/Kernel/UTXOValidator.v
formal_verification/rigor_kernel/Kernel/UnifiedState.v
formal_verification/rigor_kernel/Kernel/UnifiedTransition.v
formal_verification/rigor_kernel/Kernel/VerifiedCompilerBoundary.v
formal_verification/rigor_kernel/Pow/ASERTBoundary.v
formal_verification/rigor_kernel/Pow/ASERTMatrix.v
formal_verification/rigor_kernel/Pow/ASERTModel.v
formal_verification/rigor_kernel/Pow/AbstractHash.v
formal_verification/rigor_kernel/Pow/Acceptance.v
formal_verification/rigor_kernel/Pow/BlockProof.v
formal_verification/rigor_kernel/Pow/CompactBits.v
formal_verification/rigor_kernel/Pow/PowModel.v
formal_verification/rigor_kernel/Pow/TargetRelation.v
formal_verification/rigor_kernel/Pow/Uint256Model.v
formal_verification/rigor_kernel/Pow_Model/.gitkeep
formal_verification/rigor_kernel/README.md
formal_verification/rigor_kernel/Refinement/AbstractKernel.v
formal_verification/rigor_kernel/Refinement/ConcreteKernel.v
formal_verification/rigor_kernel/Refinement/ConcreteKernel/ConcreteKernelMapping.v
formal_verification/rigor_kernel/Refinement/ConcreteKernel/ConcreteKernelState.v
formal_verification/rigor_kernel/Refinement/ConcreteKernel/ConcreteKernelTransition.v
formal_verification/rigor_kernel/Refinement/EconomicRefinement.v
formal_verification/rigor_kernel/Refinement/EconomicState.v
formal_verification/rigor_kernel/Refinement/KernelComposition.v
formal_verification/rigor_kernel/Refinement/RefinementRelation.v
formal_verification/rigor_kernel/Refinement/TransitionRefinement.v
formal_verification/rigor_kernel/Runtime/MachineState.v
formal_verification/rigor_kernel/Runtime/MachineWord.v
formal_verification/rigor_kernel/Runtime/RuntimeFailure.v
formal_verification/rigor_kernel/Runtime/RuntimeSemantics.v
formal_verification/rigor_kernel/Runtime/RuntimeState.v
formal_verification/rigor_kernel/Runtime/RuntimeTransition.v
formal_verification/rigor_kernel/RuntimeModel/IntegerBoundary.v
formal_verification/rigor_kernel/Script/ScriptClassifier.v
formal_verification/rigor_kernel/Script/ScriptTypes.v
formal_verification/rigor_kernel/Script/Semantics/Execution.v
formal_verification/rigor_kernel/Script/Semantics/ScriptState.v
formal_verification/rigor_kernel/Script/Semantics/ScriptStep.v
formal_verification/rigor_kernel/Script/Semantics/ScriptTrace.v
formal_verification/rigor_kernel/Script/Semantics/Spend.v
formal_verification/rigor_kernel/Script_Model/.gitkeep
formal_verification/rigor_kernel/SerializationBoundary/KernelSerialization.v
formal_verification/rigor_kernel/Supply/FeeDomain.v
formal_verification/rigor_kernel/Supply/ORUConservation.v
formal_verification/rigor_kernel/Supply/ORUModel.v
formal_verification/rigor_kernel/Supply/SupplySchedule.v
formal_verification/rigor_kernel/Supply/SupplyTransition.v
formal_verification/rigor_kernel/UTXO_Model/.gitkeep
formal_verification/rigor_kernel/_CoqProject.kernel-critical
formal_verification/rigor_kernel/certicoq/.gitignore
formal_verification/rigor_kernel/certicoq/ci/Dockerfile
formal_verification/rigor_kernel/certicoq/ci/NOTICE.compcert
formal_verification/rigor_kernel/certicoq/driver/vector_driver.c
formal_verification/rigor_kernel/certicoq/generated/SHA256
formal_verification/rigor_kernel/certicoq/hello/Hello.v
formal_verification/rigor_kernel/certicoq/opam.pinned
formal_verification/rigor_kernel/certicoq/pins.sha512
formal_verification/rigor_kernel/certicoq/proofs/CertiCoqCorrespondence.v
formal_verification/rigor_kernel/certicoq/proofs/CodegenUpstream.v
formal_verification/rigor_kernel/certicoq/proofs/CompCertAST.v
formal_verification/rigor_kernel/certicoq/proofs/CompiledCheckerSoundness.v
formal_verification/rigor_kernel/certicoq/proofs/LambdaANFInstance.v
formal_verification/rigor_kernel/certicoq/proofs/PHASE10_RESULT.md
formal_verification/rigor_kernel/certicoq/proofs/PHASE9_RESULT.md
formal_verification/rigor_kernel/certicoq/proofs/Phase10Flags.v
formal_verification/rigor_kernel/certicoq/proofs/TASK018_RESULT.md
formal_verification/rigor_kernel/certicoq/proofs/TASK31A_RESULT.md
formal_verification/rigor_kernel/certicoq/proofs/TCB_PASSES.md
formal_verification/rigor_kernel/certicoq/proofs/Task018Flags.v
formal_verification/rigor_kernel/certicoq/proofs/Task31AFlags.v
formal_verification/rigor_kernel/certicoq/proofs/Task31BFlags.v
formal_verification/rigor_kernel/certicoq/proofs/Task31CFlags.v
formal_verification/rigor_kernel/certicoq/proofs/_CoqProject
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/CheckOruClightErr.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/CheckOruErase.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/CheckOruExpLemmas.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/CheckOruMemClight.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/CheckOruMemEval.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/CheckOruNoPipeline.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/CheckOruQuote.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/CheckOruStageAnf.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/CheckOruStageAnfCps.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/CheckOruStageClight.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/CheckOruStageClightCps.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/CheckOruStageEraseNP.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/CheckOruStageLocal.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/CheckOruTransfClight.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/CheckOruWellScoped.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/ClosedCminorOru.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/ClosedCompCertOru.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/ClosedCompCertOruBackend.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/ClosedCshmOru.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/ClosedHelloProbe.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/ClosedPhase31A.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/ClosedSelectionHelpersOru.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/EvalCase118Scall.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/EvalCminor.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/EvalCshm.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/EvalCshmDetail.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/EvalGetHelpers.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/EvalHelloAll.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/EvalHelloSel.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/EvalNGlobals.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/EvalPhase31B.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/EvalRTLgen.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/EvalSelection.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/EvalSimplLocals.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/EvalSimplLocalsErr.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/EvalSimplLocalsErr2.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/EvalSimplLocalsLazy.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/EvalSimplLocalsMsg.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/EvalUnliftSL.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/LambdaANFLemmasLoad.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/LambdaANFTry.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/Phase31A_SimplLocals.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/Phase31B_Assumptions.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/Phase31B_Unlift.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/Phase31C_Oracles.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/Phase31C_Selection.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/Phase31D_Native.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/PrintAssumptions31A.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/PrintAssumptions31B.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/PrintAssumptions31BOru.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/PrintAssumptions31C.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/PrintAssumptionsHelpers.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/PrintAssumptionsOru.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/UnliftDef.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/UnliftFeasibility.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/UnliftFeasibilityOru.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/UnliftOruFacts.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/UnliftRelation.v
formal_verification/rigor_kernel/certicoq/proofs/sidecar819/UnliftSimplLocals.v
formal_verification/rigor_kernel/certicoq/scripts/build_sidecar.sh
formal_verification/rigor_kernel/certicoq/scripts/build_sidecar_local.sh
formal_verification/rigor_kernel/certicoq/scripts/build_surface.sh
formal_verification/rigor_kernel/certicoq/scripts/build_surface_819.sh
formal_verification/rigor_kernel/certicoq/scripts/ccomp_link.sh
formal_verification/rigor_kernel/certicoq/scripts/compile_hello.sh
formal_verification/rigor_kernel/certicoq/scripts/compile_oru.sh
formal_verification/rigor_kernel/certicoq/scripts/compile_phase10.sh
formal_verification/rigor_kernel/certicoq/scripts/compile_phase10_doc.sh
formal_verification/rigor_kernel/certicoq/scripts/compile_phase10_eval.sh
formal_verification/rigor_kernel/certicoq/scripts/compile_phase10_mem.sh
formal_verification/rigor_kernel/certicoq/scripts/compile_phase10_nopipeline.sh
formal_verification/rigor_kernel/certicoq/scripts/compile_phase10_stages.sh
formal_verification/rigor_kernel/certicoq/scripts/compile_phase5.sh
formal_verification/rigor_kernel/certicoq/scripts/compile_phase7.sh
formal_verification/rigor_kernel/certicoq/scripts/compile_phase9.sh
formal_verification/rigor_kernel/certicoq/scripts/compile_phase9_ast.sh
formal_verification/rigor_kernel/certicoq/scripts/compile_phase9_lambdaanf.sh
formal_verification/rigor_kernel/certicoq/scripts/compile_task018_doc.sh
formal_verification/rigor_kernel/certicoq/scripts/compile_task018_probe.sh
formal_verification/rigor_kernel/certicoq/scripts/compile_task31a_doc.sh
formal_verification/rigor_kernel/certicoq/scripts/enter_sidecar.sh
formal_verification/rigor_kernel/certicoq/scripts/observe_surface.sh
formal_verification/rigor_kernel/certicoq/scripts/sidecar_lib.sh
formal_verification/rigor_kernel/certicoq/scripts/toolchain_probe.sh
formal_verification/rigor_kernel/certicoq/scripts/verify_pins.sh
formal_verification/rigor_kernel/certicoq/surface/Core/Amount.v
formal_verification/rigor_kernel/certicoq/surface/Core/Bytes.v
formal_verification/rigor_kernel/certicoq/surface/ExtractAsert.v
formal_verification/rigor_kernel/certicoq/surface/ExtractCertiCoq.v
formal_verification/rigor_kernel/certicoq/surface/ExtractPhase6.v
formal_verification/rigor_kernel/certicoq/surface/ExtractPow.v
formal_verification/rigor_kernel/certicoq/surface/ExtractRes.v
formal_verification/rigor_kernel/certicoq/surface/ExtractScript.v
formal_verification/rigor_kernel/certicoq/surface/ExtractUtxo.v
formal_verification/rigor_kernel/certicoq/surface/ExtractValidate.v
formal_verification/rigor_kernel/certicoq/surface/FRAGMENT_AUDIT.md
formal_verification/rigor_kernel/certicoq/surface/IMPORTS.md
formal_verification/rigor_kernel/certicoq/surface/Kernel/ASERT.v
formal_verification/rigor_kernel/certicoq/surface/Kernel/ORU.v
formal_verification/rigor_kernel/certicoq/surface/Kernel/Pow.v
formal_verification/rigor_kernel/certicoq/surface/Kernel/RES.v
formal_verification/rigor_kernel/certicoq/surface/Kernel/Script.v
formal_verification/rigor_kernel/certicoq/surface/Kernel/TxBool.v
formal_verification/rigor_kernel/certicoq/surface/Kernel/UTXO.v
formal_verification/rigor_kernel/certicoq/surface/Kernel/Validate.v
formal_verification/rigor_kernel/certicoq/surface/Observe.v
formal_verification/rigor_kernel/certicoq/surface/PORT_8_19.md
formal_verification/rigor_kernel/certicoq/surface/Pow/ASERTQ.v
formal_verification/rigor_kernel/certicoq/surface/Pow/Compact.v
formal_verification/rigor_kernel/certicoq/surface/Script/Classify.v
formal_verification/rigor_kernel/certicoq/surface/Script/ScriptTypes.v
formal_verification/rigor_kernel/certicoq/surface/Supply/KernelTx.v
formal_verification/rigor_kernel/certicoq/surface/Supply/ORUModel.v
formal_verification/rigor_kernel/certicoq/surface/Supply/Poles.v
formal_verification/rigor_kernel/certicoq/surface/Supply/RESTx.v
formal_verification/rigor_kernel/certicoq/surface/_CoqProject
formal_verification/rigor_kernel/evidence/ASERTScope.md
formal_verification/rigor_kernel/evidence/BinaryMutationReport.md
formal_verification/rigor_kernel/evidence/BinaryNegativeVectorReport.md
formal_verification/rigor_kernel/evidence/BinaryTrace.md
formal_verification/rigor_kernel/evidence/BoundaryCorrection.md
formal_verification/rigor_kernel/evidence/BoundaryMutationReport.md
formal_verification/rigor_kernel/evidence/CertiCoqCompCertReport.md
formal_verification/rigor_kernel/evidence/ClaimsBoundary.md
formal_verification/rigor_kernel/evidence/DiffVerifyReport.md
formal_verification/rigor_kernel/evidence/ExtractionBoundary.md
formal_verification/rigor_kernel/evidence/IndependentAuditReplayReport.md
formal_verification/rigor_kernel/evidence/KernelCone.md
formal_verification/rigor_kernel/evidence/KernelCoverage.md
formal_verification/rigor_kernel/evidence/MutationReport.md
formal_verification/rigor_kernel/evidence/PowScope.md
formal_verification/rigor_kernel/evidence/ProofInventory.md
formal_verification/rigor_kernel/evidence/README.md
formal_verification/rigor_kernel/evidence/RefinementBoundary.md
formal_verification/rigor_kernel/evidence/ReproducibleBuild.md
formal_verification/rigor_kernel/evidence/Reproduction.md
formal_verification/rigor_kernel/evidence/SerializationBoundary.md
formal_verification/rigor_kernel/evidence/TCBInventory.md
formal_verification/rigor_kernel/evidence/TheoremClassification.md
formal_verification/rigor_kernel/evidence/VerifiedCompilerPathReport.md
formal_verification/rigor_kernel/extracted/Extract.v
formal_verification/rigor_kernel/extracted/ExtractedKernel.ml
formal_verification/rigor_kernel/extracted/ExtractedKernel.mli
formal_verification/rigor_kernel/extracted/README.md
formal_verification/rigor_kernel/extracted/test_driver.ml
formal_verification/rigor_kernel/extraction/ExtractKernel.v
formal_verification/rigor_kernel/extraction/ExtractedKernel.ml
formal_verification/rigor_kernel/extraction/ExtractedKernel.mli
formal_verification/rigor_kernel/extraction/README.md
formal_verification/rigor_kernel/extraction/build.sh
formal_verification/rigor_kernel/kernel_binary/BinaryBoundaryTrace.md
formal_verification/rigor_kernel/kernel_binary/ExecutableKernelChecker.ml
formal_verification/rigor_kernel/kernel_binary/README.md
formal_verification/rigor_kernel/kernel_binary/TRUST_MANIFEST.md
formal_verification/rigor_kernel/kernel_binary/build.sh
formal_verification/rigor_kernel/kernel_binary/main.ml
formal_verification/rigor_kernel/kernel_binary/run_binary_mutations.sh
formal_verification/rigor_kernel/kernel_binary/run_invalid_vectors.sh
formal_verification/rigor_kernel/kernel_binary/run_vectors.sh
formal_verification/rigor_kernel/kernel_binary/test_vectors/asert_invalid.json
formal_verification/rigor_kernel/kernel_binary/test_vectors/asert_negative_time.json
formal_verification/rigor_kernel/kernel_binary/test_vectors/oru_conservation.json
formal_verification/rigor_kernel/kernel_binary/test_vectors/oru_invalid.json
formal_verification/rigor_kernel/kernel_binary/test_vectors/pow_invalid.json
formal_verification/rigor_kernel/kernel_binary/test_vectors/pow_target.json
formal_verification/rigor_kernel/kernel_binary/test_vectors/res_invalid.json
formal_verification/rigor_kernel/kernel_binary/test_vectors/res_schedule.json
formal_verification/rigor_kernel/kernel_binary/test_vectors/script_classification.json
formal_verification/rigor_kernel/kernel_binary/test_vectors/utxo_invalid.json
formal_verification/rigor_kernel/kernel_binary/test_vectors/utxo_over_max.json
formal_verification/rigor_kernel/kernel_binary/test_vectors/utxo_valid.json
formal_verification/rigor_kernel/kernel_binary/vectors/invalid/INVALID-001.json
formal_verification/rigor_kernel/kernel_binary/vectors/invalid/INVALID-002.json
formal_verification/rigor_kernel/kernel_binary/vectors/invalid/INVALID-003.json
formal_verification/rigor_kernel/kernel_binary/vectors/invalid/INVALID-004.json
formal_verification/rigor_kernel/kernel_binary/vectors/invalid/INVALID-005.json
formal_verification/rigor_kernel/kernel_binary_vectors/V1.json
formal_verification/rigor_kernel/kernel_binary_vectors/V10.json
formal_verification/rigor_kernel/kernel_binary_vectors/V2.json
formal_verification/rigor_kernel/kernel_binary_vectors/V3.json
formal_verification/rigor_kernel/kernel_binary_vectors/V4.json
formal_verification/rigor_kernel/kernel_binary_vectors/V5.json
formal_verification/rigor_kernel/kernel_binary_vectors/V6.json
formal_verification/rigor_kernel/kernel_binary_vectors/V7.json
formal_verification/rigor_kernel/kernel_binary_vectors/V8.json
formal_verification/rigor_kernel/kernel_binary_vectors/V9.json
formal_verification/rigor_kernel/mutation/README.md
formal_verification/rigor_kernel/mutation/admit_proof.py
formal_verification/rigor_kernel/mutation/expected_failures.md
formal_verification/rigor_kernel/mutation/gen_expected_failures.py
formal_verification/rigor_kernel/mutation/kill_set.sh
formal_verification/rigor_kernel/mutation/lib.sh
formal_verification/rigor_kernel/mutation/mutate_asert_numeric.sh
formal_verification/rigor_kernel/mutation/mutate_block_proof.sh
formal_verification/rigor_kernel/mutation/mutate_chain_and_witnesses.sh
formal_verification/rigor_kernel/mutation/mutate_chain_induction.sh
formal_verification/rigor_kernel/mutation/mutate_classifier_partition.sh
formal_verification/rigor_kernel/mutation/mutate_classifier_shapes.sh
formal_verification/rigor_kernel/mutation/mutate_completeness.sh
formal_verification/rigor_kernel/mutation/mutate_concrete_refinement.sh
formal_verification/rigor_kernel/mutation/mutate_coverage_closure.sh
formal_verification/rigor_kernel/mutation/mutate_execution_boundary.sh
formal_verification/rigor_kernel/mutation/mutate_extraction_hardening.sh
formal_verification/rigor_kernel/mutation/mutate_fork_choice.sh
formal_verification/rigor_kernel/mutation/mutate_integer_model.sh
formal_verification/rigor_kernel/mutation/mutate_kernel_core.sh
formal_verification/rigor_kernel/mutation/mutate_kernel_transition.sh
formal_verification/rigor_kernel/mutation/mutate_maturity.sh
formal_verification/rigor_kernel/mutation/mutate_non_vacuity.sh
formal_verification/rigor_kernel/mutation/mutate_oru.sh
formal_verification/rigor_kernel/mutation/mutate_oru_domain.sh
formal_verification/rigor_kernel/mutation/mutate_pow.sh
formal_verification/rigor_kernel/mutation/mutate_refinement.sh
formal_verification/rigor_kernel/mutation/mutate_reorg_replay.sh
formal_verification/rigor_kernel/mutation/mutate_res.sh
formal_verification/rigor_kernel/mutation/mutate_rfc019.sh
formal_verification/rigor_kernel/mutation/mutate_rfc020.sh
formal_verification/rigor_kernel/mutation/mutate_runtime_boundary.sh
formal_verification/rigor_kernel/mutation/mutate_script_spend.sh
formal_verification/rigor_kernel/mutation/mutate_supply.sh
formal_verification/rigor_kernel/mutation/mutate_surface_translation.sh
formal_verification/rigor_kernel/mutation/mutate_utxo.sh
formal_verification/rigor_kernel/mutation/mutate_validator_soundness.sh
formal_verification/rigor_kernel/mutation/reconcile_kill_sets.py
formal_verification/rigor_kernel/mutation/resolve_failure.py
formal_verification/rigor_kernel/mutation/run_all_mutations.sh
formal_verification/rigor_kernel/proofs/KernelTransitionSoundness.v
formal_verification/rigor_kernel/proofs/ORU_Conservation/.gitkeep
formal_verification/rigor_kernel/proofs/ORU_Conservation/ORUConservation.v
formal_verification/rigor_kernel/proofs/Pow_Correctness/.gitkeep
formal_verification/rigor_kernel/proofs/Pow_Correctness/ASERTCorrectness.v
formal_verification/rigor_kernel/proofs/Pow_Correctness/PowCorrectness.v
formal_verification/rigor_kernel/proofs/RES_Issuance/.gitkeep
formal_verification/rigor_kernel/proofs/RES_Issuance/RESIssuance.v
formal_verification/rigor_kernel/proofs/Script_Correctness/.gitkeep
formal_verification/rigor_kernel/proofs/Script_Correctness/ScriptCorrectness.v
formal_verification/rigor_kernel/proofs/UTXO_Soundness/.gitkeep
formal_verification/rigor_kernel/proofs/UTXO_Soundness/UTXOSoundness.v
formal_verification/rigor_kernel/proofs/UTXO_Soundness/UTXOTransitionSoundness.v
formal_verification/rigor_kernel/release/AUDIT_PACKAGE.md
formal_verification/rigor_kernel/release/AuditorChecklist.md
formal_verification/rigor_kernel/release/BinaryArtifactIndex.md
formal_verification/rigor_kernel/release/CategoryBCoverageDebt.md
formal_verification/rigor_kernel/release/CertiCoqCompCertPlan.md
formal_verification/rigor_kernel/release/CertiCoqToolchain.md
formal_verification/rigor_kernel/release/CompilerBoundary.md
formal_verification/rigor_kernel/release/ConcreteRefinementBoundary.md
formal_verification/rigor_kernel/release/ExtractionBoundary.md
formal_verification/rigor_kernel/release/ExtractionBoundaryHardeningReport.md
formal_verification/rigor_kernel/release/FinalAssumptionAudit.md
formal_verification/rigor_kernel/release/FinalClaimsBoundary.md
formal_verification/rigor_kernel/release/FinalProofInventory.md
formal_verification/rigor_kernel/release/FormalConeMap.md
formal_verification/rigor_kernel/release/FormalTrustRoadmap.md
formal_verification/rigor_kernel/release/FormalizationFrontier.md
formal_verification/rigor_kernel/release/FrontierExpansionPlan.md
formal_verification/rigor_kernel/release/IndependentAuditReplay.md
formal_verification/rigor_kernel/release/IndependentAuditReport.md
formal_verification/rigor_kernel/release/KernelCompletenessMatrix.md
formal_verification/rigor_kernel/release/KernelCompletenessReport.md
formal_verification/rigor_kernel/release/KernelDeepeningReport.md
formal_verification/rigor_kernel/release/KernelExecutionBoundaryReport.md
formal_verification/rigor_kernel/release/KernelInventory.md
formal_verification/rigor_kernel/release/LevelDefinition.md
formal_verification/rigor_kernel/release/MutationCompletenessAudit.md
formal_verification/rigor_kernel/release/NullContentInventory.md
formal_verification/rigor_kernel/release/OpenProperties.md
formal_verification/rigor_kernel/release/P0-119-DecisionReport.md
formal_verification/rigor_kernel/release/ProofEvidenceIndex.md
formal_verification/rigor_kernel/release/ProofSourceManifest.md
formal_verification/rigor_kernel/release/PublicClaims.md
formal_verification/rigor_kernel/release/README.md
formal_verification/rigor_kernel/release/RefinementMapping.md
formal_verification/rigor_kernel/release/ReproductionGuide.md
formal_verification/rigor_kernel/release/RuntimeBoundaryClosureReport.md
formal_verification/rigor_kernel/release/SourceManifest.sha256
formal_verification/rigor_kernel/release/TCBInventory.md
formal_verification/rigor_kernel/release/TheoremClassification.md
formal_verification/rigor_kernel/release/TheoremIndex.md
formal_verification/rigor_kernel/release/TrustDebtMatrix.md
formal_verification/rigor_kernel/release/UnclassifiedAllowlist.md
formal_verification/rigor_kernel/release/VerificationScope.md
formal_verification/rigor_kernel/release/VerifiedCompilerPath.md
formal_verification/rigor_kernel/release/VersionManifest.md
formal_verification/rigor_kernel/release/apply_audit_reclassification.py
formal_verification/rigor_kernel/release/apply_frontier_12_reclassification.py
formal_verification/rigor_kernel/release/apply_frontier_14_reclassification.py
formal_verification/rigor_kernel/release/apply_frontier_21_classification.py
formal_verification/rigor_kernel/release/apply_frontier_24_classification.py
formal_verification/rigor_kernel/release/check_release.sh
formal_verification/rigor_kernel/release/check_source_manifest.sh
formal_verification/rigor_kernel/release/final_adversarial_audit.sh
formal_verification/rigor_kernel/release/freeze_source_manifest.sh
formal_verification/rigor_kernel/release/gen_final_proof_inventory.py
formal_verification/rigor_kernel/release/gen_unclassified_allowlist.py
formal_verification/rigor_kernel/release/independent_audit_replay.sh
formal_verification/rigor_kernel/release/inventory_metadata.json
formal_verification/rigor_kernel/release/source_manifest_common.py
formal_verification/rigor_kernel/run_kernel_critical.sh
formal_verification/rigor_kernel/scripts/check_category_b_coverage.py
formal_verification/rigor_kernel/scripts/check_forbidden_tokens.py
formal_verification/rigor_kernel/scripts/check_mutation_coverage.py
formal_verification/rigor_kernel/scripts/check_no_mutation_switches.py
formal_verification/rigor_kernel/scripts/check_null_content.py
formal_verification/rigor_kernel/scripts/check_theorem_classification.py
formal_verification/rigor_kernel/scripts/diff_oracle.py
formal_verification/rigor_kernel/scripts/diff_verify.sh
formal_verification/rigor_kernel/scripts/extract_and_test.sh
formal_verification/rigor_kernel/scripts/verified_compiler_path.sh
formal_verification/rigor_kernel/vectors/.gitkeep
formal_verification/rigor_kernel/verified_compiler/.gitignore
formal_verification/rigor_kernel/verified_compiler/README.md
formal_verification/rigor_kernel/verified_compiler/fragment_checker.c
```

`TRACKED_PROOF_SOURCES=TRUE`
`HEAD=38356df9362324fcdb4a3a253a1397cb12fe1a24`
`worktree_v=121` `tracked_cone_v=218`

## Files

| kind | path | sha256 |
|------|------|--------|
| source | `Audit/KernelAssumptions.v` | `a8a07509ffbb3e47561364bd573801659169ff59b0ab01de5f628c0a4891cf95` |
| source | `Audit/KernelNonClaims.v` | `ab4d559fe1f90cb87112d477aa4cf3f61de2d7c30e70707ba43fd9174083eed1` |
| source | `Audit/Task32AFlags.v` | `56660f4ce7f67131c2d419f076db3c57654167ec0914149433fee71837cb51b8` |
| source | `Audit/Task32BFlags.v` | `67888b8a6125984b30f2a3d3834c62d59a36eb323d3e9f1af5b59f66723b3169` |
| source | `Audit/Task32CFlags.v` | `b1fef7f17cd81ab37d111b4919a73d99255cab16a4428bcc135c1cfcd62ae640` |
| source | `Audit/Task32DFlags.v` | `a23b196c8d6c9885b90f7dc323bff393e282f2e16a52adfc171a17b3bb918373` |
| source | `Audit/Task32EFlags.v` | `54dda8ad8223d57eb5001b393ea3b1a7425ee19539c447192a93a53c8f6875e2` |
| source | `Audit/Task32FFlags.v` | `81168e59d736b441e80fc945bbd7b523a505390fecdf45e46ed836ed4269cf89` |
| source | `Audit/Task32GFlags.v` | `a1d35e6930086a95d4a555b7621005b15ee5553c99c005c8b70b3a3b1b5cec4d` |
| source | `Audit/Task32HFlags.v` | `d941623d143251bd06b9eb5d5d1cd1c60b1d599dfede4c69d14fff38c6187e1d` |
| source | `Audit/Task32IFlags.v` | `ee3c2bd48ec5750cb1c4a96374f52b09683fffdcb7efdd76ec97946247ab1cf9` |
| source | `Audit/Task32JFlags.v` | `db784230245f2787b1c0130ad5e1a276d38782bef6adebf878259f23ed34cc4a` |
| source | `Audit/Task32KFlags.v` | `335ed000b4f117e70558681859371e07f0221fb7bb193418972e0d16c7b3ca0d` |
| source | `Audit/Task33A10Flags.v` | `96c55cccf4b3e27b24072345d1046b3138dfd4d651ae9447637e08f9adc86db5` |
| source | `Audit/Task33A2Flags.v` | `bc04c7fbef77b996fbce6a9a98488fc4d6fd632ff5eb847385b9f30dffb7476d` |
| source | `Audit/Task33A3Flags.v` | `d36c1772208b32ca813801c2fcc1716459fa0eef3744b13d99a0f39d04d8d883` |
| source | `Audit/Task33A4Flags.v` | `4d4b52732ea6d00226768f6e8a01df85ad9427830bba8e80c66b29634b839459` |
| source | `Audit/Task33A5Flags.v` | `c416df95947ff100b2c8af30e7a0c9894dd81c435834c6c5aa6f392040ca4d43` |
| source | `Audit/Task33A6Flags.v` | `cb59eb86cb4dd5bc391aad3f30565c57aae561ce362cb45ffe6c1f1f08dfc6b2` |
| source | `Audit/Task33A7Flags.v` | `38c0e58b1543a6a5ed089a0dc346ea82a7f21ca9214b11045c8f0fe65abef3c2` |
| source | `Audit/Task33A8Flags.v` | `84540463e1ff46fc3d2e7af056d5a587e09f3926a6e8944d1bc6f15bbf652e8f` |
| source | `Audit/Task33A9Flags.v` | `8aedfeb7892bdffe156635b6465c80a99e8375e41b9656f7648cdb2f1f2ae748` |
| source | `Audit/Task33AFlags.v` | `899e455559302fb5a5d24571cc2bc6c91f8a0eee8a8b49b717ae942355910848` |
| source | `Audit/Task33BFlags.v` | `2e569236297a383e661ebf825b8653658b2ac2205e3b8d6e253c5211647cf951` |
| source | `Audit/TaskBoundaryMapFlags.v` | `91f8b3b1ee0a127139cdc56d302abfb2090f4a581676285fc917ddf29e5a9e6a` |
| source | `CheckerContract/CheckerSpecification.v` | `81e56bf7fc2dfd9e5df9ca979088ef83ba2bd5caaf64b2edf699e98a18813c66` |
| source | `Core/Amount.v` | `7e3ef55c9d612886306f010793de86c7984e7c01d0d241329d9725da34aefef7` |
| source | `Core/Bytes.v` | `788bf0440cd0c38c4d7ef3a0387260948c255beb01fde2950514cd0c19b1c88c` |
| source | `Core/Height.v` | `42383b1a48912aa8f10386004fbdd09803583a0130c8551a0dd331f543d49e13` |
| source | `Economic/DomainSeparation.v` | `a9248ade4f5bb82d2bfd9a6f6e45a61fef53da05af6132c704e6dfe256283c00` |
| source | `Economic/ORU/ORUDomain.v` | `c305a7b16cc24064a9cf1471b519440497052f6319e7a11e77b8ca397f3bba9d` |
| source | `Economic/ORU/ORUSoundness.v` | `3a90afb42ce4955201a061995b5ca051049631af4e9755a9f1c52f300b691865` |
| source | `Economic/ORU/ORUTransition.v` | `0adc9f084e84bf37370450b722d770da8dbf2c2186cf36fd64c686b56906c46e` |
| source | `Economic/UTXOEconomicBridge.v` | `1aebc2b66eb490440d34b61c05477fbc8558d911ba7d4e0ed8b9530adb3b58f0` |
| source | `Execution/KernelMachine.v` | `c1caefdc7741428901305d18d3b8446ba2e60f9fb7f84080662692d3afae3449` |
| source | `ExecutionBoundary/ConcreteExecutionState.v` | `99a143f4cc44c4c44e6a580665888758461d94be0ca440e6012a02d1f78ed089` |
| source | `ExecutionBoundary/ExecutableInvariance.v` | `5969a09ee4b39130bccdaee96e1a08334b73e1e64c6b126c57af857c5346b4b1` |
| source | `ExecutionBoundary/ExecutableTransition.v` | `77dd6348f4f425dc2cb456c692fb0eb5078d9e642db36e185001781312f9d85f` |
| source | `ExtractionBoundary/ExtractionInterface.v` | `fbf04eb63ddca2c942ec76cd97ed1b99eebae5341e73985e258cee19724df95a` |
| source | `ExtractionBoundary/ExtractionModel.v` | `2fec974df78d723a056ae7826ddd6de7b3d004d56d8f21c34b6c8acda7479d92` |
| source | `ExtractionBoundary/ExtractionTrace.v` | `c77282ebf26af51219d48cc6fcfff121fa39092d416f9f28bee3208a1fad32b4` |
| source | `ExtractionBoundary/GoldenNegatives.v` | `21ef8014505b3e0120238cb602929ffbb065cfd61377c2bf7a7869da5288c4c6` |
| source | `Kernel/ASERTValidator.v` | `3623aad41f82efb3f615f27374b284e9895d371ebb3f508cc0bbaf4078541ef5` |
| source | `Kernel/Block/Acceptance.v` | `08abeec16b58a38c36d02e88ac99618e9a7fac1d4e80a9bcf12c3b7e2a90fbee` |
| source | `Kernel/Block/Types.v` | `9f5c6d9a8a4883cd0314abb04e20bc3fffcfe2758df4eee552dff4f3c8996888` |
| source | `Kernel/Chain/ChainRefinement.v` | `07d7e5a7f4b20543792367bfa5c0b68748a44bd5c5a8b9fdc9d0471552c47a43` |
| source | `Kernel/Chain/ChainTransition.v` | `782ae489147b5c874c20941306c8777f57798c3d16120f73f8a6e57f5e22af22` |
| source | `Kernel/Chain/ForkChoice.v` | `73cf631276dc4ffc25bbb1d07e70087a2e47ac431d6d7d5119a912d35f3eb8de` |
| source | `Kernel/Chain/ReorgReplay.v` | `d6e4c82ef31b62d757e82d969946b4253eaf07431d11fa617876b7627f9099cb` |
| source | `Kernel/CryptoAssumptions.v` | `ed8c8eba7cf9a798d5b2251046ab5535424f713bf97b78b62db7aca6d4702a8f` |
| source | `Kernel/EconomicInvariant/Economic.v` | `d5affa8ea8dffd3de7a48836e81e6d2503924e8c6468145e823a251971f7174e` |
| source | `Kernel/ExtractionBoundary.v` | `b63a1935891f934e95690066715fbbb9b21536b91ea2bc891333759596d867bd` |
| source | `Kernel/InvariantBundle.v` | `36a34bdeca7413bd5a0890980d6e5569c0af963d0a038b8af039d52c221ef366` |
| source | `Kernel/KernelInputBridge.v` | `7b35af6a37263cd7f18dc7f9805877b2136387f772987950cafdbf7bd55ad11b` |
| source | `Kernel/KernelInterface.v` | `179161f2a2328aaa0ecf0f448a190e7a5036e211453f1ee36bd7686e718e654b` |
| source | `Kernel/KernelSoundness.v` | `946c929bde1bbc8e271af5e9824b813ca5c6b661dd4189c070de1c0c45386947` |
| source | `Kernel/ORUValidator.v` | `b9ccae6d20dbbfeecc3d732001171e679280ebac3c20869ab38824cb2fccd487` |
| source | `Kernel/PowValidator.v` | `477c3abf46720f6d044f00f81bcfcd5f582db4929c2894d9f49be0f48a92cbf7` |
| source | `Kernel/RESValidator.v` | `b7f44e00c966ceb43e7209052cfbcab522ed5aeb7c5db660fd6fd7041abe54f5` |
| source | `Kernel/RefinementBoundary.v` | `35e551cb04ea6180d0df3df7ac54bb3895d6cd31355ebc2e553676d7ed33a664` |
| source | `Kernel/SafetyExtensions.v` | `ca4182fe2743af583cbdc8e0526160af742a4463a8e68f242a30bd8492ab8fb8` |
| source | `Kernel/ScriptValidator.v` | `be8c99ad08ef90425bfb53c021704c4dcefb09e091d15862d59ea2a2de2f3660` |
| source | `Kernel/SerializationBoundary.v` | `44503cb9d2f3ad4ec213dca3a753bfe72f2aa7fee0c60647894a27c2849a099c` |
| source | `Kernel/SurfaceCorrespondence.v` | `f030a05f972a0e8aa06dd82487b2ce2865e8a59bd39a1f4d8ceab1bc7ee15a0e` |
| source | `Kernel/Transaction/Types.v` | `d647e89c3cb844e2a356f8623cb8855866e3d2c6253498770cd3846cd38128e8` |
| source | `Kernel/Transaction/Validity.v` | `901bee4605120eb3eb7b0c92a4ceb5de795d49dc1d4d80a07121e66163931dde` |
| source | `Kernel/Tx/Maturity.v` | `395d99161d9db4708a5e4cfcbeeda495accc7b06c664fa81bba722687613cba9` |
| source | `Kernel/UTXOValidator.v` | `b29c40662771e6436cb430e72b8608580e0ac34a00d49e694dc23eebc43260f9` |
| source | `Kernel/UnifiedState.v` | `ab8e9c9e08fa24a2dd7d5e6ea10f8e2e9cfa5457c10d50c00a2ac1bea61ba7b2` |
| source | `Kernel/UnifiedTransition.v` | `6df08c38f8c23e8245c467a23a683e112b0de318e6f5467a84b5122e6ce50cc3` |
| source | `Kernel/VerifiedCompilerBoundary.v` | `31bbf4019408142f302d24b48820c2582f6c682400c9ee756b4bcb39f0d2756f` |
| source | `Pow/ASERTBoundary.v` | `4a4e8a1d700cd86a23cf0cad4c9240de14d16e0fa2bee39fcedcd49192251c3c` |
| source | `Pow/ASERTMatrix.v` | `292714bf9703e337e67ce06d62c546bbceab05cc8ee63110ae2f0594c54fc1d7` |
| source | `Pow/ASERTModel.v` | `339c85967247ec9b61d4014fedab85991469ce7e14598dc3b49943405e2b970d` |
| source | `Pow/AbstractHash.v` | `615d179b0db96feff9a050398057a866753d07f8fc85f53b161c24d2c6fc13da` |
| source | `Pow/Acceptance.v` | `f59da52afa67c6da69b2531e246724a0d677251249a22955e9ff575d2ca13deb` |
| source | `Pow/BlockProof.v` | `ac0b23816f334e927779dac45e3acb6bba93ef7df9e364dfe0b3f1b9302fe825` |
| source | `Pow/CompactBits.v` | `7825ffea2de96a4814828e0d3f3ab936204556e8e126adbd13239b046b6ebaee` |
| source | `Pow/PowModel.v` | `e3097cd9191ae1d0d118ce5ebd0483e21dfa5c845c90fb3be614646aec471faf` |
| source | `Pow/TargetRelation.v` | `b7b49217164ad5a38333ecc48646051d2b2022c72d621bfa7f9f306cce6074ed` |
| source | `Pow/Uint256Model.v` | `6d868a26e9ca4598d69bd3c81cead58d87d62a43f003b28b215cbf5d051ed3b8` |
| source | `Refinement/AbstractKernel.v` | `7b0c4ac87e04cce253cf356c3dbdae942f225e6206dd99febecb343d8b956d7c` |
| source | `Refinement/ConcreteKernel/ConcreteKernelMapping.v` | `324e8805097fd527daa6b5f6481c46893d69e53b8277e23f3eea0b545738fe88` |
| source | `Refinement/ConcreteKernel/ConcreteKernelState.v` | `36c755fdf86f94d108b27aed418d95917d214e55e652ed64b94efdec4cc7820d` |
| source | `Refinement/ConcreteKernel/ConcreteKernelTransition.v` | `1f33fb7b3d2defa1e1963c8d028033ce928c66dfaf2ff94894e2c34c522946b9` |
| source | `Refinement/ConcreteKernel.v` | `a42717d93c78168fc56c268ee1541eb026f9ce94b64fbd26a5bef9681719e82c` |
| source | `Refinement/EconomicRefinement.v` | `97302b588be982abd57dc404f95c075349adb14300aec7399fcd7a71da6b2608` |
| source | `Refinement/EconomicState.v` | `35c27e58c9fa6f8edecbde405473ba96214739d20ef26e535adcd76265a1b273` |
| source | `Refinement/KernelComposition.v` | `f3cdc08962253b6ca928663f169d004e2322f7da0ad3ca960108fce36e22b428` |
| source | `Refinement/RefinementRelation.v` | `72e5760306eb5eacc103c13cfaa080edcae851c54b6f856af33d2988a762aa88` |
| source | `Refinement/TransitionRefinement.v` | `c4c1a5507b6a672010f6357733e691bd9df8dc210cf3aa62cdbab5ddfbbf7697` |
| source | `Runtime/MachineState.v` | `af2820e453a319068ead4c0956795b0ec2ba2e93134574752e94d27094d668c7` |
| source | `Runtime/MachineWord.v` | `994db082c111405c638e713826a6c2efc2efee6f6138c509a9d071a69d89696e` |
| source | `Runtime/RuntimeFailure.v` | `e26a9a7cdf529d5b13d1aefbdc0c3e1452c63fc53d48d81916a59bd6d2de3513` |
| source | `Runtime/RuntimeSemantics.v` | `6c3e16c1ac3e74fe501afeda7dbd6b0347304503a1919ada68342d842f74d970` |
| source | `Runtime/RuntimeState.v` | `281c965e94dd73e664ee2ce7999b1ab22ea976aaa77e6de86dd0c38498216bc4` |
| source | `Runtime/RuntimeTransition.v` | `1cd6e6960eb5f648ae4c31f951ebb84281acef9e294d457ee04893c45824cf90` |
| source | `RuntimeModel/IntegerBoundary.v` | `81c205c978375f6eb889f4f603e3c03e784e9c140b8ed9913079b1e75fd3a6a3` |
| source | `Script/ScriptClassifier.v` | `4ced5baa66673249fbc9eea622c99fd049504b48ca5e16812340e2766e9f1066` |
| source | `Script/ScriptTypes.v` | `9d4a26268e1fb01509bf13c4fe633094c2cbe62a394f61fd76ae57f9e5d1d40e` |
| source | `Script/Semantics/Execution.v` | `41e9932c2433a9d5361da499cf36e99aef6101c8ee9d1d45c4296dd057c3f971` |
| source | `Script/Semantics/ScriptState.v` | `25043552d5583f1f68d59180e996b907b138b6b4530c122b492e0ca0950f6df3` |
| source | `Script/Semantics/ScriptStep.v` | `b5e857575bf981692a2237ae2edd43b433579c6a1d26190b1c820c0a6853a1aa` |
| source | `Script/Semantics/ScriptTrace.v` | `a9892abcd65f13fda882bd5b50332d057b3bb31a5b58eb8572ad52a5a0a9b363` |
| source | `Script/Semantics/Spend.v` | `3a82f3c69de59cddba0ce9592a47f1dbde23a413d74caea4bdec8adb73c6befa` |
| source | `SerializationBoundary/KernelSerialization.v` | `2e134fc1bb6698c7ac7e3f27bcc083a685a129dd619bbc82f99e34fa798f53cf` |
| source | `Supply/FeeDomain.v` | `e8fb2d028daccd82d19f625eee995f6226cc0c19183f3827057a9b83b9142976` |
| source | `Supply/ORUConservation.v` | `054ce36a65ebecc79ef87b62f8e9bee538404bef70a4570c158d574f426eaf28` |
| source | `Supply/ORUModel.v` | `5d7561f63da325584ae120f4f8717e91f0537415862567d3d0adb797033d39f9` |
| source | `Supply/SupplySchedule.v` | `3253f922aa7a062e3c62e8ff0bed775bfec87802ee7b70bcd9572d9aa2f2ddf8` |
| source | `Supply/SupplyTransition.v` | `fef7087bad700d394dc7ae4a6e175a629b8ff352ad096edb19424dcdc7f5d8c3` |
| source | `extracted/Extract.v` | `e4af88463d79b29b331511cff69fea2d7485ed9ed6be3e1ea0e900571e8d2856` |
| source | `extraction/ExtractKernel.v` | `9fffebaddac4fa009f2481f62bedaf35196669bbeb1ec62a9500048e780388b6` |
| source | `proofs/KernelTransitionSoundness.v` | `b90f77807b0b08711e4a1eefeeec40b82c131e43f280e2fd536476ca8dbb3704` |
| source | `proofs/ORU_Conservation/ORUConservation.v` | `3338a133f9dd3a9d3e9da64387474c9afb700b5ba13952ad72609b65263eab57` |
| source | `proofs/Pow_Correctness/ASERTCorrectness.v` | `d4fd33081b247179df573cc0b220cbbaf99980adf40cd28f58253cf4bd726b3f` |
| source | `proofs/Pow_Correctness/PowCorrectness.v` | `f9d09b911076aae4ba577807f8dce9f989f989cd5b7ea85cabdc398725ead5dc` |
| source | `proofs/RES_Issuance/RESIssuance.v` | `778f0a5c7574ca293de15b32f4200fa290bd8cac0ac98dfaff5e6bac60a31013` |
| source | `proofs/Script_Correctness/ScriptCorrectness.v` | `cb06a6e272128521d7af9dc02aa8ee37df9fa30b3a960c5dd263ede6b27a4630` |
| source | `proofs/UTXO_Soundness/UTXOSoundness.v` | `56f75c978751ba3ff3c4e9d6c8bffa07db596c2437ed2bc675af6f660c5e8f8f` |
| source | `proofs/UTXO_Soundness/UTXOTransitionSoundness.v` | `34a7b17561ab0a2518a5c90a40ff18f784da121e19fbe10607cb7b225372c1c5` |
| source | `README.md` | `384896b77ae48dd112452a31de0c0438403b397860ed75565039db4882210ee4` |
| source | `evidence/ASERTScope.md` | `e5bf997d83ad9780390285a21c85175365a021eef886ceaf2111fadd6175d169` |
| source | `evidence/BinaryMutationReport.md` | `6f50d0aae4a54469be2cb67b78288d01d51c26e00564191e02fbc9d6bbcfe837` |
| source | `evidence/BinaryNegativeVectorReport.md` | `d73f0ba15db12b44d851326c0713588ad8df7cb9aea22c36ecd5b6e8a61be0c0` |
| source | `evidence/BinaryTrace.md` | `0079f46dd089e43060cd727fca8690ac4fce68fde0a59b6d50193c9583765b73` |
| source | `evidence/BoundaryCorrection.md` | `226502212214a80e356b6ca7b630cd684279f8f1026604243d9ce4da09a77b14` |
| source | `evidence/BoundaryMutationReport.md` | `afab517f06c25329388c23f34c18b3f9651433fad17339e81b0380d15d28afbc` |
| source | `evidence/CertiCoqCompCertReport.md` | `2195e4c0448006e04d24175dfcc663aed50deaebd85b8246f51ab9a196a39de2` |
| source | `evidence/ClaimsBoundary.md` | `c937bdbd5e0af9309f629a15633217c9ad20110706d9f637f50ecbb0af0b7a6e` |
| source | `evidence/DiffVerifyReport.md` | `4d566cf2dc5fecc5058766d47d40ccefaa6c8e7c7ad5c2d512f2a92847963924` |
| source | `evidence/ExtractionBoundary.md` | `62d5b6e795e0b9f1bfae27100c7fffbca9b4ebef44c408334bc2256c90135dc3` |
| source | `evidence/IndependentAuditReplayReport.md` | `c63f395da6f3f9ca8bf75c827b02d941c0d60e04d0b7f403b0469960df9df3e5` |
| source | `evidence/KernelCone.md` | `d95189139c670936d89ca6d8eccf290a8ed8eb1690b1d8630faa8acfc6b02a60` |
| source | `evidence/KernelCoverage.md` | `da64aea052bf6fc244a56ea4d816a0693314c1f656b7fce7504a9e99904beed4` |
| source | `evidence/MutationReport.md` | `bf1210a9ec14d18177cc02585b5f95351a7a1dcd333557e9f15e562fd2848f3f` |
| source | `evidence/PowScope.md` | `235007043ab33f7739348452162bf0de941e02fed9db6dedc13ed612be42eab5` |
| source | `evidence/ProofInventory.md` | `ee69f96691f200cb2adaaeed38ccb76782ebdcccd1962130d4984ccf476715f2` |
| source | `evidence/README.md` | `5d266d4923075b18b05093b3271797dc278e25c4d2af6f0d69bff7d97672e63f` |
| source | `evidence/RefinementBoundary.md` | `d71f2db5fd063b998a833c22d68b15f4e8ea45f03f39ed9cfb9970d1deddced7` |
| source | `evidence/ReproducibleBuild.md` | `1d0e8b7b6bb42607eb32f4c86613a3753e81186576f8c8fe8a4af5fbabc289bb` |
| source | `evidence/Reproduction.md` | `fb4e93eeea1e665742a8c50cd260536c4f5e48c776ad744414daac256c2cf60e` |
| source | `evidence/SerializationBoundary.md` | `80522f25c37f4be3b525266794b7e0519ff9dc80fcf63dff7f5784e6d2e0229d` |
| source | `evidence/TCBInventory.md` | `6a0fa62fd31742d21be3d132cf02fe2831796692cccaa855ad0c235cc9991ba3` |
| source | `evidence/TheoremClassification.md` | `3be4299210fc47c55741edc6174e670adb4def362e9b6bec4376a31d9be31c39` |
| source | `evidence/VerifiedCompilerPathReport.md` | `d673bc22e00a6aa25d0b30880dcf7f576911187ecf6e650cefbf732e5a9a5645` |
| source | `extracted/README.md` | `2c36719c52097b363b3b420d75b7d01307db4bcf05fea65a75946b0a2a3d02e1` |
| source | `extraction/README.md` | `7c69a5169d3cd906b689109a31b96da985a8e8def89bd11a02f89599b0242d6f` |
| source | `kernel_binary/BinaryBoundaryTrace.md` | `82db0b93c6a97d7a4440b13aced24b6dfbe7b5d15ca9f7447b1a8dafaa17d665` |
| source | `kernel_binary/README.md` | `fc39b7976b30050c06b42e194b552237a42781645f54251442af38c105c0b56f` |
| source | `kernel_binary/TRUST_MANIFEST.md` | `11335ec3015730a363c7ace2768d5fe4599f59be5ef542625fd08ce374ccf763` |
| source | `mutation/README.md` | `762e66c4fdd957c90fc58f1cf461e53a30619f1fd5f65a4c7984bb4ed08038b7` |
| source | `mutation/expected_failures.md` | `331f3b33851f66b2b79060ab555bbd7d62cbeb7618588e5dfe1ca0c627a3ec9c` |
| source | `release/AUDIT_PACKAGE.md` | `f25dfcbde6d5c48494888564ff482af99c8b90136bd8a4bc6cb99ba315a54230` |
| source | `release/AuditorChecklist.md` | `aa22ebc2d880bd97b7dd9af6c42ea085601cd81b3de01716eeb0d55f26767fb6` |
| source | `release/BinaryArtifactIndex.md` | `04feaa45afbb8ba0623325d49d8af504146d9a6ec907690b1513e27a53c4a80c` |
| source | `release/CategoryBCoverageDebt.md` | `51afd805dd6c79c3f70f1da44fc2746f8f360b4fb1042c193f1d5022e90af45f` |
| source | `release/CertiCoqCompCertPlan.md` | `fc01528895a485fe59babb8e6e4391a29715a63a44ab791e84fb03a784434c59` |
| source | `release/CertiCoqToolchain.md` | `350907023237512a15888a4fedaeac9a449eb40810b3aefd416ae22bc9fef73a` |
| source | `release/CompilerBoundary.md` | `09976ffde3100d110773964a7d2b360e7983100e6097e86e8bc224dd9b45c84e` |
| source | `release/ConcreteRefinementBoundary.md` | `3759fce4cf618dd30e2d781a57f6ec4f00b069d3d84d527a2b8e34aa84b7043b` |
| source | `release/ExtractionBoundary.md` | `4229d901f1406e71ed465c5582013cdcbc29b07fc0f7bb07a9ef26e716603530` |
| source | `release/ExtractionBoundaryHardeningReport.md` | `d0c0d7417269b0e06095cacc131ee3a84632f45125725a3bf78e56ce0a09ea14` |
| source | `release/FinalAssumptionAudit.md` | `afc3ee1ab228246d35ab1b31ca38845f955029eeb0e4b5344441cd9a29a5c9bb` |
| source | `release/FinalClaimsBoundary.md` | `232f7cbca10534f998505ccaa4a460c03cfee763b441342a82b104ae7ff07160` |
| source | `release/FinalProofInventory.md` | `75c70137dd7e5d9f26cd4e91f8105702159b7e71d5026ed753bf00cf338705be` |
| source | `release/FormalConeMap.md` | `32c61cf6e452e1b31842efa995ef8718cef472ef6035d18cd86e2ab2d43a4628` |
| source | `release/FormalTrustRoadmap.md` | `7e1bd93c6eea93d1fccbb1d072e1f6f5f042a3865f9aeae64a88a2b986379f65` |
| source | `release/FormalizationFrontier.md` | `6d8982403e31cdaeb16b5b2a561de7d89b8d8f1c0251c34acfe7a9ec85b1684f` |
| source | `release/FrontierExpansionPlan.md` | `f5b5d3c0fe51f3e42584259cb509039f5e43cd5815f2e2da7d3b9730033ee6dc` |
| source | `release/IndependentAuditReplay.md` | `1a578546d7d7f23f4bc8038f7442206f00fbc51ff4a9b0a198002ed962be6997` |
| source | `release/IndependentAuditReport.md` | `e9ebe56f3adf4ecdc8b41df030be671338dcb4b1439639f9e844b247de7c9bce` |
| source | `release/KernelCompletenessMatrix.md` | `ea373670941262e891c4339f0199d3afd94e13ab7ee9c1bf861f50175cec2bfb` |
| source | `release/KernelCompletenessReport.md` | `0769aefc3a5e8a4414574e939f8a559346a2e9b7a14417b0a9fb1daaa034d4f6` |
| source | `release/KernelDeepeningReport.md` | `4d0d1ffb1783a32603c639bd3eed2f9e1ce0400ddc6b8565e2ba0f33613cf485` |
| source | `release/KernelExecutionBoundaryReport.md` | `3f3b127dbc905e4b6e0bdfda4be43700df3ceddd126ee4e6d58d7effe71949b2` |
| source | `release/KernelInventory.md` | `2c9959db33c5a254abd43d174d30a4bf0f1eb24ae0bc4ee5975f7ca4beb3c8e8` |
| source | `release/LevelDefinition.md` | `c617fda903e2d159ce29d8d9a01e4ce74ba0cd0969b2dea4446cd33aff198ab2` |
| source | `release/MutationCompletenessAudit.md` | `e07b5238ee195c13b77d7df2543684e0e028f8c4ab966e3eff0a026b8ca5f6fa` |
| source | `release/NullContentInventory.md` | `dee45fe50e31c418be6eb4959e19532b30647ac3f9424f7000e865d13e6257a0` |
| source | `release/OpenProperties.md` | `0c6d891731aadb4269647b5eb7fdc0d359877f34c605cb50455670924a4639ab` |
| source | `release/P0-119-DecisionReport.md` | `6a22bcf815a8bf510e3e1dbf3828f1ee3ccaaf583e5223e7a75390fc3a21db6c` |
| source | `release/ProofEvidenceIndex.md` | `ce08388382ea1a8fcf22485be5692d97cfa8cbd29322bd2dce4b8df11b421c2a` |
| source | `release/ProofSourceManifest.md` | `8c486e8934ccdf2d7c5b74a4cd0fc27adba828f4fd841fffb93c6a3aa11568b8` |
| source | `release/PublicClaims.md` | `e101ee3393a379f4279e82391a54d1b87970a4c56413fb4670a3dfca1f36586a` |
| source | `release/README.md` | `9d732a28fe66e3e0d1c9ba88e3df4501c28b49b8d3c6f794e9f5e37147c49112` |
| source | `release/RefinementMapping.md` | `06725efe76485280b4197f9d2fd477a8246712da95e7982c7b4ef2726b39ba61` |
| source | `release/ReproductionGuide.md` | `1f49a01322f9d2606788d5707af37ecbd59fd293765456ebdf07470684436c6b` |
| source | `release/RuntimeBoundaryClosureReport.md` | `19d2feef387e83198e6ef86b1bc42270b4fad6f974282fbdeab056f78994e34a` |
| source | `release/TCBInventory.md` | `da9b04cd82589898bc420ec710e804cf88d5f8949a2097cecdbd3635bf758e0a` |
| source | `release/TheoremClassification.md` | `79e20d35eebe324c2dbebe4a1075f3cc0684b00ed4ad35cba4aa8a0c255d4c23` |
| source | `release/TheoremIndex.md` | `d99e2fd087e37c84eab16a1ad17930f4097e68fad2da1e438c4b0ce016c7a634` |
| source | `release/TrustDebtMatrix.md` | `7422322fc7a4242de38e0b3faa43cff2bed48def2b9313fdcad3e9a9df7f84a0` |
| source | `release/UnclassifiedAllowlist.md` | `2337068a88e1f33fcce3d4fd546fb8068626f02db88d041eed246c20bc25d98a` |
| source | `release/VerificationScope.md` | `4f703b072a19a964dcc5333f8623f9bf57ca79a2d750aca59e3f03da0fe761b7` |
| source | `release/VerifiedCompilerPath.md` | `cf7fc29753563b924ec6091eb3f0de75309b4298d7cb16024140b5b8ec7a9379` |
| source | `release/VersionManifest.md` | `4e02e239205e9043c8ed54ee8933b8b589a9fc4954141aa1ffa3b5778741b443` |
| source | `verified_compiler/README.md` | `a386fa504fa5e8691e7cda5e964806a798336c0032f7a87c670e11ad1aac7b15` |
| source | `extraction/build.sh` | `fcae8095390dc7e41dbcbf3d163a0b5a13f50c897ae821faa231c2a0a3e14e1c` |
| source | `kernel_binary/build.sh` | `62f8b7c618980458095e9f845d2521a51c6cc4ba219d2f2dcce5eed7abdcc7ef` |
| source | `kernel_binary/run_binary_mutations.sh` | `213155eb31ee176c1c566b3ce87d381e9587e3b5f70494a7adf3aaaf0646eb7c` |
| source | `kernel_binary/run_invalid_vectors.sh` | `e5024b98ce9114a44a2950c780009dc8d40c36cc1d894f2adaa9cd14988e3767` |
| source | `kernel_binary/run_vectors.sh` | `0b8ddfada7f4a05f18a46b85035c9ac1c87a382981d51cc4a2329b6c6825b6fc` |
| source | `mutation/kill_set.sh` | `52360387955222d22e61abbef42fcb9076831533f78ce328236b7f66a91d47ef` |
| source | `mutation/lib.sh` | `f45f9147e36b461669fb3d146a4681a5a7a9c57985b43ca3e6ecf823e967451d` |
| source | `mutation/mutate_asert_numeric.sh` | `1f58ca520c7a148b81b9a39c30b9827c8b3df1c418232a574c6f91c01558b3a5` |
| source | `mutation/mutate_block_proof.sh` | `e64f15379244201d82436389e50adede697407846a4c4b36dac4c4207cb50111` |
| source | `mutation/mutate_chain_and_witnesses.sh` | `27b0d2b7d10515c816b4be78ddfa746f25967f3a25f07d2df97948c7f66a7768` |
| source | `mutation/mutate_chain_induction.sh` | `8c414afa84faef206f83a4f5d92e3c147e2bf3e9db5d5e718ee2b9994dcfc6a7` |
| source | `mutation/mutate_classifier_partition.sh` | `da25fcb58199d29534fc4045a0315aeb05be444727a4aa953c3f8f7e50623405` |
| source | `mutation/mutate_classifier_shapes.sh` | `57d97d3c8191febf47971c85c6714490b5be1c1c11d75d2f3a219ec2beb95396` |
| source | `mutation/mutate_completeness.sh` | `dd131b56c26479f88a0454e645e6daf7b3b95d287d27164e5dbf221dd3c53b75` |
| source | `mutation/mutate_concrete_refinement.sh` | `cf872522bcbad0fb5e48722c8ad376e296a49fed5a5774acb748bfcebf0b978f` |
| source | `mutation/mutate_coverage_closure.sh` | `3651c62791d9f1063ff684aa7d00e102f2651185ad6803b8c939a5af101e9661` |
| source | `mutation/mutate_execution_boundary.sh` | `05d18ea716ca22c9f36cb72988de6ab33d3f1833b720f070495f46e6d1c38b6f` |
| source | `mutation/mutate_extraction_hardening.sh` | `d44f62c2cbe3292b69421741ccd42a7ff7f13ffeb6ee124f9168925985d9d6cc` |
| source | `mutation/mutate_fork_choice.sh` | `fc2b915201f8072ab1e048cb96d8f5ceaad444db0e96474ab08f8cc57e56b4f0` |
| source | `mutation/mutate_integer_model.sh` | `ce9ed0097c8e20c5f3ac4ec0053faca89dc75315260451bd430c4e443d29f1a5` |
| source | `mutation/mutate_kernel_core.sh` | `11563ab264ba15853d6631fae1fa8c2b54c07cb389a8fd74744a514bd8af5af2` |
| source | `mutation/mutate_kernel_transition.sh` | `aeda64a4663231ce94e039131a7467f39b38a505f442b935f070fb9e3203629f` |
| source | `mutation/mutate_maturity.sh` | `4d177ef96b8facb3e60b67b84fcffb1efa2dba0e55b8a4f498fb46cc6a86254b` |
| source | `mutation/mutate_non_vacuity.sh` | `53806b59e95e0ad367c186df1480775217174d235394b5703cdaec00e1f8ee33` |
| source | `mutation/mutate_oru.sh` | `05db624d7bdb0a43321e4bc61f450132796ee88a997b6b1b5f7e59faabb33a81` |
| source | `mutation/mutate_oru_domain.sh` | `9cfa336434d6701d636cc2743b3a37e9308df61587af73635a0d6a3a9be2b6cc` |
| source | `mutation/mutate_pow.sh` | `2063e6b067fb3f08c545962785d9a5d6f1635ab7e7ff730899241d9b673e3d98` |
| source | `mutation/mutate_refinement.sh` | `4018a2066db66040e634410ee42c64f97d970b5a89b0512d90b18037594ed7aa` |
| source | `mutation/mutate_reorg_replay.sh` | `377c6e1ff0160245d3e70c6aad59503fbf778113c01566faa6b2a92cbe5eef63` |
| source | `mutation/mutate_res.sh` | `24cf3a654d2f77612e1f983a6a132731c659e3d2c7d9473ec43c0f42bf825876` |
| source | `mutation/mutate_rfc019.sh` | `df5edb9e45cd51ddb8782b71aa35b0afb38e82acb30bdf63a3933e2c0d1b3c56` |
| source | `mutation/mutate_rfc020.sh` | `28609892aff3a47903f1c00b8487153da8305f8373609123ac1c32694e457023` |
| source | `mutation/mutate_runtime_boundary.sh` | `160b6f2b60508fbcab48e2d02671828d13f70ac0e46d32a29208b365cee798d7` |
| source | `mutation/mutate_script_spend.sh` | `21de5e35b9ccc2f741ef6c266ed9ec48fa1088a83cf8bcd607d844bd1112177a` |
| source | `mutation/mutate_supply.sh` | `1cdaec7dab0c3fc86b726ff37fbafb63ea101cc1803116ef4fff10dca325bf51` |
| source | `mutation/mutate_surface_translation.sh` | `624d6617951b3fd78f14d5e548efa789d8818fb31abbe6aee805a24b3c370e22` |
| source | `mutation/mutate_utxo.sh` | `41cdd7662320ff28ed933a3637c8e6948cf0ee699c70abf34105a4176cfc25ad` |
| source | `mutation/mutate_validator_soundness.sh` | `ebf689b4c85736e1a21bffc27a76bd5f12eb50e0286c9cd6215a07a53768eea2` |
| source | `mutation/run_all_mutations.sh` | `69e31a7b888a439c616e532bbf7966302a0d683eeb2b244c2b4562f1dc5d9f43` |
| source | `release/check_release.sh` | `eec3d3dca75a32f62224cb20ff2c4301928a3b9c7df653792536f9084da80764` |
| source | `release/check_source_manifest.sh` | `2ac3d053688b5631b4206d44e9721a143ade033107ef225e156656d35c942841` |
| source | `release/final_adversarial_audit.sh` | `a0e74008adf5486175afe9e9840f4e568b15e69955a74d3cc45375521c5b8b77` |
| source | `release/freeze_source_manifest.sh` | `f3f68f312dbd5684301d9ef0b0ea1e39d0f57785fe5b6085564b51fea9875358` |
| source | `release/independent_audit_replay.sh` | `c36d7efb70af4f9dc12d65c07298bddeb25b2186a9af29dbdf700f288b9822c2` |
| source | `run_kernel_critical.sh` | `1a7eadf93a37112fab01e52fe7a403c88a07852ab5cd834f85e9df11af8f6cc4` |
| source | `scripts/diff_verify.sh` | `bd12eb4204e2819205ceb90bb28addbf8fb760c3a316713bb0976de8fa505bbe` |
| source | `scripts/extract_and_test.sh` | `f5a52e055e7a62c76895ebbc11acfbd38002caebd8471bea890cb0537b075670` |
| source | `scripts/verified_compiler_path.sh` | `82d08a5e61dc4f75bdef2eb1e22c8172da62b28795ab225c30f6fff4463181a2` |
| source | `extracted/ExtractedKernel.ml` | `85383d7f743cbb62fb9a492f3be44d25f10e757aa2ebc75259965d173acd7345` |
| source | `extracted/test_driver.ml` | `2ad8a7937b4dd31e608803a2a7c8965b8c59df7c4264ef3b20af60a2326b64ba` |
| generated | `extraction/ExtractedKernel.ml` | `94782ad1f404d66adaaf405c39af6f52545415835f5d486f33d78d091c7aeadf` |
| source | `kernel_binary/ExecutableKernelChecker.ml` | `0e569f84bebc365563c4df6ed98e18114dba6d988505f41f26ff406af287daac` |
| source | `kernel_binary/main.ml` | `79e61f2dc8a186063ce891b850e8356979ab95070f21613b396cbea34871ae15` |
| source | `extracted/diff_work/extra_asert_quot_mismatch.json` | `86b0107248224ed1aeb44b789e7cbb5f42570573933d2f1ddcd00fc69f9884d9` |
| source | `extracted/diff_work/extra_asert_quot_tau.json` | `a3779d073fdfe80ee9b5d1b8c67af2fed205dbe0d59bf8f28143c2b4d4a8cb22` |
| source | `extracted/diff_work/extra_asert_tau1.json` | `9b9ba0f7ac73fc375e365c64370526b4bdeb96176612afe821b08a82a4c8bf4a` |
| source | `extracted/diff_work/extra_asert_te_neg1.json` | `5815f7267bc7a6dc7fb0d70f213966016b816f7b8e92be6a3d4cb3c2770730ae` |
| source | `extracted/diff_work/extra_asert_te_pos.json` | `1a3c7ad750fa184c520e07cb0fff5ea5ebe46d90513a5a4ad19e5958ff224d26` |
| source | `extracted/diff_work/extra_oru_domres_inflate.json` | `b868909ec504af05c9d02eb964aa7e42aeea6efdd8be2d8a48bfedd1dc7606fb` |
| source | `extracted/diff_work/extra_oru_domres_ok.json` | `4111f153ef22b38374c5b2905468a601867d546d3f338ea2a0b265f591f3f0b4` |
| source | `extracted/diff_work/extra_oru_domres_zero_in.json` | `6292737d92ca0f79716fa24c661f05907bb3e969a3d39dbe849d1a6e9463bc29` |
| source | `extracted/diff_work/extra_oru_equal.json` | `11cd3dcc461b7b67432b59fd5a09f20e4a928a786fb546910d0ee58526c8766f` |
| source | `extracted/diff_work/extra_oru_neg_leg.json` | `e6d7eb4fc72c25cd143e7abead3f831a09271f8c187fdbe4e62100fbfdde91dc` |
| source | `extracted/diff_work/extra_oru_res_leak.json` | `f9463d13d80099db7d137cc905a0e2f10be37d1b8bed4c6093a3d15fef6c997b` |
| source | `extracted/diff_work/extra_pow_above_target.json` | `904da85a818184fbf3a35016ec782b7f5809f61c016ec015fb1f75596e1d89fa` |
| source | `extracted/diff_work/extra_pow_compact_zero.json` | `b3cb7acec1d3a6153cc86aab739d5f31ca788c403743adad7bcf1d48a97185e1` |
| source | `extracted/diff_work/extra_pow_equal.json` | `df11d7e483c280f3aa34c76868f55b7b8ca764da8b82dc0a2cd4df5fb37d8bf0` |
| source | `extracted/diff_work/extra_pow_neg_target.json` | `d6dceab7ee5c6ef36318292ac4831a13662cb03d010d2f21d2d44a5ce44cebf6` |
| source | `extracted/diff_work/extra_pow_zero_target.json` | `8ad82984eb982331ac52b63c5f8fe9d4bd2bf60b5bacdc0469d38a2f44494087` |
| source | `extracted/diff_work/extra_rand_asert_0.json` | `5a146145934f418b76b528e328824ddea28d48e056bf20755ad31e7990023506` |
| source | `extracted/diff_work/extra_rand_asert_1.json` | `82963bf4ef58ef64e7eadf97035128dcae4905f87dcac96e2b408bea03bd3120` |
| source | `extracted/diff_work/extra_rand_asert_2.json` | `34b1e80df38aafa02567a4fe1e10d10d7dde3935b379d19b846c17363adb4ce7` |
| source | `extracted/diff_work/extra_rand_asert_3.json` | `bd5f27c9798234a9f0b89d960cc7a7d9cb9cf1a61c60523da378d1642b87c290` |
| source | `extracted/diff_work/extra_rand_asert_4.json` | `f0da4094efb36753d4f9942460c056a606579c7c46d556d756841822160c0561` |
| source | `extracted/diff_work/extra_rand_asert_5.json` | `ff1501fa610c7edf0f5e986dfe209c0b52ff83965e7a47501c10ef386012c21a` |
| source | `extracted/diff_work/extra_rand_asert_6.json` | `3495e1b2a1da7121c3ec4e22ca82143e86b92762851e092e0f54cf068de6de55` |
| source | `extracted/diff_work/extra_rand_asert_7.json` | `3926e6e6b4265d81a4531d3525711f864fbd482353fd3d2622633f7e939bebec` |
| source | `extracted/diff_work/extra_rand_asert_8.json` | `472eb1630a4c2217686b1cd5fdc17f75db21dd20f8e47bb6419e2aa95547030f` |
| source | `extracted/diff_work/extra_rand_asert_9.json` | `8434978e8bb661ce8b11e3e6f9fea21aed5edecf39437a65c1eacadf25ec7390` |
| source | `extracted/diff_work/extra_rand_oru_0.json` | `af14e1a0470979f8b6fa2477e8174f0f4d546d58c836302a93dd5046326d36af` |
| source | `extracted/diff_work/extra_rand_oru_1.json` | `5018c1fd634b69d568d7fba8b5d1838c6b9c6322679af1302ce048e8216ae8e0` |
| source | `extracted/diff_work/extra_rand_oru_10.json` | `adbd06dfb0479ed35d0523fcfaebe2a8acb99ddec7d5848b8d03b36987a94029` |
| source | `extracted/diff_work/extra_rand_oru_11.json` | `2757acdf5b8517a1cdc7bfc75094ef05e0a3952db53a777bccb2b0bf796bd837` |
| source | `extracted/diff_work/extra_rand_oru_12.json` | `1288c8d141eef2d4f8ba02e3dadbc7e362b8265d392eea543e1d5f1c2fcc73fd` |
| source | `extracted/diff_work/extra_rand_oru_13.json` | `8d7012f2d592858d8c2c2b5f66935e77ad44feae8a97ce70ae90584c285974db` |
| source | `extracted/diff_work/extra_rand_oru_14.json` | `dad727c258d7b9cc0591f89614c9167f055cc1d684f6506907d1405e0d2e26b5` |
| source | `extracted/diff_work/extra_rand_oru_15.json` | `7856f99c59039989fa6ef8b43ffb75e455ba92b6c91b5c13ba8e2d0c3ccff0e1` |
| source | `extracted/diff_work/extra_rand_oru_16.json` | `f15b3ef74376ef51b034ddd5d419e76dba32961ae56acfae87af0b952367fae2` |
| source | `extracted/diff_work/extra_rand_oru_17.json` | `0d76ad87f5b175b261aeebeb837ea1146b685f9cc12cecd8650243057f78a570` |
| source | `extracted/diff_work/extra_rand_oru_18.json` | `a999be72f4d0a67709067bf93c5767cb45d70f601e49c1950632086dbe7a3676` |
| source | `extracted/diff_work/extra_rand_oru_19.json` | `5b8c0dae090d60e13318f37605b725311661dc52a845a5cff8dd9699117d944c` |
| source | `extracted/diff_work/extra_rand_oru_2.json` | `43f5dc80504fa63327ce86c6da09d5cad7e1d4203ca52c970e2d86db21db0d09` |
| source | `extracted/diff_work/extra_rand_oru_3.json` | `5ace16c966f11d6722764387724ca584c4da0ffe2d07b25cbdd7ac8183dde8b6` |
| source | `extracted/diff_work/extra_rand_oru_4.json` | `220d024f05df2b79cbe056480be728904482861ecc9db9ee941974b73e274c9d` |
| source | `extracted/diff_work/extra_rand_oru_5.json` | `ee988296de27b547c7f7543637a34b687ed0194fd9c5e2088f95ff494f63dd0c` |
| source | `extracted/diff_work/extra_rand_oru_6.json` | `f6065bc8b37bc306da14de061db48dfac74dbc0cbbcd6636f1a99d30af9db02b` |
| source | `extracted/diff_work/extra_rand_oru_7.json` | `1e680a6d187ecb722e650788d441a66cd34364dc8e62d0c6933a0b9ca34bf292` |
| source | `extracted/diff_work/extra_rand_oru_8.json` | `eb0442a95663d528ed17218fb1ee69f6623ed65e99e69227a7f62bc54f0d4d51` |
| source | `extracted/diff_work/extra_rand_oru_9.json` | `a53736590765ca4d3d0f62a95f1d2a4c50427efda39b684882585cfb2ee14d9f` |
| source | `extracted/diff_work/extra_rand_pow_0.json` | `02c5f99a933bbef95a2dc0e68ce4006a922afb9de5f559e80fe6ef005580e5eb` |
| source | `extracted/diff_work/extra_rand_pow_1.json` | `a73d7c768e5b320c928e1e811c9b382cd3951666e1b5cb44faafc96476650f3e` |
| source | `extracted/diff_work/extra_rand_pow_2.json` | `829f3ea131aa5f47d191ed76de5356f80e8e58d16eabd8e31d615c1dc8d9f8f4` |
| source | `extracted/diff_work/extra_rand_pow_3.json` | `73065443debff491382a2541509b3e40c312885a1b893cf2e0249d146f3cc05b` |
| source | `extracted/diff_work/extra_rand_pow_4.json` | `513703fc712b1d43a51cd6d8b2c79e956a67f3f2b0dd53fad921e6ee507fe644` |
| source | `extracted/diff_work/extra_rand_pow_5.json` | `3939474612973643c457e11aade2be256f88d42a953d03636507365d4a95c749` |
| source | `extracted/diff_work/extra_rand_pow_6.json` | `3f6537deb8caa2256c90fe3aa38a340f4cd82ae5d6d90c79552519159a2e2a1c` |
| source | `extracted/diff_work/extra_rand_pow_7.json` | `1ec3630d078c88724d66335775a5f2ab8a529ccf995cb99359dc150e89a3c41a` |
| source | `extracted/diff_work/extra_rand_pow_8.json` | `c3fdb12208a3ad8ccb8b7f4170e0dde7e4de1db6683585eee7f1135c3550d84f` |
| source | `extracted/diff_work/extra_rand_pow_9.json` | `239a5a6fe4a135afd19e5bf18cb08c677defd3155ff30241d13d6c9b2129f242` |
| source | `extracted/diff_work/extra_rand_res_0.json` | `3529a57b2ea63504f75cda712b7188ccc79189599d8f8db71c95bf2b45653208` |
| source | `extracted/diff_work/extra_rand_res_1.json` | `0e638ef1779ba588231de7eee521ba48af12d4785c1fb39465c0e90dd79b5e8d` |
| source | `extracted/diff_work/extra_rand_res_2.json` | `41147f94e090f1d3ac76532621594637b77543792758e17cc64a60b756ccaf17` |
| source | `extracted/diff_work/extra_rand_res_3.json` | `c619d3fc234ac1b0315df88a1f70678faa4a0b3034e13e12304f171cbe6c0fda` |
| source | `extracted/diff_work/extra_rand_res_4.json` | `ff694848f6db54da2f7514edac8b62a38dfd6deffeae16f4785f1b2335cce97b` |
| source | `extracted/diff_work/extra_rand_res_5.json` | `bcb2e3d10181850addc9fdd64fa1c48669dbdf12172ec59d59790b84cfeea43a` |
| source | `extracted/diff_work/extra_rand_res_6.json` | `0e638ef1779ba588231de7eee521ba48af12d4785c1fb39465c0e90dd79b5e8d` |
| source | `extracted/diff_work/extra_rand_res_7.json` | `0e638ef1779ba588231de7eee521ba48af12d4785c1fb39465c0e90dd79b5e8d` |
| source | `extracted/diff_work/extra_rand_res_8.json` | `3529a57b2ea63504f75cda712b7188ccc79189599d8f8db71c95bf2b45653208` |
| source | `extracted/diff_work/extra_rand_res_9.json` | `bcb2e3d10181850addc9fdd64fa1c48669dbdf12172ec59d59790b84cfeea43a` |
| source | `extracted/diff_work/extra_rand_script_0.json` | `815475520ce608fcb3d0e74d14cf61518b4581545df78a1d24de4fae0ebc133d` |
| source | `extracted/diff_work/extra_rand_script_1.json` | `d43717b5a3047c3cc4eeaf800f561465ae4266f3386a84555a1bc1fbc7505a59` |
| source | `extracted/diff_work/extra_rand_script_2.json` | `c15940ffcfaf316455e9caf03aab11b7c73a2c4a61285b6481a0091a9fcaff51` |
| source | `extracted/diff_work/extra_rand_script_3.json` | `1ad52c30d64ea59456481f750e359ca39dc7c005eeaf82be48d4947306943d0d` |
| source | `extracted/diff_work/extra_rand_script_4.json` | `65bb5f3f0158aedc1e3ec784e0ec57f876d0737147bd611d441ebc82e2e88a01` |
| source | `extracted/diff_work/extra_rand_script_5.json` | `fd915fafe80cad95638d75bbea08a1367df37f80ad954500b7332d3f3ec2bf89` |
| source | `extracted/diff_work/extra_rand_script_6.json` | `d1aa170e60fd60d5424bf1c768c5f7b7e634b26004e91110bbb6fd38c5bf3e65` |
| source | `extracted/diff_work/extra_rand_script_7.json` | `324106ac3f30066bf89feb318f1d60a7f13e0cf6881b5204eaf01bbe16a7f649` |
| source | `extracted/diff_work/extra_rand_script_8.json` | `a42209d66eb3bf3c5271f5dc81ef550c7b64cbacae073ab4c2a03317238f0481` |
| source | `extracted/diff_work/extra_rand_script_9.json` | `af23439ddbdc048cfd9ea253363b25cfe8e1dd3729846d0d140ac8ab150843dd` |
| source | `extracted/diff_work/extra_rand_utxo_0.json` | `839e4bc9d8e08bec3e744f2f400b9a61b86d75fbf1c3319a1532561a33914049` |
| source | `extracted/diff_work/extra_rand_utxo_1.json` | `1692dd1026c093bdf7ea4ceafb6d168928a857924cdc91ec0fbfa170409819c5` |
| source | `extracted/diff_work/extra_rand_utxo_10.json` | `b25b74a0627af7b5e2a50fee0aa72069b19d4c259233382e6b993d219f017e7f` |
| source | `extracted/diff_work/extra_rand_utxo_11.json` | `409efd5c0e8021b6dedde5d991000e4af6f6d38e3e0035669cef127953746b16` |
| source | `extracted/diff_work/extra_rand_utxo_12.json` | `65271974f56368b192e0cfc2edeadb92cff1e975edec3a96ef7ffac20fcf0bc4` |
| source | `extracted/diff_work/extra_rand_utxo_13.json` | `0269f6629f0729c6ba8194d91679baa0664ba549c84369700575dd33d203f71a` |
| source | `extracted/diff_work/extra_rand_utxo_14.json` | `d3495d21b22e38f8e22c7b5f38b40dc6a68ecdf5009e12d5188f4e37b71e6474` |
| source | `extracted/diff_work/extra_rand_utxo_15.json` | `7e74c344bf5a31aef32b47c122b062c726a8aacad6089fdb47c0b9c384b8c8e8` |
| source | `extracted/diff_work/extra_rand_utxo_16.json` | `d7085779027c4430904098c003972aeb6ad43956730f27b8cbf7ec3ca15aa6a2` |
| source | `extracted/diff_work/extra_rand_utxo_17.json` | `5163202f04411ce1f645df4bfaac4d1aeff72644c46743faeab4f7649caed246` |
| source | `extracted/diff_work/extra_rand_utxo_18.json` | `99d100cec0df2edf8a9b915075c1989f18fed9f5d3a7a2293f0b486427cf1a60` |
| source | `extracted/diff_work/extra_rand_utxo_19.json` | `cff65528b713a08fa9786705ee8ba14581d233bec2bc5d3835c57d391e91d6d5` |
| source | `extracted/diff_work/extra_rand_utxo_2.json` | `5db303a24d55f04f77729955424f39ac821db560c7f2a541e432c83d1b6fd728` |
| source | `extracted/diff_work/extra_rand_utxo_3.json` | `5f4f5aa604a3969bd8ed50e270a3e4bc9d937872d27ab2246d17849c0b03fb0d` |
| source | `extracted/diff_work/extra_rand_utxo_4.json` | `307b3cf58adea569594236cb508866869294af92fe77438907dc8ad9a45204d4` |
| source | `extracted/diff_work/extra_rand_utxo_5.json` | `dd2f8b57aff9b52847521eafbb75693490b92fa432cf28a1e75efa2d99378abe` |
| source | `extracted/diff_work/extra_rand_utxo_6.json` | `6cfa69464afbd29c77b7911e572799e0ea446bd0ba26ae48f6f4e8e0eb1a3731` |
| source | `extracted/diff_work/extra_rand_utxo_7.json` | `825f7eb48bf678e6001a8eb767453c4ef9e943206ba14737d0ff780bcabc0c72` |
| source | `extracted/diff_work/extra_rand_utxo_8.json` | `1912afeea4df98d76b196d3ee6f94b815bb23708fb05575ac0b87feddcb852ba` |
| source | `extracted/diff_work/extra_rand_utxo_9.json` | `c0348b6bcbaf0ea50fb65fd857dad270a3fcca755dad1764c513a1b758a5051a` |
| source | `extracted/diff_work/extra_res_coinbase.json` | `91c5f81cfd917aba1aea5af692f86a88b1f8142c6d7fc9f0b5102e500edf598c` |
| source | `extracted/diff_work/extra_res_epoch1_exact.json` | `54c9248617bfa84a2b0aec156df8fe20f629fd00321d6e555a2ecc6079a25a56` |
| source | `extracted/diff_work/extra_res_epoch1_plus.json` | `d8cd73147bde5e695d3cf90ab521c1631d1e2eadbaa778c90bb88235b0c69268` |
| source | `extracted/diff_work/extra_res_epoch251.json` | `eabf0b41c4a01d02f8c8abc81886a3b49cde9a92b4a05ed9fbf40ae12d3bf15c` |
| source | `extracted/diff_work/extra_res_exact_w0.json` | `783cd507739fc99be39662b5f6fd27b8eb80d0183fb1c45b76081619b9baa153` |
| source | `extracted/diff_work/extra_res_neg_height.json` | `26040ef3fbe804bd3c29266f239cc86c48ae749f892218377e3e2ebcd7115de8` |
| source | `extracted/diff_work/extra_res_no_draw.json` | `ceb706eb58655644fcfee33a094a2d5f2ba4e4dc8b2b922b376e94e888ed0367` |
| source | `extracted/diff_work/extra_res_no_mint_shape.json` | `540ea14d7a9d15bd9ce42ccdfba18b3b936ea5ba22ff7021f1f216348ddfffa6` |
| source | `extracted/diff_work/extra_res_not_coinbase.json` | `73e1513dbf6dfb7079da74ecf707ea307749e5e18682ef3c421abfa63f7aa7a0` |
| source | `extracted/diff_work/extra_script_converge.json` | `6e8ac3ae900c63680fbb3597bda619791180695577a3253cabadd5fb006ba66e` |
| source | `extracted/diff_work/extra_script_empty.json` | `b9f41b1d65528d767fec0bd66fc060034aac514f8775aa893e7e8667dd15c1dc` |
| source | `extracted/diff_work/extra_script_mint.json` | `03a268cac1744c9a369d3d7d8fe62d39fbdc4d95da26192bce9718f54fbadbe9` |
| source | `extracted/diff_work/extra_script_null_data.json` | `ff53a689596bb6315f07b040d56b2bf5d56d09bb48f654cddf5041036594d516` |
| source | `extracted/diff_work/extra_script_opreturn_not_pushonly.json` | `55b74756308c6f801096527fdb94f7258a6bf36aa8c788aa2e7d3c9d095ecc8f` |
| source | `extracted/diff_work/extra_script_v0_wrong_len.json` | `3b398184aaf081f5424baa98ebc43457199050ce5e7abff396baf9155664245e` |
| source | `extracted/diff_work/extra_script_v1_cvg2.json` | `cc3782f4f2c84d16bdc780374aa487ac56bad9e646a09b8bf86334e42dbf2956` |
| source | `extracted/diff_work/extra_script_v1_native.json` | `d4a9c6485ad003dcc75307fe223239ff9bcf35f7050dd283a3c3d1229ef71ce3` |
| source | `extracted/diff_work/extra_script_witness_no_fallthrough.json` | `91010e43adfd4190163721f574c7c11f11191c92e940b5692167ff915e88e842` |
| source | `extracted/diff_work/extra_script_wrong.json` | `0bb90dc5694c49a64e2f11fd0ce5add62c7677d02e9e6e1c716bd6387d9a3f33` |
| source | `extracted/diff_work/extra_utxo_coinbase.json` | `759a9bc0d233cf73c560df148f5333b59455d11645c4ff689e1e158a128768e9` |
| source | `extracted/diff_work/extra_utxo_max_money.json` | `4828506249a317c2b48859f6cab848335de3950c33bf804e28adff624789abb2` |
| source | `extracted/diff_work/extra_utxo_zero.json` | `42fe803cbb6c87496022815751e97211fd3a885e870f0a2dd4c25c81baff0cdf` |
| source | `extracted/diff_work/gap_asert_exp_q_over_clamp.json` | `a85da5f656b2bc7cd4c4fbccf7fa2b64f8ab4460020d81f5257ec954ff4906ac` |
| source | `extracted/diff_work/gap_asert_nbits_not_compared.json` | `3abf918f42c9715ac7b058f04f298124f20ad12e8dc406f4b3dca22c13cdf35d` |
| source | `extracted/diff_work/gap_pow_compact_overflow.json` | `e8deb451cff1ac1af0ee0f1fbdfdf078cb25b7f4f9bb450826b6709baafbd3fa` |
| source | `extracted/diff_work/gap_pow_compact_signbit.json` | `3c130ffbedafed4436e29c4ee6eb0c49501242aac3ec708e59445df38df56430` |
| source | `extracted/diff_work/gap_pow_keccak_not_compared.json` | `26d6e80769084447ca5624297cd86a58652397ea536c418d7317c8690deb6866` |
| source | `extracted/diff_work/gap_pow_target_above_powlimit.json` | `d066cbbb951e2a029223205d3b48da16c9185a8deee0e1598c54c8b5489dc64f` |
| source | `extracted/diff_work/gap_res_sres_ignored.json` | `99ecd2dc4f9ae50dddf1b9e3394b13d78c3914635114b24c4f2a525eb38f72c4` |
| source | `extracted/diff_work/gap_utxo_dup_in.json` | `33f7dddfc5be1fde81452c658732c32dea4b5fa705d5cb275bfd47e7184cb24a` |
| source | `extracted/diff_work/gap_utxo_empty_in.json` | `e376629c3c87b127f5ef83d4ee072e0bda93c5f02ef19fd49a74d1799acc3e1b` |
| source | `extracted/diff_work/gap_utxo_empty_out.json` | `49da789d17b133f47726aab31ef95e8c1cfb21ee2280b0d6429d44404538e39e` |
| source | `extracted/diff_work/gap_utxo_model_valid_not_connectblock.json` | `ecfda1d7da61f6c20ddfdbfca4f77058d98e43930c68a7f4b92301184c5e40f7` |
| source | `extracted/diff_work/gap_utxo_neg_value.json` | `2a64d217c4f4bde777375bc9931f0185b79bc362e36ca463770ca2deea06e6ec` |
| source | `extracted/diff_work/gap_utxo_self_spend.json` | `551cb0428b2f9b6e6bf2df0d76df3e4fc28eece7ad21c10c65a0e37d9a595f0f` |
| source | `extracted/diff_work/gap_utxo_sum_overflow.json` | `84ad68dc98508b027669c8677903fbeb448197c1dd60687ddd11bb4cc7c988bd` |
| source | `kernel_binary/test_vectors/asert_invalid.json` | `c50bfd866cdec29eed786e439410ab6138cf42aad37366cfba2a487b31822ba0` |
| source | `kernel_binary/test_vectors/asert_negative_time.json` | `92410470b38997aa58591c70652cc437f0756872918738f9bebe30eb22c3c1f9` |
| source | `kernel_binary/test_vectors/oru_conservation.json` | `9d43b1aee9c19618a46fa5007ee5719862c898e8d6fdc7a235192543e1ad0ee4` |
| source | `kernel_binary/test_vectors/oru_invalid.json` | `c4a6459d8dc225ec79c56a079bbc4c02f1c908e90bf48a2b231eb9c4eaed0e9d` |
| source | `kernel_binary/test_vectors/pow_invalid.json` | `15f04f893d2299414c04af7118a46d3dfe9061d50e948ae6dd13347eee0c0b22` |
| source | `kernel_binary/test_vectors/pow_target.json` | `72895cd49e717f2f05b59c6d14aae74cace6e1e34a01da3b6ce37136d3d3424d` |
| source | `kernel_binary/test_vectors/res_invalid.json` | `a1a0a638971bed796444d148ed0281163e58d0bc8ebf4fcd301ee7011368ce89` |
| source | `kernel_binary/test_vectors/res_schedule.json` | `7b0a4faf53a9a75645efe9c8de8d21ab54002bca673ff057b18426f1c1c67e22` |
| source | `kernel_binary/test_vectors/script_classification.json` | `1a91fb2829dc77730305f2d70b9685dd12fc2a4420487dbeca155e97c8cf4838` |
| source | `kernel_binary/test_vectors/utxo_invalid.json` | `8242418d8da90bd5cbece54e1387112d5842e689020d1772a7086d46e4f29f64` |
| source | `kernel_binary/test_vectors/utxo_over_max.json` | `0ec6605a3a1d79e41e5f35e2f75d430370dfe5e5da4c0918d5a7dcd3ac6a875e` |
| source | `kernel_binary/test_vectors/utxo_valid.json` | `3902b2d9e8546bc998fdc3e1a0d0982021eafabc0e4cfcae06c8002ade0fdcde` |
| source | `kernel_binary/vectors/invalid/INVALID-001.json` | `989add52993fad3b4e237ba4a919e11779385a703776cf734773c837ea0f2bdb` |
| source | `kernel_binary/vectors/invalid/INVALID-002.json` | `2a0715a35bc68a90022ad5424fe8fc2aabcc6734316ff428c9dc6f69ad793b89` |
| source | `kernel_binary/vectors/invalid/INVALID-003.json` | `44d4f3974cf1f37e0e1187e6bef36817458339395d172df4af7fd17612b2bdf2` |
| source | `kernel_binary/vectors/invalid/INVALID-004.json` | `6027c9eadd2e0e6d6110bb4214e7861e03e66ba04b72344a720ae30c62d58d60` |
| source | `kernel_binary/vectors/invalid/INVALID-005.json` | `9c8945b3f9ea532a0b01ecf8ccc81164bab9c9aaab36062379e22863c83c0213` |
| source | `kernel_binary_vectors/V1.json` | `c6a5969f1454ccc05d07529fea39c274488e5c7be6bdf50733463cfb04287e2d` |
| source | `kernel_binary_vectors/V10.json` | `e7937fa650c81fef57b5f39629c00bf65f4c35d01fdeb6c91b6c78864cf39e13` |
| source | `kernel_binary_vectors/V2.json` | `3e0feb1d85b25bdd53569cfd5bcf805a8fdaf71e35347076f289ad70fc113439` |
| source | `kernel_binary_vectors/V3.json` | `5dee9c7526af55bdf74a4edc0752bb3a8981a41014f566275a99ace1c7ffbce6` |
| source | `kernel_binary_vectors/V4.json` | `f92cec2293bab6766ce8605c0c111ad70022fe4fb07e56cd8d4fa17324bf37b0` |
| source | `kernel_binary_vectors/V5.json` | `aef827f71503f1fedce40264e4cdcabede619a9e4a9e009c423f0d8662467a9b` |
| source | `kernel_binary_vectors/V6.json` | `dd28a34fb9c35229a6c4ff02a5aa6c6b82fff6134240535270e53b49e3a287ec` |
| source | `kernel_binary_vectors/V7.json` | `30b3fabfba2b7b95392688f4f78f927d92b808503f17aa6ca77b1c9787eca10e` |
| source | `kernel_binary_vectors/V8.json` | `67a419e2ac78c966303c1e7e2773f13523409f97806bb8662a01a2f2560a8108` |
| source | `kernel_binary_vectors/V9.json` | `579d30d410c97a42534d6ae5ee88602c31ed15cfccb8bc4f6ae75c7d5f6b3991` |
| source | `release/inventory_metadata.json` | `ea537a652b500d348845fc4883b139cefcc3e63d589d19d53b811137104cb4dd` |

## Counts

- all `.v` sources listed: True
- all proof markdown listed: True
- all scripts listed: True

