# Differential Verification Report

Extracted Coq checker (executable specification) vs the shadow of
the *comparable fragment* (see `release/RefinementMapping.md` for
production-derived vs model-mirrored columns).

**Not a production refinement proof.**

`MATCH` means the two *implementations agreed*. It is **not** an
independent correctness assertion and **not** a C++ refinement.
When `expect` is `VALID` or `INVALID`, that absolute verdict is
checked separately (`EXPECT_HIT` / `EXPECT_MISS`). `AUTO` or a
missing `expect` is agreement-only.

`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`
`EXTRACTION_FORMAL_PROOF=source_identity_only`
`EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE`

Crypto assumptions: `Kernel/CryptoAssumptions.v`.
Mapping: `release/RefinementMapping.md`.

Vectors: 142  Matches: 142  Diffs: 0  expect_checked: 24  expect_miss: 0

| Vector | Domain | Model | Shadow | Result | expect |
|---|---|---|---|---|---|
| `utxo_valid.json` | utxo | VALID | VALID | MATCH | EXPECT_HIT |
| `oru_conservation.json` | oru | VALID | VALID | MATCH | EXPECT_HIT |
| `res_schedule.json` | res | VALID | VALID | MATCH | EXPECT_HIT |
| `script_classification.json` | script | VALID | VALID | MATCH | EXPECT_HIT |
| `pow_target.json` | pow | VALID | VALID | MATCH | EXPECT_HIT |
| `asert_negative_time.json` | asert | VALID | VALID | MATCH | EXPECT_HIT |
| `INVALID-001.json` | utxo | INVALID | INVALID | MATCH | EXPECT_HIT |
| `INVALID-002.json` | oru | INVALID | INVALID | MATCH | EXPECT_HIT |
| `INVALID-003.json` | res | INVALID | INVALID | MATCH | EXPECT_HIT |
| `INVALID-004.json` | asert | INVALID | INVALID | MATCH | EXPECT_HIT |
| `INVALID-005.json` | pow | INVALID | INVALID | MATCH | EXPECT_HIT |
| `extra_utxo_max_money.json` | utxo | VALID | VALID | MATCH | EXPECT_HIT |
| `extra_utxo_zero.json` | utxo | VALID | VALID | MATCH | EXPECT_HIT |
| `extra_utxo_coinbase.json` | utxo | INVALID | INVALID | MATCH | EXPECT_HIT |
| `extra_oru_equal.json` | oru | VALID | VALID | MATCH | EXPECT_HIT |
| `extra_oru_res_leak.json` | oru | INVALID | INVALID | MATCH | EXPECT_HIT |
| `extra_res_exact_w0.json` | res | VALID | VALID | MATCH | EXPECT_HIT |
| `extra_res_neg_height.json` | res | INVALID | INVALID | MATCH | EXPECT_HIT |
| `extra_pow_equal.json` | pow | VALID | VALID | MATCH | EXPECT_HIT |
| `extra_pow_zero_target.json` | pow | INVALID | INVALID | MATCH | EXPECT_HIT |
| `extra_asert_te_pos.json` | asert | INVALID | INVALID | MATCH | EXPECT_HIT |
| `extra_asert_te_neg1.json` | asert | VALID | VALID | MATCH | EXPECT_HIT |
| `extra_script_empty.json` | script | VALID | VALID | MATCH | EXPECT_HIT |
| `extra_script_wrong.json` | script | INVALID | INVALID | MATCH | EXPECT_HIT |
| `gap_utxo_empty_in.json` | utxo | INVALID | INVALID | MATCH |  |
| `gap_utxo_empty_out.json` | utxo | INVALID | INVALID | MATCH |  |
| `gap_utxo_dup_in.json` | utxo | INVALID | INVALID | MATCH |  |
| `gap_utxo_self_spend.json` | utxo | INVALID | INVALID | MATCH |  |
| `gap_utxo_sum_overflow.json` | utxo | INVALID | INVALID | MATCH |  |
| `gap_utxo_neg_value.json` | utxo | INVALID | INVALID | MATCH |  |
| `gap_utxo_model_valid_not_connectblock.json` | utxo | VALID | VALID | MATCH |  |
| `extra_oru_domres_ok.json` | oru | VALID | VALID | MATCH |  |
| `extra_oru_domres_zero_in.json` | oru | INVALID | INVALID | MATCH |  |
| `extra_oru_domres_inflate.json` | oru | INVALID | INVALID | MATCH |  |
| `extra_oru_neg_leg.json` | oru | INVALID | INVALID | MATCH |  |
| `extra_res_epoch1_exact.json` | res | VALID | VALID | MATCH |  |
| `extra_res_epoch1_plus.json` | res | INVALID | INVALID | MATCH |  |
| `extra_res_no_draw.json` | res | VALID | VALID | MATCH |  |
| `extra_res_no_mint_shape.json` | res | INVALID | INVALID | MATCH |  |
| `extra_res_coinbase.json` | res | INVALID | INVALID | MATCH |  |
| `extra_res_epoch251.json` | res | VALID | VALID | MATCH |  |
| `gap_res_sres_ignored.json` | res | VALID | VALID | MATCH |  |
| `extra_script_mint.json` | script | VALID | VALID | MATCH |  |
| `extra_script_converge.json` | script | VALID | VALID | MATCH |  |
| `extra_script_null_data.json` | script | VALID | VALID | MATCH |  |
| `extra_script_v0_wrong_len.json` | script | VALID | VALID | MATCH |  |
| `extra_script_v1_native.json` | script | VALID | VALID | MATCH |  |
| `extra_script_v1_cvg2.json` | script | VALID | VALID | MATCH |  |
| `extra_script_witness_no_fallthrough.json` | script | INVALID | INVALID | MATCH |  |
| `extra_script_opreturn_not_pushonly.json` | script | VALID | VALID | MATCH |  |
| `extra_pow_above_target.json` | pow | INVALID | INVALID | MATCH |  |
| `extra_pow_compact_zero.json` | pow | VALID | VALID | MATCH |  |
| `extra_pow_neg_target.json` | pow | INVALID | INVALID | MATCH |  |
| `gap_pow_keccak_not_compared.json` | pow | VALID | VALID | MATCH |  |
| `extra_asert_quot_tau.json` | asert | VALID | VALID | MATCH |  |
| `extra_asert_quot_mismatch.json` | asert | INVALID | INVALID | MATCH |  |
| `extra_asert_tau1.json` | asert | VALID | VALID | MATCH |  |
| `gap_asert_nbits_not_compared.json` | asert | VALID | VALID | MATCH |  |
| `gap_pow_target_above_powlimit.json` | pow | VALID | VALID | MATCH |  |
| `gap_pow_compact_signbit.json` | pow | VALID | VALID | MATCH |  |
| `gap_pow_compact_overflow.json` | pow | VALID | VALID | MATCH |  |
| `gap_asert_exp_q_over_clamp.json` | asert | VALID | VALID | MATCH |  |
| `extra_rand_utxo_0.json` | utxo | VALID | VALID | MATCH |  |
| `extra_rand_oru_0.json` | oru | VALID | VALID | MATCH |  |
| `extra_rand_utxo_1.json` | utxo | VALID | VALID | MATCH |  |
| `extra_rand_oru_1.json` | oru | VALID | VALID | MATCH |  |
| `extra_rand_utxo_2.json` | utxo | VALID | VALID | MATCH |  |
| `extra_rand_oru_2.json` | oru | INVALID | INVALID | MATCH |  |
| `extra_rand_utxo_3.json` | utxo | VALID | VALID | MATCH |  |
| `extra_rand_oru_3.json` | oru | VALID | VALID | MATCH |  |
| `extra_rand_utxo_4.json` | utxo | VALID | VALID | MATCH |  |
| `extra_rand_oru_4.json` | oru | VALID | VALID | MATCH |  |
| `extra_rand_utxo_5.json` | utxo | VALID | VALID | MATCH |  |
| `extra_rand_oru_5.json` | oru | INVALID | INVALID | MATCH |  |
| `extra_rand_utxo_6.json` | utxo | VALID | VALID | MATCH |  |
| `extra_rand_oru_6.json` | oru | INVALID | INVALID | MATCH |  |
| `extra_rand_utxo_7.json` | utxo | VALID | VALID | MATCH |  |
| `extra_rand_oru_7.json` | oru | INVALID | INVALID | MATCH |  |
| `extra_rand_utxo_8.json` | utxo | VALID | VALID | MATCH |  |
| `extra_rand_oru_8.json` | oru | INVALID | INVALID | MATCH |  |
| `extra_rand_utxo_9.json` | utxo | VALID | VALID | MATCH |  |
| `extra_rand_oru_9.json` | oru | VALID | VALID | MATCH |  |
| `extra_rand_utxo_10.json` | utxo | VALID | VALID | MATCH |  |
| `extra_rand_oru_10.json` | oru | VALID | VALID | MATCH |  |
| `extra_rand_utxo_11.json` | utxo | VALID | VALID | MATCH |  |
| `extra_rand_oru_11.json` | oru | INVALID | INVALID | MATCH |  |
| `extra_rand_utxo_12.json` | utxo | VALID | VALID | MATCH |  |
| `extra_rand_oru_12.json` | oru | VALID | VALID | MATCH |  |
| `extra_rand_utxo_13.json` | utxo | VALID | VALID | MATCH |  |
| `extra_rand_oru_13.json` | oru | VALID | VALID | MATCH |  |
| `extra_rand_utxo_14.json` | utxo | VALID | VALID | MATCH |  |
| `extra_rand_oru_14.json` | oru | VALID | VALID | MATCH |  |
| `extra_rand_utxo_15.json` | utxo | VALID | VALID | MATCH |  |
| `extra_rand_oru_15.json` | oru | VALID | VALID | MATCH |  |
| `extra_rand_utxo_16.json` | utxo | VALID | VALID | MATCH |  |
| `extra_rand_oru_16.json` | oru | INVALID | INVALID | MATCH |  |
| `extra_rand_utxo_17.json` | utxo | VALID | VALID | MATCH |  |
| `extra_rand_oru_17.json` | oru | INVALID | INVALID | MATCH |  |
| `extra_rand_utxo_18.json` | utxo | VALID | VALID | MATCH |  |
| `extra_rand_oru_18.json` | oru | INVALID | INVALID | MATCH |  |
| `extra_rand_utxo_19.json` | utxo | VALID | VALID | MATCH |  |
| `extra_rand_oru_19.json` | oru | VALID | VALID | MATCH |  |
| `extra_rand_res_0.json` | res | INVALID | INVALID | MATCH |  |
| `extra_rand_asert_0.json` | asert | INVALID | INVALID | MATCH |  |
| `extra_rand_pow_0.json` | pow | VALID | VALID | MATCH |  |
| `extra_rand_script_0.json` | script | VALID | VALID | MATCH |  |
| `extra_rand_res_1.json` | res | INVALID | INVALID | MATCH |  |
| `extra_rand_asert_1.json` | asert | VALID | VALID | MATCH |  |
| `extra_rand_pow_1.json` | pow | VALID | VALID | MATCH |  |
| `extra_rand_script_1.json` | script | VALID | VALID | MATCH |  |
| `extra_rand_res_2.json` | res | INVALID | INVALID | MATCH |  |
| `extra_rand_asert_2.json` | asert | VALID | VALID | MATCH |  |
| `extra_rand_pow_2.json` | pow | VALID | VALID | MATCH |  |
| `extra_rand_script_2.json` | script | VALID | VALID | MATCH |  |
| `extra_rand_res_3.json` | res | VALID | VALID | MATCH |  |
| `extra_rand_asert_3.json` | asert | VALID | VALID | MATCH |  |
| `extra_rand_pow_3.json` | pow | VALID | VALID | MATCH |  |
| `extra_rand_script_3.json` | script | VALID | VALID | MATCH |  |
| `extra_rand_res_4.json` | res | VALID | VALID | MATCH |  |
| `extra_rand_asert_4.json` | asert | VALID | VALID | MATCH |  |
| `extra_rand_pow_4.json` | pow | VALID | VALID | MATCH |  |
| `extra_rand_script_4.json` | script | VALID | VALID | MATCH |  |
| `extra_rand_res_5.json` | res | INVALID | INVALID | MATCH |  |
| `extra_rand_asert_5.json` | asert | INVALID | INVALID | MATCH |  |
| `extra_rand_pow_5.json` | pow | VALID | VALID | MATCH |  |
| `extra_rand_script_5.json` | script | VALID | VALID | MATCH |  |
| `extra_rand_res_6.json` | res | INVALID | INVALID | MATCH |  |
| `extra_rand_asert_6.json` | asert | VALID | VALID | MATCH |  |
| `extra_rand_pow_6.json` | pow | VALID | VALID | MATCH |  |
| `extra_rand_script_6.json` | script | VALID | VALID | MATCH |  |
| `extra_rand_res_7.json` | res | INVALID | INVALID | MATCH |  |
| `extra_rand_asert_7.json` | asert | VALID | VALID | MATCH |  |
| `extra_rand_pow_7.json` | pow | VALID | VALID | MATCH |  |
| `extra_rand_script_7.json` | script | VALID | VALID | MATCH |  |
| `extra_rand_res_8.json` | res | INVALID | INVALID | MATCH |  |
| `extra_rand_asert_8.json` | asert | VALID | VALID | MATCH |  |
| `extra_rand_pow_8.json` | pow | VALID | VALID | MATCH |  |
| `extra_rand_script_8.json` | script | VALID | VALID | MATCH |  |
| `extra_rand_res_9.json` | res | INVALID | INVALID | MATCH |  |
| `extra_rand_asert_9.json` | asert | INVALID | INVALID | MATCH |  |
| `extra_rand_pow_9.json` | pow | VALID | VALID | MATCH |  |
| `extra_rand_script_9.json` | script | VALID | VALID | MATCH |  |

