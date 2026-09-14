(** RigorSurface.Observe — Phase 3 observational witnesses.

    Same Gallina, evaluated on Coq 8.15.0 and sidecar 8.19.2.
    [vm_compute] is a host evaluation, not a cross-version
    metatheorem and not CertiCoq / CompCert.

    INVALID-001..005 match [Kernel/VerifiedCompilerBoundary.v].
    Positive smokes match [kernel_binary/test_vectors/].

    NOT CLAIMED:
      compiler correctness
      binary semantic equivalence
      C++ / ConnectBlock / rigord
*)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
Import ListNotations.
From RigorSurface Require Import Supply.ORUModel.
From RigorSurface Require Import Supply.KernelTx.
From RigorSurface Require Import Supply.RESTx.
From RigorSurface Require Import Script.ScriptTypes.
From RigorSurface Require Import Kernel.TxBool.
From RigorSurface Require Import Kernel.UTXO.
From RigorSurface Require Import Kernel.ORU.
From RigorSurface Require Import Kernel.RES.
From RigorSurface Require Import Kernel.Script.
From RigorSurface Require Import Kernel.Pow.
From RigorSurface Require Import Kernel.ASERT.
From RigorSurface Require Import Kernel.Validate.

Local Open Scope Z_scope.

(** * INVALID-001..005 (cone [VerifiedCompilerBoundary]) *)

Definition invalid001_tx : UTXOState :=
  {| tx_id := [1%nat];
     tx_in := [{| op_txid := [2%nat]; op_index := 0%nat |}];
     tx_out := [{| txo_value := 1000000000000000001; txo_spk := nil |}];
     tx_coinbase := false |}.

Definition invalid002_tx : ORUState :=
  {| kt_dom := DomORU;
     kt_oru_in := 7;
     kt_oru_out := 10;
     kt_res_in := 0;
     kt_res_out := 0;
     kt_old_supply := 100;
     kt_subsidy := 50 |}.

Definition invalid003_tx : RESState :=
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

(* Surface-only negative for check_script. No INVALID-00x in the
   cone boundary covers script. Empty script is T_NONSTANDARD. *)
Definition invalid_script : ScriptState :=
  {| ss_script := [];
     ss_expected := T_WITNESS_V0 |}.

(** * Positive smokes ([kernel_binary/test_vectors/]) *)

Definition smoke_utxo : UTXOState :=
  {| tx_id := [1%nat];
     tx_in := [{| op_txid := [2%nat]; op_index := 0%nat |}];
     tx_out := [{| txo_value := 1; txo_spk := nil |}];
     tx_coinbase := false |}.

Definition smoke_oru : ORUState :=
  {| kt_dom := DomORU;
     kt_oru_in := 10;
     kt_oru_out := 7;
     kt_res_in := 0;
     kt_res_out := 0;
     kt_old_supply := 100;
     kt_subsidy := 50 |}.

Definition smoke_res : RESState :=
  {| rt_coinbase := false;
     rt_mint_ok := true;
     rt_mint_amount := 1;
     rt_height := 0;
     rt_burn_oru := 1024;
     rt_dom := DomORU;
     rt_res_in := 0;
     rt_res_out := 0;
     rt_oru_in := 0 |}.

Definition smoke_script : ScriptState :=
  {| ss_script :=
       [0; 20; 0; 0; 0; 0; 0; 0; 0; 0; 0; 0;
        0; 0; 0; 0; 0; 0; 0; 0; 0; 0]%nat;
     ss_expected := T_WITNESS_V0 |}.

Definition smoke_pow : PowHeader :=
  {| pow_digest_z := 1;
     pow_target := 100;
     pow_compact_t := 100 |}.

Definition smoke_asert : ASERTInput :=
  {| asert_te := -1;
     asert_tau := 43200;
     asert_expected_q := -1 |}.

(** * Host evaluation. Same expected bools on 8.15 and 8.19. *)

Example invalid001_obs : check_utxo invalid001_tx = false.
Proof. vm_compute. reflexivity. Qed.

Example invalid002_obs : check_oru invalid002_tx = false.
Proof. vm_compute. reflexivity. Qed.

Example invalid003_obs : check_res invalid003_tx = false.
Proof. vm_compute. reflexivity. Qed.

Example invalid004_obs : check_asert invalid004_in = false.
Proof. vm_compute. reflexivity. Qed.

Example invalid005_obs : check_pow invalid005_in = false.
Proof. vm_compute. reflexivity. Qed.

Example invalid_script_obs : check_script invalid_script = false.
Proof. vm_compute. reflexivity. Qed.

Example smoke_utxo_obs : check_utxo smoke_utxo = true.
Proof. vm_compute. reflexivity. Qed.

Example smoke_oru_obs : check_oru smoke_oru = true.
Proof. vm_compute. reflexivity. Qed.

Example smoke_res_obs : check_res smoke_res = true.
Proof. vm_compute. reflexivity. Qed.

Example smoke_script_obs : check_script smoke_script = true.
Proof. vm_compute. reflexivity. Qed.

Example smoke_pow_obs : check_pow smoke_pow = true.
Proof. vm_compute. reflexivity. Qed.

Example smoke_asert_obs : check_asert smoke_asert = true.
Proof. vm_compute. reflexivity. Qed.

Example dispatch_invalid001 :
  validate_kernel (KI_UTXO invalid001_tx) = false.
Proof. vm_compute. reflexivity. Qed.

Example dispatch_invalid002 :
  validate_kernel (KI_ORU invalid002_tx) = false.
Proof. vm_compute. reflexivity. Qed.

Example dispatch_invalid003 :
  validate_kernel (KI_RES invalid003_tx) = false.
Proof. vm_compute. reflexivity. Qed.

Example dispatch_invalid004 :
  validate_kernel (KI_ASERT invalid004_in) = false.
Proof. vm_compute. reflexivity. Qed.

Example dispatch_invalid005 :
  validate_kernel (KI_Pow invalid005_in) = false.
Proof. vm_compute. reflexivity. Qed.

Example dispatch_invalid_script :
  validate_kernel (KI_Script invalid_script) = false.
Proof. vm_compute. reflexivity. Qed.

Example dispatch_smoke_utxo :
  validate_kernel (KI_UTXO smoke_utxo) = true.
Proof. vm_compute. reflexivity. Qed.

Example dispatch_smoke_oru :
  validate_kernel (KI_ORU smoke_oru) = true.
Proof. vm_compute. reflexivity. Qed.

Example dispatch_smoke_res :
  validate_kernel (KI_RES smoke_res) = true.
Proof. vm_compute. reflexivity. Qed.

Example dispatch_smoke_script :
  validate_kernel (KI_Script smoke_script) = true.
Proof. vm_compute. reflexivity. Qed.

Example dispatch_smoke_pow :
  validate_kernel (KI_Pow smoke_pow) = true.
Proof. vm_compute. reflexivity. Qed.

Example dispatch_smoke_asert :
  validate_kernel (KI_ASERT smoke_asert) = true.
Proof. vm_compute. reflexivity. Qed.
