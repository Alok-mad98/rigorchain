(** RigorKernel.Kernel.ExtractionBoundary — extraction source identity.

    Scope: Coq consensus kernel only.

    Proved here (class B):
      The six functions named as extraction targets are definitionally
      the Coq validators. validate_kernel dispatches to those same
      functions. Core records are determined by their fields.
      Boolean checkers reflect the Coq predicates (one direction
      already in the validator modules).

    Not proved (class C / non-claim):
      OCaml compiler correctness
      Native integer / ExtrOcamlZInt semantics
      Binary semantic equivalence
      Full extraction refinement (CompCert / CertiCoq)

    NOT CLAIMED:
      C++ implementation equivalence
      Compiler correctness
      Full OCaml semantic preservation
*)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From RigorKernel.proofs.UTXO_Soundness Require Import UTXOSoundness.
From RigorKernel.proofs.ORU_Conservation Require Import ORUConservation.
From RigorKernel.proofs.RES_Issuance Require Import RESIssuance.
From RigorKernel Require Import Supply.SupplySchedule.
From RigorKernel Require Import Pow.ASERTModel.
From RigorKernel Require Import Script.ScriptClassifier.
From RigorKernel Require Import Kernel.UTXOValidator.
From RigorKernel Require Import Kernel.ORUValidator.
From RigorKernel Require Import Kernel.RESValidator.
From RigorKernel Require Import Kernel.ScriptValidator.
From RigorKernel Require Import Kernel.PowValidator.
From RigorKernel Require Import Kernel.ASERTValidator.
From RigorKernel Require Import Kernel.KernelSoundness.

Local Open Scope Z_scope.

(** Compiler / runtime remain outside the proved Coq cone. *)
Definition extraction_in_proof_scope : bool := false.

Definition extraction_boundary : Prop :=
  extraction_in_proof_scope = false.

(** The Coq terms we extract *are* the proved validators. *)
Definition extraction_source_identity : bool := true.

(** Full OCaml / compiler refinement is still not claimed. *)
Definition extraction_full_equivalence : bool := false.

Theorem extraction_boundary_declared :
  extraction_boundary.
Proof.
  unfold extraction_boundary, extraction_in_proof_scope. reflexivity.
Qed.

Theorem we_do_not_claim_extraction_in_proof_scope :
  extraction_in_proof_scope = false.
Proof. reflexivity. Qed.

Theorem extraction_source_identity_declared :
  extraction_source_identity = true.
Proof. reflexivity. Qed.

Theorem we_do_not_claim_extraction_full_equivalence :
  extraction_full_equivalence = false.
Proof. reflexivity. Qed.

(** * Extraction targets = Coq validators (definitional) *)

Definition extracted_check_utxo := UTXOValidator.check_utxo.
Definition extracted_check_oru := ORUValidator.check_oru.
Definition extracted_check_res := RESValidator.check_res.
Definition extracted_check_script := ScriptValidator.check_script.
Definition extracted_check_pow := PowValidator.check_pow.
Definition extracted_check_asert := ASERTValidator.check_asert.

Theorem utxo_extraction_source_eq :
  extracted_check_utxo = UTXOValidator.check_utxo.
Proof. reflexivity. Qed.

Theorem oru_extraction_source_eq :
  extracted_check_oru = ORUValidator.check_oru.
Proof. reflexivity. Qed.

Theorem res_extraction_source_eq :
  extracted_check_res = RESValidator.check_res.
Proof. reflexivity. Qed.

Theorem script_extraction_source_eq :
  extracted_check_script = ScriptValidator.check_script.
Proof. reflexivity. Qed.

Theorem pow_extraction_source_eq :
  extracted_check_pow = PowValidator.check_pow.
Proof. reflexivity. Qed.

Theorem asert_extraction_source_eq :
  extracted_check_asert = ASERTValidator.check_asert.
Proof. reflexivity. Qed.

Theorem utxo_extraction_is_valid_tx_b :
  extracted_check_utxo = valid_tx_b.
Proof. reflexivity. Qed.

Theorem oru_extraction_is_valid_oru_tx_b :
  forall s, extracted_check_oru s = valid_oru_tx_b (to_oru s).
Proof. intros s. reflexivity. Qed.

(** * validate_kernel dispatches to the same six functions *)

Theorem validate_kernel_utxo :
  forall s, validate_kernel (KI_UTXO s) = extracted_check_utxo s.
Proof. intros s. reflexivity. Qed.

Theorem validate_kernel_oru :
  forall s, validate_kernel (KI_ORU s) = extracted_check_oru s.
Proof. intros s. reflexivity. Qed.

Theorem validate_kernel_res :
  forall s, validate_kernel (KI_RES s) = extracted_check_res s.
Proof. intros s. reflexivity. Qed.

