(** RigorKernel.Supply.FeeDomain — Phase B fee-domain extension.

    Scope: consensus kernel only.

    Extends the Phase A ORU model with an explicit fee-domain
    classification: burn is contributed only by the RES-transfer
    domain. Conservation is restated as [oru_conservation_extended].

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      Hash collision resistance
      Signature security
      Runtime rejection equivalence
*)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
Import ListNotations.
From RigorKernel Require Import Supply.ORUModel.
From RigorKernel Require Import Supply.ORUConservation.

Local Open Scope Z_scope.

(** Fee domain of a non-coinbase transaction. *)
Inductive fee_domain :=
| FeeORU
| FeeRESTransfer.

Definition fee_domain_of (d : input_domain) : fee_domain :=
  match d with
  | DomORU => FeeORU
  | DomRES => FeeRESTransfer
  end.

Definition domain_burn (fd : fee_domain) (fee : Z) : Z :=
  match fd with
  | FeeRESTransfer => burn_share fee
  | FeeORU => 0
  end.

Definition domain_claimable (fd : fee_domain) (fee : Z) : Z :=
  fee - domain_burn fd fee.

Definition tx_fee_domain (t : oru_tx) : fee_domain :=
  fee_domain_of (tx_dom t).

(** Statement: a non-zero burn implies the RES-transfer fee domain.
    Assumptions: [domain_burn fd fee <> 0].
    Consensus kernel only: yes. *)
Theorem burn_only_res_transfer_domain :
  forall (fd : fee_domain) (fee : Z),
    domain_burn fd fee <> 0 ->
    fd = FeeRESTransfer.
Proof.
  intros fd fee Hnz.
  destruct fd; [contradiction| reflexivity].
Qed.

(** Statement: ORU conservation in both writings —
      new_supply = old + subsidy + fees - burn
      new_supply = old + oru_claimable
    where oru_claimable = (fees - burn) + subsidy.
    Assumptions: none beyond the Phase A definitions.
    Consensus kernel only: yes. *)
Theorem oru_conservation_extended :
  forall old_supply subsidy fees burn : Z,
    let new_supply := ORU_supply_transition old_supply subsidy fees burn in
    new_supply = old_supply + subsidy + fees - burn /\
    new_supply = old_supply + oru_claimable fees burn subsidy.
Proof.
  intros old_supply subsidy fees burn.
  split.
  - apply oru_conservation.
  - apply oru_conservation_via_claimable.
Qed.

(** Statement: [domain_burn] agrees with Phase A [tx_burn] on the
    corresponding input domain.
    Assumptions: none.
    Consensus kernel only: yes. *)
Theorem domain_burn_agrees_tx_burn :
  forall (d : input_domain) (fee : Z),
    domain_burn (fee_domain_of d) fee = tx_burn d fee.
Proof.
  intros d fee. destruct d; reflexivity.
Qed.

(** Statement: ORU-domain fees contribute zero burn.
    Assumptions: [fd = FeeORU].
    Consensus kernel only: yes. *)
Theorem fee_oru_domain_burns_nothing :
  forall fee : Z,
    domain_burn FeeORU fee = 0.
Proof. intros fee. reflexivity. Qed.
