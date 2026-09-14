(** RigorKernel.proofs.ORU_Conservation.ORUConservation — P0-108 ORU kernel.

    Scope: consensus kernel only.

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      Hash collision resistance
      Signature security
      Runtime rejection equivalence
*)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorKernel Require Import Supply.ORUModel.
From RigorKernel Require Import Supply.ORUConservation.
From RigorKernel Require Import Supply.FeeDomain.

Local Open Scope Z_scope.

(** A kernel transaction plus the supply step it participates in. *)
Record kernel_tx := {
  kt_dom : input_domain;
  kt_oru_in : Z;
  kt_oru_out : Z;
  kt_res_in : Z;
  kt_res_out : Z;
  kt_old_supply : Z;
  kt_subsidy : Z
}.

Definition to_oru (t : kernel_tx) : oru_tx :=
  {| tx_dom := kt_dom t;
     oru_in := kt_oru_in t;
     oru_out := kt_oru_out t;
     res_in := kt_res_in t;
     res_out := kt_res_out t |}.

Definition valid_tx (t : kernel_tx) : Prop := valid_oru_tx (to_oru t).

Definition oru_input (t : kernel_tx) : Z := kt_oru_in t.
Definition oru_output (t : kernel_tx) : Z := kt_oru_out t.
Definition fee (t : kernel_tx) : Z := kt_oru_in t - kt_oru_out t.
Definition fees (t : kernel_tx) : Z := fee t.
Definition burn (t : kernel_tx) : Z := tx_burn (kt_dom t) (fee t).
Definition old_supply (t : kernel_tx) : Z := kt_old_supply t.
Definition subsidy (t : kernel_tx) : Z := kt_subsidy t.
Definition new_supply (t : kernel_tx) : Z :=
  ORU_supply_transition (old_supply t) (subsidy t) (fees t) (burn t).

(** Statement: a valid tx conserves ORU as input = output + fee.
    Assumptions: [valid_tx tx].
    Consensus kernel only: yes. *)
Theorem oru_conservation_tx :
  forall tx : kernel_tx,
    valid_tx tx ->
    oru_input tx = oru_output tx + fee tx.
Proof.
  intros tx Hv. unfold oru_input, oru_output, fee. lia.
Qed.

(** Statement: no hidden inflation — the same identity under the
    P0-108 names [oru_input] / [oru_output] / [fee].
    Assumptions: [valid_tx tx].
    Consensus kernel only: yes. *)
Theorem no_hidden_inflation :
  forall tx : kernel_tx,
    valid_tx tx ->
    oru_input tx = oru_output tx + fee tx.
Proof. apply oru_conservation_tx. Qed.

(** Statement: supply after the step equals old + subsidy + fees - burn.
    Assumptions: [valid_tx tx] (unused; the identity is definitional).
    Consensus kernel only: yes. *)
Theorem oru_conservation_extended :
  forall tx : kernel_tx,
    valid_tx tx ->
    new_supply tx =
    old_supply tx + subsidy tx + fees tx - burn tx.
Proof.
  intros tx Hv.
  unfold new_supply, old_supply, subsidy, fees, burn.
  apply oru_conservation.
Qed.

(** Statement: fee is non-negative on a valid tx.
    Assumptions: [valid_tx tx].
    Consensus kernel only: yes. *)
Theorem fee_nonneg_kernel :
  forall tx : kernel_tx,
    valid_tx tx ->
    0 <= fee tx.
Proof.
  intros tx Hv. unfold valid_tx, valid_oru_tx, to_oru, fee in *.
  simpl in Hv. lia.
Qed.

(** Statement: fee splits into claimable share + burn.
    Assumptions: none.
    Consensus kernel only: yes. *)
Theorem fee_split_kernel :
  forall tx : kernel_tx,
    fee tx = tx_claimable_share (kt_dom tx) (fee tx) + burn tx.
Proof.
  intros tx. unfold burn. apply fee_split.
Qed.

(** Statement: ORU in = ORU out + claimable + burn.
    Assumptions: [valid_tx tx].
    Consensus kernel only: yes. *)
Theorem oru_conservation_with_burn :
  forall tx : kernel_tx,
    valid_tx tx ->
    oru_input tx =
    oru_output tx + tx_claimable_share (kt_dom tx) (fee tx) + burn tx.
Proof.
  intros tx Hv.
  rewrite oru_conservation_tx by exact Hv.
  unfold tx_claimable_share, burn. lia.
Qed.

(** Statement: a valid non-coinbase kernel tx cannot create ORU.
    Assumptions: [valid_tx tx].
    Consensus kernel only: yes. *)
Theorem no_unauthorized_oru_creation_kernel :
  forall tx : kernel_tx,
    valid_tx tx ->
    oru_output tx <= oru_input tx.
Proof.
  intros tx Hv. unfold valid_tx, valid_oru_tx, to_oru, oru_output, oru_input in *.
  simpl in Hv. lia.
Qed.

(** Statement: accounting functions are deterministic.
    Assumptions: equal field values.
    Consensus kernel only: yes. *)
Theorem oru_accounting_deterministic_kernel :
  forall t1 t2 : kernel_tx,
    t1 = t2 ->
    new_supply t1 = new_supply t2 /\
    fee t1 = fee t2 /\
    burn t1 = burn t2.
Proof. intros t1 t2 Heq. subst. split; [reflexivity|]. split; reflexivity. Qed.

(** Re-export Phase A burn bounds under this module. *)
Theorem burn_share_bounds_kernel :
  forall f : Z, 0 <= burn_share f /\ burn_share f <= Z.max 0 f.
Proof. apply burn_share_bounds. Qed.

Theorem burn_only_res_transfer_domain_kernel :
  forall (fd : fee_domain) (fee0 : Z),
    domain_burn fd fee0 <> 0 ->
    fd = FeeRESTransfer.
Proof. apply burn_only_res_transfer_domain. Qed.
