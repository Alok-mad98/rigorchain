Require Import Coq.ZArith.ZArith.
Require Import Coq.Lists.List.
Import ListNotations.
Require Import Rigor.legacy_account_model.Types.
Require Import Rigor.legacy_account_model.State.
Require Import Execution.
Require Import Rigor.legacy_account_model.Selection.
Require Import ResSchedule.

(** RES cap.

    This was [21000000], a bare count of whole RES, while [State.RES_supply] is accumulated in
    atoms. The two were never in the same unit, so [supply_bound_property]
    asserted something 10^6 times stricter than the protocol rule and would have been violated by
    the 22nd mint. Both now name [RES_MAX_SUPPLY] from ResSchedule.v, which is the single
    definition of the cap in atoms. Cap overshoot reject is
    [ResMintObligation.cap_overshoot_rejects_apply_block], not the quarantined
    [legacy_account_model/ConnectBlockScaffold] skip. *)
Definition RES_CAP : Z := RES_MAX_SUPPLY.

(** Runtime cap-fit, in the same units as [State.RES_supply] (atoms).

    Dual insurance with [max_issuance_within_cap] (finite sum) and the
    replay reject [apply_block = None] on overshoot. Equal-to-cap is
    admitted; one atom over is not. Honesty: not a C++ refinement. *)
Definition mint_fits_cap (supply amount : Z) : bool :=
  res_supply_plus_mint_within_cap supply amount.

Lemma mint_fits_cap_iff_le :
  forall supply amount : Z,
    mint_fits_cap supply amount = true <-> (supply + amount <= RES_CAP)%Z.
Proof.
  intros supply amount. unfold mint_fits_cap, res_supply_plus_mint_within_cap, RES_CAP.
  split; apply Z.leb_le.
Qed.

Lemma mint_exact_cap_fits :
  forall supply amount : Z,
    (supply + amount = RES_CAP)%Z ->
    mint_fits_cap supply amount = true.
Proof.
  intros supply amount Heq. unfold mint_fits_cap, RES_CAP.
  apply res_exact_cap_admitted. exact Heq.
Qed.

Lemma mint_overshoot_rejected :
  forall supply amount : Z,
    (RES_CAP < supply + amount)%Z ->
    mint_fits_cap supply amount = false.
Proof.
  intros supply amount Hlt. unfold mint_fits_cap, RES_CAP.
  apply res_overshoot_rejected. exact Hlt.
Qed.

Lemma mint_one_atom_over_rejected :
  forall supply amount : Z,
    (supply + amount = RES_CAP + 1)%Z ->
    mint_fits_cap supply amount = false.
Proof.
  intros supply amount Heq. unfold mint_fits_cap, RES_CAP.
  apply res_overshoot_one_atom_rejected. exact Heq.
Qed.

(* Supply bound property *)
Definition supply_bound_property (s : State) : Prop :=
  Z.le s.(RES_supply) RES_CAP.

Lemma supply_at_cap_satisfies_bound :
  forall s : State, s.(RES_supply) = RES_CAP -> supply_bound_property s.
Proof.
  intros s Heq. unfold supply_bound_property. rewrite Heq. apply Z.le_refl.
Qed.

Lemma supply_one_atom_over_violates_bound :
  forall s : State, s.(RES_supply) = (RES_CAP + 1)%Z -> ~ supply_bound_property s.
Proof.
  intros s Heq Hbound.
  unfold supply_bound_property in Hbound. rewrite Heq in Hbound.
  exact (Z.nle_succ_diag_l RES_CAP Hbound).
Qed.

(** Same atom-level pin as [ResSchedule.res_cap_atom_boundaries], in the
    [Properties] namespace used by the PHASE-3 scaffold. *)
Theorem res_cap_atom_boundaries :
  (forall supply amount : Z,
     (supply + amount = RES_CAP)%Z ->
     mint_fits_cap supply amount = true) /\
  (forall supply amount : Z,
     (supply + amount = RES_CAP + 1)%Z ->
     mint_fits_cap supply amount = false).
Proof.
  split.
  - exact mint_exact_cap_fits.
  - exact mint_one_atom_over_rejected.
Qed.

(* Non-negative supply *)
Definition non_negative_supply (s : State) : Prop :=
  Z.ge s.(ORU_supply) 0 /\ Z.ge s.(RES_supply) 0.

(* Height monotonicity *)
Definition height_monotonic (b : Block) (s : State) : Prop :=
  Z.eq b.(height) (s.(current_height) + 1).

(* Valid block property *)
Definition valid_block (b : Block) (s : State) : Prop :=
  Z.gt b.(height) s.(current_height).
