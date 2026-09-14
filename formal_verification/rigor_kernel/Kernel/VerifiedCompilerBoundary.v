(** RigorKernel.Kernel.VerifiedCompilerBoundary — compiler-path flags.

    Scope: Coq consensus kernel only.

    Documents the verified-compiler *intent* and the current TCB.
    CompCert / CakeML / CertiCoq are not in this cone's checking TCB.
    Default executable path remains Coq → OCaml (unverified ocamlopt).

    Concrete INVALID-001..005 are rejected by the Coq checkers
    ([vm_compute]). That is a model-side fact. It is not a CompCert
    refinement and not binary semantic equivalence.

    NOT CLAIMED:
      CompCert correctness of this build
      CakeML backend in use
      Coq Extraction Language C compatibility
      Full extraction refinement
      Binary semantic equivalence
      Production-code refinement
*)
From Coq Require Import List.
Import ListNotations.
From Coq Require Import ZArith.ZArith.
From RigorKernel Require Import Audit.KernelNonClaims.
From RigorKernel Require Import Supply.ORUModel.
From RigorKernel.proofs.UTXO_Soundness Require Import UTXOSoundness.
From RigorKernel.proofs.ORU_Conservation Require Import ORUConservation.
From RigorKernel.proofs.RES_Issuance Require Import RESIssuance.
From RigorKernel Require Import Kernel.UTXOValidator.
From RigorKernel Require Import Kernel.ORUValidator.
From RigorKernel Require Import Kernel.RESValidator.
From RigorKernel Require Import Kernel.PowValidator.
From RigorKernel Require Import Kernel.ASERTValidator.

Local Open Scope Z_scope.

(** * Current TCB flags *)

Definition verified_compiler_in_current_tcb : bool := false.
Definition verified_compiler_path_documented : bool := true.
Definition verified_compiler_forced_switch : bool := false.
Definition compcert_compiles_coq_extract : bool := false.
Definition cakeml_backend_in_use : bool := false.

Theorem we_do_not_claim_verified_compiler_in_tcb :
  verified_compiler_in_current_tcb = false
  /\ compiler_correctness = false
  /\ binary_semantic_equivalence = false.
Proof. repeat split; reflexivity. Qed.

Theorem verified_compiler_path_is_documented :
  verified_compiler_path_documented = true
  /\ verified_compiler_forced_switch = false.
Proof. split; reflexivity. Qed.

Theorem we_do_not_claim_compcert_or_cakeml_backend :
  compcert_compiles_coq_extract = false
  /\ cakeml_backend_in_use = false.
Proof. split; reflexivity. Qed.

(** * Concrete negative vectors — Coq checkers reject them.

    These are the same INVALID-001..005 records as the JSON harness.
    [vm_compute] is a Coq evaluation, not a compiler proof. *)

Definition invalid001_tx : tx :=
  {| tx_id := [1%nat];
     tx_in := [{| op_txid := [2%nat]; op_index := 0%nat |}];
     tx_out := [{| txo_value := 1000000000000000001; txo_spk := nil |}];
     tx_coinbase := false |}.

Definition invalid002_tx : kernel_tx :=
  {| kt_dom := DomORU;
     kt_oru_in := 7;
     kt_oru_out := 10;
     kt_res_in := 0;
     kt_res_out := 0;
     kt_old_supply := 100;
     kt_subsidy := 50 |}.

Definition invalid003_tx : res_tx :=
  {| rt_coinbase := true;
     rt_mint_ok := true;
     rt_mint_amount := 1;
     rt_height := 0;
     rt_burn_oru := 0;
     rt_dom := DomORU;
     rt_res_in := 0;
     rt_res_out := 0;
     rt_oru_in := 0 |}.

Definition invalid004_in : ASERTInput :=
  {| asert_te := -1000000;
     asert_tau := 43200;
     asert_expected_q := 0 |}.

Definition invalid005_in : PowHeader :=
  {| pow_digest_z := 1;
     pow_target := 100;
     pow_compact_t := -1 |}.

Theorem invalid001_rejected_by_coq : check_utxo invalid001_tx = false.
Proof. vm_compute. reflexivity. Qed.

Theorem invalid002_rejected_by_coq : check_oru invalid002_tx = false.
Proof. vm_compute. reflexivity. Qed.

Theorem invalid003_rejected_by_coq : check_res invalid003_tx = false.
Proof. vm_compute. reflexivity. Qed.

Theorem invalid004_rejected_by_coq : check_asert invalid004_in = false.
Proof. vm_compute. reflexivity. Qed.

Theorem invalid005_rejected_by_coq : check_pow invalid005_in = false.
Proof. vm_compute. reflexivity. Qed.

Theorem negative_vectors_rejected_in_coq :
  check_utxo invalid001_tx = false
  /\ check_oru invalid002_tx = false
  /\ check_res invalid003_tx = false
  /\ check_asert invalid004_in = false
  /\ check_pow invalid005_in = false.
Proof.
  split; [apply invalid001_rejected_by_coq|].
  split; [apply invalid002_rejected_by_coq|].
  split; [apply invalid003_rejected_by_coq|].
  split; [apply invalid004_rejected_by_coq|].
  apply invalid005_rejected_by_coq.
Qed.
