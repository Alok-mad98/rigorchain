(** Coq models of the V1–V6 negative golden vectors.

    Each executable validator must reject the corresponding invalid
    case. [force_accept_invalid] is the M36 hook.

    NOT CLAIMED: binary semantic equivalence, C++ ConnectBlock.
*)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
Import ListNotations.
From RigorKernel.proofs.UTXO_Soundness Require Import UTXOSoundness.
From RigorKernel.proofs.ORU_Conservation Require Import ORUConservation.
From RigorKernel.proofs.RES_Issuance Require Import RESIssuance.
From RigorKernel Require Import Supply.ORUModel.
From RigorKernel Require Import Script.ScriptTypes.
From RigorKernel Require Import Kernel.UTXOValidator.
From RigorKernel Require Import Kernel.ORUValidator.
From RigorKernel Require Import Kernel.RESValidator.
From RigorKernel Require Import Kernel.ScriptValidator.
From RigorKernel Require Import Kernel.PowValidator.
From RigorKernel Require Import Kernel.ASERTValidator.

Local Open Scope Z_scope.

(** M36 sets this to [true] (force-accept invalid vectors). *)
Definition force_accept_invalid : bool := false.

Definition apply_vector_check (b : bool) : bool :=
  if force_accept_invalid then true else b.

Definition v1_invalid_utxo : tx :=
  {| tx_id := [1%nat];
     tx_in := [];
     tx_out := [{| txo_value := 1; txo_spk := [] |}];
     tx_coinbase := false |}.

Definition v2_invalid_oru : kernel_tx :=
  {| kt_dom := DomORU;
     kt_oru_in := 7;
     kt_oru_out := 10;
     kt_res_in := 0;
     kt_res_out := 0;
     kt_old_supply := 100;
     kt_subsidy := 50 |}.

Definition v3_invalid_res : res_tx :=
  {| rt_coinbase := true;
     rt_mint_ok := true;
     rt_mint_amount := 1;
     rt_height := 0;
     rt_burn_oru := 0;
     rt_dom := DomORU;
     rt_res_in := 0;
     rt_res_out := 0;
     rt_oru_in := 0 |}.

Definition v4_invalid_script : ScriptState :=
  {| ss_script := [];
     ss_expected := T_WITNESS_V0 |}.

Definition v5_invalid_pow : PowHeader :=
  {| pow_digest_z := 1;
     pow_target := 100;
     pow_compact_t := -1 |}.

Definition v6_invalid_asert : ASERTInput :=
  {| asert_te := -1000000;
     asert_tau := 43200;
     asert_expected_q := 0 |}.

Theorem golden_negative_vectors_rejected :
  apply_vector_check (check_utxo v1_invalid_utxo) = false /\
  apply_vector_check (check_oru v2_invalid_oru) = false /\
  apply_vector_check (check_res v3_invalid_res) = false /\
  apply_vector_check (check_script v4_invalid_script) = false /\
  apply_vector_check (check_pow v5_invalid_pow) = false /\
  apply_vector_check (check_asert v6_invalid_asert) = false.
Proof.
  unfold apply_vector_check, force_accept_invalid.
  split; [vm_compute; reflexivity|].
  split; [vm_compute; reflexivity|].
  split; [vm_compute; reflexivity|].
  split; [vm_compute; reflexivity|].
  split; [vm_compute; reflexivity|].
  vm_compute; reflexivity.
Qed.