Theorem validate_kernel_script :
  forall s, validate_kernel (KI_Script s) = extracted_check_script s.
Proof. intros s. reflexivity. Qed.

Theorem validate_kernel_pow :
  forall h, validate_kernel (KI_Pow h) = extracted_check_pow h.
Proof. intros h. reflexivity. Qed.

Theorem validate_kernel_asert :
  forall i, validate_kernel (KI_ASERT i) = extracted_check_asert i.
Proof. intros i. reflexivity. Qed.

(** * Checkers reflect Coq predicates (source-level equivalence) *)

Theorem utxo_extract_reflects_valid_tx :
  forall s, extracted_check_utxo s = true -> UTXOSoundness.valid_tx s.
Proof. intros s H. apply check_utxo_valid. exact H. Qed.

Theorem oru_extract_reflects_conservation :
  forall s,
    extracted_check_oru s = true ->
    new_supply s = old_supply s + subsidy s + fees s - burn s
    /\ oru_input s = oru_output s + fee s.
Proof. intros s H. apply check_oru_sound. exact H. Qed.

Theorem res_extract_reflects_issuance :
  forall s,
    extracted_check_res s = true ->
    coinbase s = false
    /\ valid_res_mint s
    /\ mint_amount s * res_destroy_pole_sq (rt_height s)
       <= Z.shiftl (burn_oru s) RES_DESTROY_SHIFT.
Proof. intros s H. apply check_res_sound. exact H. Qed.

Theorem script_extract_reflects_classify :
  forall s,
    extracted_check_script s = true ->
    classify_script (ss_script s) = ss_expected s.
Proof.
  intros s H. apply check_script_sound in H. exact (proj1 H).
Qed.

Theorem pow_extract_reflects_target :
  forall h,
    extracted_check_pow h = true ->
    0 < pow_target h /\ pow_digest_z h <= pow_target h.
Proof.
  intros h H. apply check_pow_sound in H. tauto.
Qed.

Theorem asert_extract_reflects_quot :
  forall i,
    extracted_check_asert i = true ->
    asert_te i < 0
    /\ exp_q_raw (asert_te i) (asert_tau i) = asert_expected_q i.
Proof.
  intros i H. apply check_asert_sound in H. tauto.
Qed.

(** * Core records are determined by their fields (extraction data shape) *)

Theorem utxo_state_fields_determine :
  forall s : UTXOState,
    {| tx_id := tx_id s;
       tx_in := tx_in s;
       tx_out := tx_out s;
       tx_coinbase := tx_coinbase s |} = s.
Proof. intros []. reflexivity. Qed.

Theorem oru_state_fields_determine :
  forall s : ORUState,
    {| kt_dom := kt_dom s;
       kt_oru_in := kt_oru_in s;
       kt_oru_out := kt_oru_out s;
       kt_res_in := kt_res_in s;
       kt_res_out := kt_res_out s;
       kt_old_supply := kt_old_supply s;
       kt_subsidy := kt_subsidy s |} = s.
Proof. intros []. reflexivity. Qed.

Theorem res_state_fields_determine :
  forall s : RESState,
    {| rt_coinbase := rt_coinbase s;
       rt_mint_ok := rt_mint_ok s;
       rt_mint_amount := rt_mint_amount s;
       rt_height := rt_height s;
       rt_burn_oru := rt_burn_oru s;
       rt_dom := rt_dom s;
       rt_res_in := rt_res_in s;
       rt_res_out := rt_res_out s;
       rt_oru_in := rt_oru_in s |} = s.
Proof. intros []. reflexivity. Qed.

Theorem script_state_fields_determine :
  forall s : ScriptState,
    {| ss_script := ss_script s; ss_expected := ss_expected s |} = s.
Proof. intros []. reflexivity. Qed.

Theorem pow_header_fields_determine :
  forall h : PowHeader,
    {| pow_digest_z := pow_digest_z h;
       pow_target := pow_target h;
       pow_compact_t := pow_compact_t h |} = h.
Proof. intros []. reflexivity. Qed.

Theorem asert_input_fields_determine :
  forall i : ASERTInput,
    {| asert_te := asert_te i;
       asert_tau := asert_tau i;
       asert_expected_q := asert_expected_q i |} = i.
Proof. intros []. reflexivity. Qed.

(** Bundle: all six source-identity equalities. *)
Theorem extraction_source_equivalence :
  extracted_check_utxo = UTXOValidator.check_utxo
  /\ extracted_check_oru = ORUValidator.check_oru
  /\ extracted_check_res = RESValidator.check_res
  /\ extracted_check_script = ScriptValidator.check_script
  /\ extracted_check_pow = PowValidator.check_pow
  /\ extracted_check_asert = ASERTValidator.check_asert.
Proof. repeat split; reflexivity. Qed.
