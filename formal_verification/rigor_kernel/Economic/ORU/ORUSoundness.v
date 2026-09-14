(** Independent ORU conservation. Category A.

    ORU conservation and RES issuance are separately modeled and verified.

    NOT CLAIMED: full monetary system, C++ refinement, node.
*)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorKernel Require Import Economic.ORU.ORUDomain.
From RigorKernel Require Import Economic.ORU.ORUTransition.

Local Open Scope Z_scope.

Theorem oru_domain_conservation :
  forall (pre : ORUState) (f : ORUValueFlow),
    let post := oru_apply pre f in
    old_value pre + incoming_value f
      = outgoing_value f post + burned_value f.
Proof.
  intros pre f. unfold old_value, incoming_value, outgoing_value, burned_value.
  unfold oru_apply. simpl. lia.
Qed.

Theorem oru_fees_are_transfer_only :
  forall (pre : ORUState) (f : ORUValueFlow),
    oru_apply pre f
      = oru_apply pre {| ovf_incoming := ovf_incoming f;
                         ovf_outgoing := ovf_outgoing f;
                         ovf_fees := 0;
                         ovf_subsidy := ovf_subsidy f;
                         ovf_burn := ovf_burn f |}.
Proof.
  intros pre f. unfold oru_apply. reflexivity.
Qed.

(** Task name. Distinct from [Kernel.EconomicInvariant.Economic]. *)
Theorem fees_are_transfer_only :
  forall (pre : ORUState) (f : ORUValueFlow),
    oru_apply pre f
      = oru_apply pre {| ovf_incoming := ovf_incoming f;
                         ovf_outgoing := ovf_outgoing f;
                         ovf_fees := 0;
                         ovf_subsidy := ovf_subsidy f;
                         ovf_burn := ovf_burn f |}.
Proof.
  exact oru_fees_are_transfer_only.
Qed.

Theorem burn_is_monotonic_supply_decrease :
  forall (pre : ORUState) (f : ORUValueFlow),
    valid_oru_flow f ->
    oru_stock (oru_apply pre f)
      <= oru_stock (oru_apply pre {| ovf_incoming := ovf_incoming f;
                                     ovf_outgoing := ovf_outgoing f;
                                     ovf_fees := ovf_fees f;
                                     ovf_subsidy := ovf_subsidy f;
                                     ovf_burn := 0 |}).
Proof.
  intros pre f [Hin [Hout [Hfee [Hsub Hburn]]]].
  unfold oru_apply. simpl. lia.
Qed.

Theorem subsidy_is_only_creation_source :
  forall (pre : ORUState) (f : ORUValueFlow),
    valid_oru_flow f ->
    ovf_subsidy f = 0 ->
    oru_stock (oru_apply pre f)
      <= oru_stock pre + ovf_incoming f - ovf_outgoing f.
Proof.
  intros pre f [Hin [Hout [Hfee [Hsub Hburn]]]] Hz.
  unfold oru_apply. simpl. rewrite Hz. lia.
Qed.