## Structured coverage of mapped gaps (fragment only)

These vectors exercise the *comparable fragment* around points
that `release/RefinementMapping.md` marks as **not refined**:
ConnectBlock extras, running `S_RES`, classifier ladder, keccak
PoW hash, full ASERT nBits, `powLimit`, SetCompact sign/overflow,
unclamped `exp_q_raw` vs production `±ASERT_EXP_CLAMP`.
A `MATCH` here is still fragment agreement only.

Gap-tagged vectors: `gap_utxo_empty_in.json`, `gap_utxo_empty_out.json`, `gap_utxo_dup_in.json`, `gap_utxo_self_spend.json`, `gap_utxo_sum_overflow.json`, `gap_utxo_neg_value.json`, `gap_utxo_model_valid_not_connectblock.json`, `gap_res_sres_ignored.json`, `gap_pow_keccak_not_compared.json`, `gap_asert_nbits_not_compared.json`, `gap_pow_target_above_powlimit.json`, `gap_pow_compact_signbit.json`, `gap_pow_compact_overflow.json`, `gap_asert_exp_q_over_clamp.json`

## Known gaps (not compared)

- Production `ConnectBlock` prevouts / scripts / signatures / locktime.
- Production `CheckProofOfWork` keccak / `GetRigorPowHash`.
- Production `powLimit` upper bound and `SetCompact` fNegative / fOverflow.
- Production `GetNextWorkASERT` clamps `exp_q` to `±ASERT_EXP_CLAMP`, then
  256-bit scale / clamp / compact nBits. The model compares unclamped `exp_q_raw`.
- Running `S_RES` scalar (C-3): not a model reject and not a C++ reject.
- Classifier ≡ `rigor_classify.cpp` (C-7) is open.

`MATCH` means the two implementations agreed. It is **not** a C++
refinement and **not** an independent correctness assertion.

DIFF_VERIFY=PASS
