# Formal refinement progression (P0-040)

**Task:** `TASK-RIGOR-FORMAL-REFINEMENT-PROGRESSION-P0-040`  
**Date:** 2026-08-29

P0-039 models stay Qed. This step names C++ field maps and reusable parser lemmas. It is not a catalog and not a ∀ VST. Production W / T / OpuHash / RES schedule / maturity were not changed. Script VM was not restored. `REFINEMENT_PROVED` stays **false**.

```
MODEL_PROOFS_PRESERVED       = true
COQ_ADMITTED                 = 0
COQ_AXIOMS                   = 0
COQ_PARAMETERS               = 0

CLASSIFIER_REFINEMENT        = parser_lemmas_qed; finite_byte_lock; forall_cpp_unproved
CONNECTBLOCK_REFINEMENT      = cpp_slice_field_map_qed; observational_lock; vst_unproved
REORG_REFINEMENT             = disconnect_boundary_named; depth_1_W_gtW_gt100_qed; vst_unproved
SIGHASH_REFINEMENT           = sha256d_preimage_lock; ed25519_vst_unproved

FUZZ_COUNTEREXAMPLES         = 0 / 4
CONSENSUS_CPP_MODIFIED       = false
REFINEMENT_PROVED            = false
```

fuzz ≠ theorem ≠ refinement proof.

## Breakpoints

**Classifier.** Live `parse_canonical_mint` uniqueness: successful parse implies encoder bytes. `OP_1`..`OP_16` cannot be a mint prefix (`OP_PUSHBYTES_1 0x05` remains RES; `OP_5` remains INVALID). `classify` is the C++ ladder (witness → mint → converge → OP_RETURN) by definition. Finite golden vectors are unchanged.

**ConnectBlock.** `CppConnectBlockSlice` maps `GetBlockSubsidy` / `res_mint_coinbase` (`m_block_res_mint_sum`) / `block_converge_sum` / `nFeesOruResTransferBurn` onto `apply_block_supply`. Observational ORU = parent + subsidy − converge (fee burn is circulating-only). C++ `TestChain100Setup` checks that write and that ordinary test-chain blocks mint 0.

**Reorg.** `DisconnectBlock` / `InvalidateBlock` correspondence: surviving `apply_deltas` does not include the orphan. C++: orphan coinbase leaves the UTXO set; observational cache on the disconnected index is invalidated. Model depths 1 / W / >W / >100 unchanged. W does not enter maturity.

**Sighash.** `hash256_bytes = SHA256d`. Three P0-018/P0-024 preimages hash to golden digests in Coq. C++ `Hash(json_preimage) = SignatureHash = expected_hash`. No libsodium/RFC8032 VST.

**Fuzz (counterexample search only).** `rigor_scriptpubkey` OP_1..OP_16 mint prefix; `rigor_reorg` surviving mint = kept + new suffix; `rigor_supply` ledger vs circulating; `script_interpreter` SignatureHash determinism.
